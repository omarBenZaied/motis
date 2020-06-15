#include "motis/rsl/update_load.h"

#include "utl/erase.h"
#include "utl/verify.h"

#include "motis/rsl/graph_access.h"

namespace motis::rsl {

void update_load(passenger_group* pg, reachability_info const& reachability,
                 passenger_localization const& localization, graph const& g) {
  auto disabled_edges = pg->edges_;
  pg->edges_.clear();

  if (reachability.ok_) {
    for (auto const& rt : reachability.reachable_trips_) {
      utl::verify(rt.valid_exit(), "update_load: invalid exit");
      for (auto i = rt.enter_edge_idx_; i <= rt.exit_edge_idx_; ++i) {
        auto e = rt.td_->edges_[i];
        utl::erase(disabled_edges, e);
        pg->edges_.emplace_back(e);
        add_passenger_group_to_edge(e, pg);
      }
    }
  } else {
    for (auto const& rt : reachability.reachable_trips_) {
      auto const exit_idx =
          rt.valid_exit() ? rt.exit_edge_idx_ : rt.td_->edges_.size() - 1;
      for (auto i = rt.enter_edge_idx_; i <= exit_idx; ++i) {
        auto e = rt.td_->edges_[i];
        if (e->from(g)->time_ > localization.arrival_time_) {
          break;
        }
        utl::erase(disabled_edges, e);
        pg->edges_.emplace_back(e);
        add_passenger_group_to_edge(e, pg);
        auto const to = e->to(g);
        if (to->station_ == localization.at_station_->index_ &&
            to->time_ == localization.arrival_time_) {
          break;
        }
      }
    }
  }

  for (auto e : disabled_edges) {
    remove_passenger_group_from_edge(e, pg);
  }
}

}  // namespace motis::rsl
