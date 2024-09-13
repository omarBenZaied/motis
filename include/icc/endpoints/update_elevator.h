#pragma once

#include "boost/json/value.hpp"

#include "utl/init_from.h"

#include "nigiri/types.h"

#include "osr/types.h"

#include "icc/fwd.h"
#include "icc/match_platforms.h"
#include "icc/point_rtree.h"
#include "icc/types.h"

namespace icc::ep {

struct update_elevator {
  boost::json::value operator()(boost::json::value const&) const;

  nigiri::timetable const& tt_;
  osr::ways const& w_;
  osr::lookup const& l_;
  osr::platforms const& pl_;
  point_rtree<nigiri::location_idx_t> const& loc_rtree_;
  hash_set<osr::node_idx_t> const& elevator_nodes_;
  platform_matches_t const& matches_;
  std::shared_ptr<rt>& rt_;
};

}  // namespace icc::ep