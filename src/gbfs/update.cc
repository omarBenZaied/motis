#include "motis/gbfs/update.h"

#include <cassert>
#include <chrono>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string_view>

#include <iostream>

#include "boost/asio/co_spawn.hpp"
#include "boost/asio/detached.hpp"
#include "boost/asio/experimental/awaitable_operators.hpp"
#include "boost/asio/experimental/parallel_group.hpp"
#include "boost/asio/redirect_error.hpp"
#include "boost/asio/steady_timer.hpp"

#include "boost/json.hpp"

#include "cista/hash.h"

#include "fmt/format.h"

#include "utl/helpers/algorithm.h"
#include "utl/timer.h"
#include "utl/to_vec.h"

#include "motis/config.h"
#include "motis/data.h"
#include "motis/gbfs/data.h"
#include "motis/http_client.h"
#include "motis/http_req.h"

#include "motis/gbfs/compression.h"
#include "motis/gbfs/diff.h"
#include "motis/gbfs/osr_mapping.h"
#include "motis/gbfs/parser.h"
#include "motis/gbfs/partition.h"
#include "motis/gbfs/routing_data.h"

namespace asio = boost::asio;
using asio::awaitable;
using namespace asio::experimental::awaitable_operators;

namespace json = boost::json;

namespace motis::gbfs {

struct gbfs_file {
  json::value json_;
  cista::hash_t hash_{};
  std::chrono::system_clock::time_point next_refresh_;
};

std::string read_file(std::filesystem::path const& path) {
  auto is = std::ifstream{path};
  auto buf = std::stringstream{};
  buf << is.rdbuf();
  return buf.str();
}

bool needs_refresh(file_info const& fi) {
  return fi.needs_update(std::chrono::system_clock::now());
}

// try to hash only the value of the "data" key to ignore fields like
// "last_updated"
cista::hash_t hash_gbfs_data(std::string_view const json) {
  auto const pos = json.find("\"data\"");
  if (pos == std::string_view::npos) {
    return cista::hash(json);
  }

  auto i = pos + 6;
  auto const skip_whitespace = [&]() {
    while (i < json.size() && (json[i] == ' ' || json[i] == '\n' ||
                               json[i] == '\r' || json[i] == '\t')) {
      ++i;
    }
  };
  skip_whitespace();

  if (i >= json.size() || json[i++] != ':') {
    return cista::hash(json);
  }

  skip_whitespace();

  if (i >= json.size() || json[i] != '{') {
    return cista::hash(json);
  }

  auto const start = i;
  auto depth = 1;
  auto in_string = false;

  while (++i < json.size()) {
    if (in_string) {
      if (json[i] == '"' && json[i - 1] != '\\') {
        in_string = false;
      }
      continue;
    }

    switch (json[i]) {
      case '"': in_string = true; break;
      case '{': ++depth; break;
      case '}':
        if (--depth == 0) {
          return cista::hash(json.substr(start, i - start + 1));
        }
    }
  }

  return cista::hash(json);
}

std::chrono::system_clock::time_point get_expiry(
    boost::json::object const& root,
    std::chrono::seconds const def = std::chrono::seconds{0}) {
  auto const now = std::chrono::system_clock::now();
  if (root.contains("data")) {
    auto const& data = root.at("data").as_object();
    if (data.contains("ttl")) {
      return now + std::chrono::seconds{data.at("ttl").to_number<int>()};
    }
  }
  return now + def;
}

struct gbfs_update {
  gbfs_update(config::gbfs const& c,
              osr::ways const& w,
              osr::lookup const& l,
              gbfs_data* d,
              gbfs_data const* prev_d)
      : c_{c}, w_{w}, l_{l}, d_{d}, prev_d_{prev_d} {
    client_.timeout_ = std::chrono::seconds{c.http_timeout_};
  }

  awaitable<void> run() {
    auto executor = co_await asio::this_coro::executor;
    if (prev_d_ == nullptr) {
      // this is first time gbfs_update is run: initialize feeds from config
      d_->aggregated_feeds_ =
          std::make_shared<std::vector<std::unique_ptr<aggregated_feed>>>();
      d_->standalone_feeds_ =
          std::make_shared<std::vector<std::unique_ptr<provider_feed>>>();

      auto const no_hdr = headers_t{};
      auto awaitables = utl::to_vec(c_.feeds_, [&](auto const& f) {
        auto const& id = f.first;
        auto const& feed = f.second;
        auto const dir =
            feed.url_.starts_with("http:") || feed.url_.starts_with("https:")
                ? std::nullopt
                : std::optional<std::filesystem::path>{feed.url_};

        return boost::asio::co_spawn(
            executor,
            [this, id, feed, dir, &no_hdr]() -> awaitable<void> {
              co_await init_feed(id, feed.url_, feed.headers_.value_or(no_hdr),
                                 dir);
            },
            asio::deferred);
      });

      co_await asio::experimental::make_parallel_group(awaitables)
          .async_wait(asio::experimental::wait_for_all(), asio::use_awaitable);
    } else {
      // update run: copy over data from previous state and update feeds
      // where necessary
      d_->aggregated_feeds_ = prev_d_->aggregated_feeds_;
      d_->standalone_feeds_ = prev_d_->standalone_feeds_;
      // the set of providers can change if aggregated feeds are used + change.
      // gbfs_provider_idx_t for existing providers is stable, if a provider is
      // removed its entry is set to a nullptr. new providers may be added.
      d_->providers_.resize(prev_d_->providers_.size());
      d_->provider_by_id_ = prev_d_->provider_by_id_;
      d_->provider_rtree_ = prev_d_->provider_rtree_;
      d_->cache_ = prev_d_->cache_;

      if (!d_->aggregated_feeds_->empty()) {
        co_await asio::experimental::make_parallel_group(
            utl::to_vec(*d_->aggregated_feeds_,
                        [&](auto const& af) {
                          return boost::asio::co_spawn(
                              executor,
                              [this, af = af.get()]() -> awaitable<void> {
                                co_await update_aggregated_feed(*af);
                              },
                              asio::deferred);
                        }))
            .async_wait(asio::experimental::wait_for_all(),
                        asio::use_awaitable);
      }

      if (!d_->standalone_feeds_->empty()) {
        co_await asio::experimental::make_parallel_group(
            utl::to_vec(*d_->standalone_feeds_,
                        [&](auto const& pf) {
                          return boost::asio::co_spawn(
                              executor,
                              [this, pf = pf.get()]() -> awaitable<void> {
                                co_await update_provider_feed(*pf);
                              },
                              asio::deferred);
                        }))
            .async_wait(asio::experimental::wait_for_all(),
                        asio::use_awaitable);
      }
    }
  }

  awaitable<void> init_feed(std::string const& id,
                            std::string const& url,
                            headers_t const& headers,
                            std::optional<std::filesystem::path> const& dir) {
    // initialization of a (standalone or aggregated) feed from the config
    try {
      auto discovery = co_await fetch_file("gbfs", url, headers, dir);
      auto const& root = discovery.json_.as_object();
      if ((root.contains("data") &&
           root.at("data").as_object().contains("datasets")) ||
          root.contains("systems")) {
        // file is not an individual feed, but a manifest.json / Lamassu file
        co_return co_await init_aggregated_feed(id, url, headers, root);
      }

      auto saf =
          d_->standalone_feeds_
              ->emplace_back(std::make_unique<provider_feed>(
                  provider_feed{.id_ = id,
                                .url_ = url,
                                .headers_ = headers,
                                .dir_ = dir,
                                .default_restrictions_ =
                                    lookup_default_restrictions("", id)}))
              .get();

      co_return co_await update_provider_feed(*saf, std::move(discovery));
    } catch (std::exception const& ex) {
      std::cerr << "[GBFS] error initializing feed " << id << " (" << url
                << "): " << ex.what() << "\n";
    }
  }

  awaitable<void> update_provider_feed(
      provider_feed const& pf,
      std::optional<gbfs_file> discovery = std::nullopt) {
    auto& provider = add_provider(pf);

    // check if exists in old data - if so, reuse existing file infos
    gbfs_provider const* prev_provider = nullptr;
    if (prev_d_ != nullptr) {
      if (auto const it = prev_d_->provider_by_id_.find(pf.id_);
          it != end(prev_d_->provider_by_id_)) {
        prev_provider = prev_d_->providers_[it->second].get();
        provider.file_infos_ = prev_provider->file_infos_;
      }
    }
    if (!provider.file_infos_) {
      provider.file_infos_ = std::make_shared<provider_file_infos>();
    }

    co_return co_await process_provider_feed(pf, provider, prev_provider,
                                             std::move(discovery));
  }

  gbfs_provider& add_provider(provider_feed const& pf) {
    auto const init_provider = [&](gbfs_provider& provider,
                                   gbfs_provider_idx_t const idx) {
      provider.id_ = pf.id_;
      provider.idx_ = idx;
      provider.default_restrictions_ = pf.default_restrictions_;
    };

    if (auto it = d_->provider_by_id_.find(pf.id_);
        it != end(d_->provider_by_id_)) {
      // existing provider, keep idx
      auto const idx = it->second;
      assert(d_->providers_.at(idx) == nullptr);
      d_->providers_[idx] = std::make_unique<gbfs_provider>();
      auto& provider = *d_->providers_[idx].get();
      init_provider(provider, idx);
      return provider;
    } else {
      // new provider
      auto const idx = gbfs_provider_idx_t{d_->providers_.size()};
      auto& provider =
          *d_->providers_.emplace_back(std::make_unique<gbfs_provider>()).get();
      d_->provider_by_id_[pf.id_] = idx;
      init_provider(provider, idx);
      return provider;
    }
  }

  awaitable<void> process_provider_feed(
      provider_feed const& pf,
      gbfs_provider& provider,
      gbfs_provider const* prev_provider,
      std::optional<gbfs_file> discovery = std::nullopt) {
    auto& file_infos = provider.file_infos_;

    if (!discovery && !provider.file_infos_->needs_update()) {
      co_return;
    }

    if (!discovery && needs_refresh(provider.file_infos_->urls_fi_)) {
      discovery = co_await fetch_file("gbfs", pf.url_, pf.headers_, pf.dir_);
    }
    if (discovery) {
      file_infos->urls_ = parse_discovery(discovery->json_);
      file_infos->urls_fi_.expiry_ = discovery->next_refresh_;
      file_infos->urls_fi_.hash_ = discovery->hash_;
    }

    auto const update = [&](std::string_view const name, file_info& fi,
                            auto const& fn,
                            bool const force = false) -> awaitable<bool> {
      if (force || (file_infos->urls_.contains(name) && needs_refresh(fi))) {
        auto file = co_await fetch_file(name, file_infos->urls_.at(name),
                                        pf.headers_, pf.dir_);
        auto const hash_changed = file.hash_ != fi.hash_;
        auto j_root = file.json_.as_object();
        fi.expiry_ = file.next_refresh_;
        fi.hash_ = file.hash_;
        fn(provider, file.json_);
        co_return hash_changed;
      }
      co_return false;
    };

    auto const sys_info_updated = co_await update(
        "system_information", file_infos->system_information_fi_,
        load_system_information);
    if (!sys_info_updated && prev_provider != nullptr) {
      provider.sys_info_ = prev_provider->sys_info_;
    }

    auto const vehicle_types_updated = co_await update(
        "vehicle_types", file_infos->vehicle_types_fi_, load_vehicle_types);
    if (!vehicle_types_updated && prev_provider != nullptr) {
      provider.vehicle_types_ = prev_provider->vehicle_types_;
    }

    auto const stations_updated = co_await update(
        "station_information", file_infos->station_information_fi_,
        load_station_information);
    if (!stations_updated && prev_provider != nullptr) {
      provider.stations_ = prev_provider->stations_;
    }

    auto const station_status_updated =
        co_await update("station_status", file_infos->station_status_fi_,
                        load_station_status, stations_updated);

    auto const vehicle_status_updated =
        co_await update("vehicle_status", file_infos->vehicle_status_fi_,
                        load_vehicle_status)  // 3.x
        || co_await update("free_bike_status", file_infos->vehicle_status_fi_,
                           load_vehicle_status);  // 1.x / 2.x
    if (!vehicle_status_updated && prev_provider != nullptr) {
      provider.vehicle_status_ = prev_provider->vehicle_status_;
    }

    auto const geofencing_updated =
        co_await update("geofencing_zones", file_infos->geofencing_zones_fi_,
                        load_geofencing_zones);
    if (!geofencing_updated && prev_provider != nullptr) {
      provider.geofencing_zones_ = prev_provider->geofencing_zones_;
    }

    if (prev_provider != nullptr) {
      provider.has_vehicles_to_rent_ = prev_provider->has_vehicles_to_rent_;
    }

    auto const data_changed = vehicle_types_updated || stations_updated ||
                              station_status_updated ||
                              vehicle_status_updated || geofencing_updated;

    if (data_changed) {
      partition_provider(provider);
      provider.has_vehicles_to_rent_ = utl::any_of(
          provider.segments_,
          [](auto const& seg) { return seg.has_vehicles_to_rent_; });

      update_rtree(provider, prev_provider);

      d_->cache_.update_if_exists(provider.idx_, [&](auto const& /*old*/) {
        return compute_provider_routing_data(w_, l_, provider);
      });
    } else if (prev_provider != nullptr) {
      // data not changed, copy previously computed segments
      provider.segments_ = prev_provider->segments_;
      provider.has_vehicles_to_rent_ = prev_provider->has_vehicles_to_rent_;
    }
  }

  void partition_provider(gbfs_provider& provider) {
    if (provider.vehicle_types_.empty()) {
      // providers without vehicle types only need one segment
      auto& seg = provider.segments_.emplace_back();
      seg.idx_ = gbfs_segment_idx_t{0};
      seg.has_vehicles_to_rent_ =
          utl::any_of(provider.stations_,
                      [](auto const& st) {
                        return st.second.status_.is_renting_ &&
                               st.second.status_.num_vehicles_available_ > 0;
                      }) ||
          utl::any_of(provider.vehicle_status_, [](auto const& vs) {
            return !vs.is_disabled_ && !vs.is_reserved_;
          });
    } else {
      auto part = partition{provider.vehicle_types_.size()};

      auto vt_id_to_idx = hash_map<std::string, std::size_t>{};
      auto vt_idx_to_id = std::vector<std::string>{};
      for (auto const& [id, vt] : provider.vehicle_types_) {
        auto const idx = vt_id_to_idx.size();
        vt_id_to_idx[id] = idx;
        vt_idx_to_id.emplace_back(id);
      }

      // refine by form factor
      auto by_form_factor =
          hash_map<vehicle_form_factor, std::vector<std::size_t>>{};
      for (auto const& [id, vt] : provider.vehicle_types_) {
        by_form_factor[vt.form_factor_].push_back(vt_id_to_idx[id]);
      }
      for (auto const& [_, vt_indices] : by_form_factor) {
        part.refine(vt_indices);
      }

      // refine by return stations
      // TODO: only do this if the station is not in a zone where vehicles
      //   can be returned anywhere
      auto vts = std::vector<std::size_t>{};
      for (auto const& [id, st] : provider.stations_) {
        if (!st.status_.vehicle_docks_available_.empty()) {
          vts.clear();
          for (auto const& [vt, num] : st.status_.vehicle_docks_available_) {
            if (auto const it = vt_id_to_idx.find(vt);
                it != end(vt_id_to_idx)) {
              vts.push_back(it->second);
            }
            part.refine(vts);
          }
        }
      }

      // refine by geofencing zones
      for (auto const& z : provider.geofencing_zones_.zones_) {
        for (auto const& r : z.rules_) {
          vts.clear();
          for (auto const& id : r.vehicle_type_ids_) {
            if (auto const it = vt_id_to_idx.find(id);
                it != end(vt_id_to_idx)) {
              vts.push_back(it->second);
            }
          }
          part.refine(vts);
        }
      }

      for (auto const& set : part.get_sets()) {
        auto const seg_idx = gbfs_segment_idx_t{provider.segments_.size()};
        auto& seg = provider.segments_.emplace_back();
        seg.idx_ = seg_idx;
        seg.vehicle_types_ =
            utl::to_vec(set, [&](auto const idx) { return vt_idx_to_id[idx]; });
        seg.form_factor_ =
            provider.vehicle_types_.at(seg.vehicle_types_.front()).form_factor_;
        seg.has_vehicles_to_rent_ =
            utl::any_of(provider.stations_,
                        [&](auto const& st) {
                          return st.second.status_.is_renting_ &&
                                 st.second.status_.num_vehicles_available_ > 0;
                        }) ||
            utl::any_of(provider.vehicle_status_, [&](auto const& vs) {
              return !vs.is_disabled_ && !vs.is_reserved_ &&
                     seg.includes_vehicle_type(vs.vehicle_type_id_);
            });
      }
    }
  }

  void update_rtree(gbfs_provider const& provider,
                    gbfs_provider const* prev_provider) {
    auto added_stations = 0U;
    auto added_vehicles = 0U;
    auto removed_stations = 0U;
    auto removed_vehicles = 0U;
    auto moved_stations = 0U;
    auto moved_vehicles = 0U;

    if (prev_provider != nullptr) {
      diff(
          prev_provider->stations_, provider.stations_,
          [&](auto const& s) {
            d_->provider_rtree_.remove(s.second.info_.pos_, provider.idx_);
            ++removed_stations;
          },
          [&](auto const& s) {
            d_->provider_rtree_.add(s.second.info_.pos_, provider.idx_);
            ++added_stations;
          },
          [&](auto const& old_s, auto const& new_s) {
            if (old_s.second.info_.pos_ != new_s.second.info_.pos_) {
              d_->provider_rtree_.remove(old_s.second.info_.pos_,
                                         provider.idx_);
              d_->provider_rtree_.add(new_s.second.info_.pos_, provider.idx_);
              ++moved_stations;
            }
          });
      diff(
          prev_provider->vehicle_status_, provider.vehicle_status_,
          [&](auto const& v) {
            d_->provider_rtree_.remove(v.pos_, provider.idx_);
            ++removed_vehicles;
          },
          [&](auto const& v) {
            d_->provider_rtree_.add(v.pos_, provider.idx_);
            ++added_vehicles;
          },
          [&](auto const& old_v, auto const& new_v) {
            if (old_v.pos_ != new_v.pos_) {
              d_->provider_rtree_.remove(old_v.pos_, provider.idx_);
              d_->provider_rtree_.add(new_v.pos_, provider.idx_);
              ++moved_vehicles;
            }
          });
    } else {
      for (auto const& station : provider.stations_) {
        d_->provider_rtree_.add(station.second.info_.pos_, provider.idx_);
        ++added_stations;
      }
      for (auto const& vehicle : provider.vehicle_status_) {
        if (vehicle.station_id_.empty()) {
          d_->provider_rtree_.add(vehicle.pos_, provider.idx_);
          ++added_vehicles;
        }
      }
    }
  }

  awaitable<void> init_aggregated_feed(std::string const& prefix,
                                       std::string const& url,
                                       headers_t const& headers,
                                       boost::json::object const& root) {
    auto af =
        d_->aggregated_feeds_
            ->emplace_back(std::make_unique<aggregated_feed>(aggregated_feed{
                .id_ = prefix,
                .url_ = url,
                .headers_ = headers,
                .expiry_ = get_expiry(root, std::chrono::hours{1})}))
            .get();

    co_return co_await process_aggregated_feed(*af, root);
  }

  awaitable<void> update_aggregated_feed(aggregated_feed& af) {
    if (af.needs_update()) {
      auto const file = co_await fetch_file("manifest", af.url_, af.headers_);
      co_await process_aggregated_feed(af, file.json_.as_object());
    }
  }

  awaitable<void> process_aggregated_feed(aggregated_feed& af,
                                          boost::json::object const& root) {
    auto feeds = std::vector<provider_feed>{};
    if (root.contains("data") &&
        root.at("data").as_object().contains("datasets")) {
      // GBFS 3.x manifest.json
      for (auto const& dataset : root.at("data").at("datasets").as_array()) {
        auto const system_id =
            static_cast<std::string>(dataset.at("system_id").as_string());
        auto const combined_id = fmt::format("{}:{}", af.id_, system_id);

        auto const& versions = dataset.at("versions").as_array();
        if (versions.empty()) {
          continue;
        }
        // versions array must be sorted by increasing version number
        auto const& latest_version = versions.back().as_object();
        feeds.emplace_back(provider_feed{
            .id_ = combined_id,
            .url_ =
                static_cast<std::string>(latest_version.at("url").as_string()),
            .default_restrictions_ =
                lookup_default_restrictions(af.id_, combined_id)});
      }
    } else if (root.contains("systems")) {
      // Lamassu 2.3 format
      for (auto const& system : root.at("systems").as_array()) {
        auto const system_id =
            static_cast<std::string>(system.at("id").as_string());
        auto const combined_id = fmt::format("{}:{}", af.id_, system_id);
        feeds.emplace_back(provider_feed{
            .id_ = combined_id,
            .url_ = static_cast<std::string>(system.at("url").as_string()),
            .default_restrictions_ =
                lookup_default_restrictions(af.id_, combined_id)});
      }
    }

    af.feeds_ = std::move(feeds);

    auto executor = co_await asio::this_coro::executor;
    co_await asio::experimental::make_parallel_group(
        utl::to_vec(af.feeds_,
                    [&](auto const& pf) {
                      return boost::asio::co_spawn(
                          executor,
                          [this, pf = &pf]() -> awaitable<void> {
                            co_await update_provider_feed(*pf);
                          },
                          asio::deferred);
                    }))
        .async_wait(asio::experimental::wait_for_all(), asio::use_awaitable);
  }

  awaitable<gbfs_file> fetch_file(
      std::string_view const name,
      std::string_view const url,
      headers_t const& headers,
      std::optional<std::filesystem::path> const& dir = std::nullopt) {
    auto content = std::string{};
    if (dir.has_value()) {
      content = read_file(*dir / fmt::format("{}.json", name));
    } else {
      auto const res = co_await client_.get(boost::urls::url{url}, headers);
      content = get_http_body(res);
    }
    auto j = json::parse(content);
    auto j_root = j.as_object();
    auto const next_refresh =
        std::chrono::system_clock::now() +
        std::chrono::seconds{
            j_root.contains("ttl") ? j_root.at("ttl").to_number<int>() : 0};
    co_return gbfs_file{.json_ = std::move(j),
                        .hash_ = hash_gbfs_data(content),
                        .next_refresh_ = next_refresh};
  }

  geofencing_restrictions lookup_default_restrictions(std::string const& prefix,
                                                      std::string const& id) {
    auto const convert = [&](config::gbfs::restrictions const& r) {
      return geofencing_restrictions{
          .ride_start_allowed_ = r.ride_start_allowed_,
          .ride_end_allowed_ = r.ride_end_allowed_,
          .ride_through_allowed_ = r.ride_through_allowed_};
    };

    if (auto const it = c_.default_restrictions_.find(id);
        it != end(c_.default_restrictions_)) {
      return convert(it->second);
    } else if (auto const prefix_it = c_.default_restrictions_.find(prefix);
               prefix_it != end(c_.default_restrictions_)) {
      return convert(prefix_it->second);
    } else {
      return {};
    }
  }

  config::gbfs const& c_;
  osr::ways const& w_;
  osr::lookup const& l_;

  gbfs_data* d_;
  gbfs_data const* prev_d_;

  http_client client_;
};

awaitable<void> update(config const& c,
                       osr::ways const& w,
                       osr::lookup const& l,
                       std::shared_ptr<gbfs_data>& data_ptr) {
  auto const t = utl::scoped_timer{"gbfs::update"};

  if (!c.gbfs_.has_value()) {
    co_return;
  }

  auto const prev_d = data_ptr;
  auto const d = std::make_shared<gbfs_data>(c.gbfs_->cache_size_);

  auto update = gbfs_update{*c.gbfs_, w, l, d.get(), prev_d.get()};
  co_await update.run();
  data_ptr = d;
}

void run_gbfs_update(boost::asio::io_context& ioc,
                     config const& c,
                     osr::ways const& w,
                     osr::lookup const& l,
                     std::shared_ptr<gbfs_data>& data_ptr) {
  boost::asio::co_spawn(
      ioc,
      [&]() -> awaitable<void> {
        auto executor = co_await asio::this_coro::executor;
        auto timer = asio::steady_timer{executor};
        auto ec = boost::system::error_code{};

        while (true) {
          // Remember when we started so we can schedule the next update.
          auto const start = std::chrono::steady_clock::now();

          co_await update(c, w, l, data_ptr);

          // Schedule next update.
          timer.expires_at(start +
                           std::chrono::seconds{c.gbfs_->update_interval_});
          co_await timer.async_wait(
              asio::redirect_error(asio::use_awaitable, ec));
          if (ec == asio::error::operation_aborted) {
            co_return;
          }
        }
      },
      boost::asio::detached);
}

}  // namespace motis::gbfs
