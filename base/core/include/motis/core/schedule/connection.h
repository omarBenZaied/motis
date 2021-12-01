#pragma once

#include <cinttypes>
#include <tuple>

#include "utl/pipes.h"

#include "motis/string.h"
#include "motis/vector.h"

#include "cista/reflection/comparable.h"

#include "motis/core/common/hash_helper.h"
#include "motis/core/schedule/attribute.h"
#include "motis/core/schedule/bitfield.h"
#include "motis/core/schedule/event_type.h"
#include "motis/core/schedule/provider.h"
#include "motis/core/schedule/time.h"
#include "motis/core/schedule/trip_idx.h"

namespace motis {

constexpr auto kMaxValidTrainNr = 99999;

using service_class_t = uint8_t;

enum class service_class : service_class_t {
  AIR = 0,
  ICE = 1,
  IC = 2,
  COACH = 3,
  N = 4,
  RE = 5,
  RB = 6,
  S = 7,
  U = 8,
  STR = 9,
  BUS = 10,
  SHIP = 11,
  OTHER = 12,
  NUM_CLASSES
};

inline service_class& operator++(service_class& c) {
  return c = static_cast<service_class>(static_cast<service_class_t>(c) + 1);
}

struct connection_info {
  CISTA_COMPARABLE();

  auto attributes(day_idx_t const day) const {
    return utl::all(attributes_)  //
           | utl::remove_if(
                 [day](auto&& e) { return !e.traffic_days_->test(day); })  //
           | utl::transform([&](auto&& e) { return e.attr_; })  //
           | utl::iterable();
  }

  mcd::vector<traffic_day_attribute> attributes_;
  mcd::string line_identifier_;
  ptr<mcd::string const> dir_{nullptr};
  ptr<provider const> provider_{nullptr};
  uint32_t category_{0U};
  uint32_t train_nr_{0U};
  uint32_t original_train_nr_{0U};
  ptr<connection_info const> merged_with_{nullptr};
};

struct connection {
  uint16_t get_track(event_type const t) const {
    return t == event_type::DEP ? d_track_ : a_track_;
  }

  ptr<connection_info const> con_info_{nullptr};
  uint16_t price_{0U};
  uint16_t d_track_{0U}, a_track_{0U};
  service_class clasz_{service_class::AIR};  // service_class 0
};

struct light_connection {
  light_connection() = default;

  light_connection(mam_t const d_time, mam_t const a_time)
      : full_con_{nullptr},
        d_time_{d_time},
        a_time_{a_time},
        traffic_days_{0U},
        trips_{0U},
        valid_{false} {}

  light_connection(mam_t const d_time, mam_t const a_time,
                   size_t const bitfield_idx, connection const* full_con,
                   merged_trips_idx const trips)
      : full_con_{full_con},
        d_time_{d_time},
        a_time_{a_time},
        traffic_days_{bitfield_idx},
        trips_{trips},
        valid_{1U} {}

  time event_time(event_type const t, day_idx_t day) const {
    return {day, t == event_type::DEP ? d_time_ : a_time_};
  }

  duration_t travel_time() const { return a_time_ - d_time_; }

  ptr<connection const> full_con_{nullptr};
  mam_t d_time_{std::numeric_limits<decltype(d_time_)>::max()};
  mam_t a_time_{std::numeric_limits<decltype(a_time_)>::max()};
  bitfield_idx_or_ptr traffic_days_;
  uint32_t trips_ : 31;
  uint32_t valid_ : 1;
};

struct d_time_lt {
  bool operator()(light_connection const& a, light_connection const& b) {
    return a.d_time_ < b.d_time_;
  }
};

struct a_time_gt {
  bool operator()(light_connection const& a, light_connection const& b) {
    return a.a_time_ > b.a_time_;
  }
};

struct a_time_lt {
  bool operator()(light_connection const& a, light_connection const& b) {
    return a.a_time_ < b.a_time_;
  }
};

// Index of a light_connection in a route edge.
using lcon_idx_t = uint32_t;

}  // namespace motis
