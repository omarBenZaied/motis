#pragma once
#include "motis/origin_destination_matrix/origin_destination_matrix.h"
#include <nigiri/routing/query.h>
namespace origin_destination_matrix{
using namespace osmium::geom;
using namespace nigiri;

using route_result = motis::api::Itinerary;
using origin_destination_matrix = std::vector<std::vector<std::vector<route_result>>>;

std::vector<Coordinates> get_center_points(Coordinates const& north_west_corner, Coordinates const& south_east_corner,int const& x_partitions,int const& y_partitions) {
  auto east_degrees = south_east_corner.x-north_west_corner.x;
  auto south_degrees = south_east_corner.y-north_west_corner.y;

  auto field_width = east_degrees / x_partitions;
  auto field_height = south_degrees / y_partitions;

  auto start_x = north_west_corner.x;
  auto start_y = north_west_corner.y;

  std::vector<Coordinates> center_points;
  for(int i=0;i<y_partitions;++i) {
    for(int j=0;j<x_partitions;++j) {
      center_points.emplace_back(start_x + field_width * (j+0.5), start_y + field_height * (i+0.5));
      utl::verify(center_points.back().valid(),"Coordinate isnt valid");
    }
  }
  return center_points;
}

std::vector<interval<unixtime_t>> make_intervals(interval<unixtime_t> const& start_time) {
  auto max_interval_min = std::chrono::duration_cast<std::chrono::minutes>(routing::kMaxSearchIntervalSize);
  auto number_of_intervals = (start_time.to_ - start_time.from_).count()/max_interval_min.count();
  std::vector<interval<unixtime_t>> intervals;
  intervals.reserve(number_of_intervals+1);

  auto start_point = start_time.from_;
  for(int i=0;i<number_of_intervals;++i) {
    intervals.emplace_back(start_point,start_point+max_interval_min);
    start_point += max_interval_min;
  }
  intervals.emplace_back(start_point,start_time.to_);
  return intervals;
}


std::vector<route_result> route(Coordinates const& from, Coordinates const& to, interval<unixtime_t>const& interval, motis::ep::routing const& routing) {
  std::vector<route_result> results{};
  Coordinates swapped_from(from.y,from.x);
  Coordinates swapped_to(to.y,to.x);
  std::string from_string = "?fromPlace=";
  swapped_from.append_to_string(from_string,',',14);
  std::string to_string = "&toPlace=";
  swapped_to.append_to_string(to_string,',',14);
  std::string time_string = std::format("&time={0:%F}T{0:%R}Z",interval.from_);
  std::string window = "&searchWindow="+std::to_string((interval.to_ - interval.from_).count()*60);

  std::string query = from_string+to_string+time_string+window;
  auto routing_result = routing(query);
  results.append_range(routing_result.direct_);
  results.append_range(routing_result.itineraries_);
  return results;
}


origin_destination_matrix many_to_many_routing(Coordinates const& north_west_corner,Coordinates const& south_east_corner ,int const& x_partitions,int const& y_partitions,interval<unixtime_t> const& start_time,motis::ep::routing const& routing){

  auto center_points = get_center_points(north_west_corner,south_east_corner,x_partitions,y_partitions);
  auto time_intervals = make_intervals(start_time);

  origin_destination_matrix og_dest_matrix(center_points.size(),std::vector<std::vector<route_result>>{});
  for(auto& row : og_dest_matrix) {
    for(int j=0;j<center_points.size();++j) {
      row.emplace_back();
    }
  }

  for(int i=0;i<center_points.size();++i) {
      for(int j=0;j<center_points.size();++j) {
        if(i==j) continue;
        for(auto const& interval : time_intervals) {
          auto result = route(center_points[i],center_points[j],interval,routing);
          og_dest_matrix[i][j].append_range(result);
        }
      }
    }

  return og_dest_matrix;
}

} // origin_destination_matrix
