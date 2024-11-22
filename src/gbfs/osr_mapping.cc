#include "motis/gbfs/osr_mapping.h"

#include <optional>
#include <utility>
#include <vector>

#include "tg.h"

#include "geo/box.h"

#include "osr/lookup.h"
#include "osr/routing/profiles/foot.h"
#include "osr/types.h"
#include "osr/ways.h"

#include "utl/enumerate.h"
#include "utl/helpers/algorithm.h"
#include "utl/to_vec.h"
#include "utl/zip.h"

#include "motis/constants.h"
#include "motis/types.h"

#include "motis/gbfs/compression.h"
#include "motis/gbfs/data.h"
#include "motis/gbfs/geofencing.h"

namespace motis::gbfs {

struct osr_mapping {
  osr_mapping(osr::ways const& w,
              osr::lookup const& l,
              gbfs_provider const& provider)
      : w_{w}, l_{l}, provider_{provider} {
    segment_data_.resize(provider.segments_.size());
  }

  void map_geofencing_zones() {
    auto const make_loc_bitvec = [&]() {
      auto bv = osr::bitvec<osr::node_idx_t>{};
      bv.resize(static_cast<typename osr::bitvec<osr::node_idx_t>::size_type>(
          w_.n_nodes() + provider_.stations_.size() +
          provider_.vehicle_status_.size()));
      return bv;
    };

    for (auto [seg, rd] : utl::zip(provider_.segments_, segment_data_)) {
      auto done = make_loc_bitvec();
      rd.start_allowed_ = make_loc_bitvec();
      rd.end_allowed_ = make_loc_bitvec();
      rd.through_allowed_ = make_loc_bitvec();
      auto default_restrictions = provider_.default_restrictions_;

      // global rules
      for (auto const& r : provider_.geofencing_zones_.global_rules_) {
        if (!applies(r.vehicle_type_ids_, seg.vehicle_types_)) {
          continue;
        }
        default_restrictions.ride_start_allowed_ = r.ride_start_allowed_;
        default_restrictions.ride_end_allowed_ = r.ride_end_allowed_;
        default_restrictions.ride_through_allowed_ = r.ride_through_allowed_;
        default_restrictions.station_parking_ = r.station_parking_;
        break;
      }

      if (default_restrictions.ride_end_allowed_ &&
          !default_restrictions.station_parking_) {
        rd.end_allowed_.one_out();
      }
      if (default_restrictions.ride_through_allowed_) {
        rd.through_allowed_.one_out();
      }
      auto const global_station_parking =
          default_restrictions.station_parking_.value_or(false);

      auto const handle_point = [&](osr::node_idx_t const n,
                                    geo::latlng const& pos) {
        auto start_allowed = std::optional<bool>{};
        auto end_allowed = std::optional<bool>{};
        auto through_allowed = std::optional<bool>{};
        auto station_parking = global_station_parking;
        for (auto const& z : provider_.geofencing_zones_.zones_) {
          // check if pos is inside the zone multipolygon
          if (multipoly_contains_point(z.geom_.get(), pos)) {
            // vehicle_type_ids currently ignored, using first rule
            for (auto const& r : z.rules_) {
              if (!applies(r.vehicle_type_ids_, seg.vehicle_types_)) {
                continue;
              }
              start_allowed = r.ride_start_allowed_;
              end_allowed = r.ride_end_allowed_;
              through_allowed = r.ride_through_allowed_;
              if (r.station_parking_.has_value()) {
                station_parking = r.station_parking_.value();
              }
              break;
            }
            if (start_allowed.has_value()) {
              break;  // for now
            }
          }
        }
        if (end_allowed.has_value() && !station_parking) {
          rd.end_allowed_.set(n, *end_allowed);
        }
        if (through_allowed.has_value()) {
          rd.through_allowed_.set(n, *through_allowed);
        }
      };

      auto const* osr_r = w_.r_.get();
      for (auto const& z : provider_.geofencing_zones_.zones_) {
        auto const rect = tg_geom_rect(z.geom_.get());
        auto const bb = geo::box{geo::latlng{rect.min.y, rect.min.x},
                                 geo::latlng{rect.max.y, rect.max.x}};

        l_.find(bb, [&](osr::way_idx_t const way) {
          for (auto const n : osr_r->way_nodes_[way]) {
            if (done.test(n)) {
              continue;
            }
            done.set(n, true);
            handle_point(n, w_.get_node_pos(n).as_latlng());
          }
        });
      }
    }
  }

  void map_stations() {
    for (auto [seg, rd] : utl::zip(provider_.segments_, segment_data_)) {
      auto next_node_id = static_cast<osr::node_idx_t>(
          w_.n_nodes() + rd.additional_nodes_.size());
      for (auto const& [id, st] : provider_.stations_) {
        auto is_renting =
            st.status_.is_renting_ && st.status_.num_vehicles_available_ > 0;
        auto is_returning = st.status_.is_returning_;

        // if the station lists vehicles available by type, at least one of
        // the vehicle types included in the segment must be available
        if (is_renting && !st.status_.vehicle_types_available_.empty()) {
          is_renting = utl::any_of(
              st.status_.vehicle_types_available_, [&](auto const& vt) {
                return vt.second != 0 && seg.includes_vehicle_type(vt.first);
              });
        }

        // same for returning vehicles
        if (is_returning && !st.status_.vehicle_docks_available_.empty()) {
          is_returning = utl::any_of(
              st.status_.vehicle_docks_available_, [&](auto const& vt) {
                return vt.second != 0 && seg.includes_vehicle_type(vt.first);
              });
        }

        if (!is_renting && !is_returning) {
          continue;
        }

        auto const matches = l_.match<osr::foot<false>>(
            osr::location{st.info_.pos_, osr::level_t{}}, false,
            osr::direction::kForward, kMaxMatchingDistance, nullptr);
        if (matches.empty()) {
          continue;
        }

        auto const additional_node_id = next_node_id++;
        rd.additional_nodes_.emplace_back(
            additional_node{additional_node::station{id}});
        if (is_renting) {
          rd.start_allowed_.set(additional_node_id, true);
        }
        if (is_returning) {
          rd.end_allowed_.set(additional_node_id, true);
          if (st.info_.station_area_) {
            auto const* geom = st.info_.station_area_.get();
            auto const rect = tg_geom_rect(geom);
            auto const bb = geo::box{geo::latlng{rect.min.y, rect.min.x},
                                     geo::latlng{rect.max.y, rect.max.x}};
            auto const* osr_r = w_.r_.get();
            l_.find(bb, [&](osr::way_idx_t const way) {
              for (auto const n : osr_r->way_nodes_[way]) {
                if (multipoly_contains_point(geom,
                                             w_.get_node_pos(n).as_latlng())) {
                  rd.end_allowed_.set(n, true);
                }
              }
            });
          }
        }

        for (auto const& m : matches) {
          auto const handle_node = [&](osr::node_candidate const node) {
            if (node.valid() && node.dist_to_node_ <= kMaxMatchingDistance) {
              auto const edge_to_an = osr::additional_edge{
                  additional_node_id,
                  static_cast<osr::distance_t>(node.dist_to_node_)};
              auto& node_edges = rd.additional_edges_[node.node_];
              if (utl::find(node_edges, edge_to_an) == end(node_edges)) {
                node_edges.emplace_back(edge_to_an);
              }

              auto const edge_from_an = osr::additional_edge{
                  node.node_, static_cast<osr::distance_t>(node.dist_to_node_)};
              auto& an_edges = rd.additional_edges_[additional_node_id];
              if (utl::find(an_edges, edge_from_an) == end(an_edges)) {
                an_edges.emplace_back(edge_from_an);
              }
            }
          };

          handle_node(m.left_);
          handle_node(m.right_);
        }
      }
    }
  }

  void map_vehicles() {
    for (auto [seg, rd] : utl::zip(provider_.segments_, segment_data_)) {
      auto next_node_id = static_cast<osr::node_idx_t>(
          w_.n_nodes() + rd.additional_nodes_.size());
      for (auto const [vehicle_idx, vs] :
           utl::enumerate(provider_.vehicle_status_)) {
        if (vs.is_disabled_ || vs.is_reserved_ || !vs.station_id_.empty() ||
            !vs.home_station_id_.empty() ||
            !seg.includes_vehicle_type(vs.vehicle_type_id_)) {
          continue;
        }

        auto const restrictions = provider_.geofencing_zones_.get_restrictions(
            vs.pos_, vs.vehicle_type_id_, geofencing_restrictions{});
        if (!restrictions.ride_start_allowed_) {
          continue;
        }

        auto const matches = l_.match<osr::foot<false>>(
            osr::location{vs.pos_, osr::level_t{}}, false,
            osr::direction::kForward, kMaxMatchingDistance, nullptr);
        if (matches.empty()) {
          continue;
        }

        auto const additional_node_id = next_node_id++;
        rd.additional_nodes_.emplace_back(
            additional_node{additional_node::vehicle{vehicle_idx}});
        rd.start_allowed_.set(additional_node_id, true);

        auto const& add_additional_edges = [&](osr::node_candidate const& nc) {
          auto const edge_to_an = osr::additional_edge{
              additional_node_id,
              static_cast<osr::distance_t>(nc.dist_to_node_)};
          auto& node_edges = rd.additional_edges_[nc.node_];
          if (utl::find(node_edges, edge_to_an) == end(node_edges)) {
            node_edges.emplace_back(edge_to_an);
          }

          auto const edge_from_an = osr::additional_edge{
              nc.node_, static_cast<osr::distance_t>(nc.dist_to_node_)};
          auto& an_edges = rd.additional_edges_[additional_node_id];
          if (utl::find(an_edges, edge_from_an) == end(an_edges)) {
            an_edges.emplace_back(edge_from_an);
          }
        };

        for (auto const& m : matches) {
          if (m.left_.valid() &&
              m.left_.dist_to_node_ <= kMaxMatchingDistance) {
            add_additional_edges(m.left_);
          }
          if (m.right_.valid() &&
              m.right_.dist_to_node_ <= kMaxMatchingDistance) {
            add_additional_edges(m.right_);
          }
        }
      }
    }
  }

  osr::ways const& w_;
  osr::lookup const& l_;
  gbfs_provider const& provider_;

  std::vector<routing_data> segment_data_;
};

void map_data(osr::ways const& w,
              osr::lookup const& l,
              gbfs_provider const& provider,
              provider_routing_data& prd) {
  auto mapping = osr_mapping{w, l, provider};
  mapping.map_geofencing_zones();
  mapping.map_stations();
  mapping.map_vehicles();

  prd.segments_ = utl::to_vec(mapping.segment_data_, [&](auto& rd) {
    return compressed_routing_data{
        .additional_nodes_ = std::move(rd.additional_nodes_),
        .additional_edges_ = std::move(rd.additional_edges_),
        .start_allowed_ = compress_bitvec(rd.start_allowed_),
        .end_allowed_ = compress_bitvec(rd.end_allowed_),
        .through_allowed_ = compress_bitvec(rd.through_allowed_)};
  });
}

}  // namespace motis::gbfs
