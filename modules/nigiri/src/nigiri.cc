#include "motis/nigiri/nigiri.h"

#include "boost/filesystem.hpp"

#include "utl/enumerate.h"
#include "utl/helpers/algorithm.h"
#include "utl/verify.h"

#include "nigiri/loader/dir.h"
#include "nigiri/loader/hrd/load_timetable.h"

#include "motis/core/common/logging.h"
#include "motis/module/event_collector.h"
#include "motis/nigiri/routing.h"

namespace fs = std::filesystem;
namespace mm = motis::module;
namespace n = ::nigiri;
namespace fbs = flatbuffers;

namespace motis::nigiri {

struct nigiri::impl {
  std::shared_ptr<cista::wrapped<n::timetable>> tt_;
  std::vector<std::string> tags_;
};

nigiri::nigiri() : module("Next Generation Routing", "nigiri") {
  param(no_cache_, "no_cache", "disable timetable caching");
}

nigiri::~nigiri() = default;

void nigiri::init(motis::module::registry& reg) {
  reg.register_op("/nigiri",
                  [&](mm::msg_ptr const& msg) {
                    return route(impl_->tags_, **impl_->tt_, msg);
                  },
                  {});
}

void nigiri::import(motis::module::import_dispatcher& reg) {
  std::make_shared<mm::event_collector>(
      get_data_directory().generic_string(), "nigiri", reg,
      [this](mm::event_collector::dependencies_map_t const& dependencies,
             mm::event_collector::publish_fn_t const&) {
        auto const& msg = dependencies.at("SCHEDULE");

        impl_ = std::make_unique<impl>();

        using import::FileEvent;
        auto h = std::uint64_t{};
        auto datasets = std::vector<std::tuple<
            n::source_idx_t, decltype(n::loader::hrd::configs)::const_iterator,
            std::unique_ptr<n::loader::dir>>>{};
        for (auto const [i, p] :
             utl::enumerate(*motis_content(FileEvent, msg)->paths())) {
          if (p->tag()->str() != "schedule") {
            continue;
          }
          auto const path = fs::path{p->path()->str()};
          auto d = n::loader::make_dir(path);
          auto const c = utl::find_if(n::loader::hrd::configs, [&](auto&& c) {
            return n::loader::hrd::applicable(c, *d);
          });
          utl::verify(c != end(n::loader::hrd::configs),
                      "no loader applicable to {}", path);
          h = n::loader::hrd::hash(*c, *d, h);

          datasets.emplace_back(n::source_idx_t{i}, c, std::move(d));

          auto const tag = p->options()->str();
          impl_->tags_.emplace_back(tag + (tag.empty() ? "" : "-"));
        }

        auto const dir = get_data_directory() / "nigiri";
        auto const dump_file_path = dir / fmt::to_string(h);
        if (!no_cache_ && std::filesystem::is_regular_file(dump_file_path)) {
          impl_->tt_ = std::make_shared<cista::wrapped<n::timetable>>(
              n::timetable::read(cista::memory_holder{cista::buf<cista::mmap>{
                  cista::mmap{dump_file_path.string().c_str(),
                              cista::mmap::protection::READ}}}));
        } else {
          impl_->tt_ = std::make_shared<cista::wrapped<n::timetable>>(
              cista::raw::make_unique<n::timetable>());
          for (auto const& [s, c, d] : datasets) {
            LOG(logging::info) << "loading nigiri timetable with configuration "
                               << c->version_.view();
            n::loader::hrd::load_timetable(s, *c, *d, **impl_->tt_);
          }

          if (!no_cache_) {
            std::filesystem::create_directories(dir);
            (*impl_->tt_)->write(dump_file_path);
          }
        }

        LOG(logging::info) << "nigiri timetable: stations="
                           << (*impl_->tt_)->locations_.names_.size() << "\n";

        import_successful_ = true;
      })
      ->require("SCHEDULE", [](mm::msg_ptr const& msg) {
        if (msg->get()->content_type() != MsgContent_FileEvent) {
          return false;
        }
        using import::FileEvent;
        return utl::all_of(
            *motis_content(FileEvent, msg)->paths(),
            [](import::ImportPath const* p) {
              if (p->tag()->str() != "schedule") {
                return true;
              }
              auto const d = n::loader::make_dir(fs::path{p->path()->str()});
              return utl::any_of(n::loader::hrd::configs, [&](auto&& c) {
                return n::loader::hrd::applicable(c, *d);
              });
            });
      });
}

}  // namespace motis::nigiri
