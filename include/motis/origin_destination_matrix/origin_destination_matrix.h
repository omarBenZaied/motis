#pragma once
#include <osmium/geom/coordinates.hpp>
#include "motis/endpoints/routing.h"

//#include <nigiri/routing/query.h>
//#include <nigiri/routing/search.h>
namespace origin_destination_matrix{
  using namespace osmium::geom;
  using namespace nigiri;
  using route_result = motis::api::Itinerary;
  using origin_destination_matrix = std::vector<std::vector<std::vector<route_result>>>;

  origin_destination_matrix many_to_many_routing(Coordinates const& north_west_corner,Coordinates const& south_east_corner ,int const& x_partitions,int const& y_partitions,interval<unixtime_t> const& start_time,motis::ep::routing const& routing,std::string const& additional = "");
  std::vector<Coordinates> get_center_points(Coordinates const& north_west_corner, Coordinates const& south_east_corner,int const& x_partitions,int const& y_partitions);
  std::vector<interval<unixtime_t>> make_intervals(interval<unixtime_t> const& start_time);
  std::vector<route_result> route(Coordinates const& from, Coordinates const& to, interval<unixtime_t>const& interval, motis::ep::routing const& routing,std::string const& additional = "");
} // namespace origin_destination_matrix
