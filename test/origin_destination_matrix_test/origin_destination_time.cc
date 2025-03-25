#pragma once
#include <nigiri/shapes_storage.h>
#include <utl/init_from.h>

#include "nigiri/routing/limits.h"
#include "gtest/gtest.h"
#include "motis/origin_destination_matrix/origin_destination_matrix.h"

#include "motis/import.h"

namespace origin_destination_matrix {

using namespace std::string_view_literals;

TEST(origin_destination_matrix, time_measurement) {
  constexpr auto const kGTFS = R"(
# agency.txt
agency_id,agency_name,agency_url,agency_timezone
DB,Deutsche Bahn,https://deutschebahn.com,Europe/Berlin

# stops.txt
stop_id,stop_name,stop_lat,stop_lon,location_type,parent_station,platform_code
DA,DA Hbf,49.87260,8.63085,1,,
DA_3,DA Hbf,49.87355,8.63003,0,DA,3
DA_10,DA Hbf,49.87336,8.62926,0,DA,10
FFM,FFM Hbf,50.10701,8.66341,1,,
FFM_101,FFM Hbf,50.10739,8.66333,0,FFM,101
FFM_10,FFM Hbf,50.10593,8.66118,0,FFM,10
FFM_12,FFM Hbf,50.10658,8.66178,0,FFM,12
de:6412:10:6:1,FFM Hbf U-Bahn,50.107577,8.6638173,0,FFM,U4
LANGEN,Langen,49.99359,8.65677,1,,1
FFM_HAUPT,FFM Hauptwache,50.11403,8.67835,1,,
FFM_HAUPT_U,Hauptwache U1/U2/U3/U8,50.11385,8.67912,0,FFM_HAUPT,
FFM_HAUPT_S,FFM Hauptwache S,50.11404,8.67824,0,FFM_HAUPT,

# routes.txt
route_id,agency_id,route_short_name,route_long_name,route_desc,route_type
S3,DB,S3,,,109
U4,DB,U4,,,402
ICE,DB,ICE,,,101

# trips.txt
route_id,service_id,trip_id,trip_headsign,block_id
S3,S1,S3,,
U4,S1,U4,,
ICE,S1,ICE,,

# stop_times.txt
trip_id,arrival_time,departure_time,stop_id,stop_sequence,pickup_type,drop_off_type
S3,01:15:00,01:15:00,FFM_101,1,0,0
S3,01:20:00,01:20:00,FFM_HAUPT_S,2,0,0
U4,01:05:00,01:05:00,de:6412:10:6:1,0,0,0
U4,01:10:00,01:10:00,FFM_HAUPT_U,1,0,0
ICE,00:35:00,00:35:00,DA_10,0,0,0
ICE,00:45:00,00:45:00,FFM_10,1,0,0

# calendar_dates.txt
service_id,date,exception_type
S1,20190501,1

# frequencies.txt
trip_id,start_time,end_time,headway_secs
S3,01:15:00,25:15:00,3600
ICE,00:35:00,24:35:00,3600
U4,01:05:00,25:01:00,3600
)"sv;
  Coordinates upper_left(8.62771903392948,49.87526849014631);
  Coordinates lower_right(8.629724234688751,49.87253873915287);
  std::chrono::year_month_day day(std::chrono::year(2019),std::chrono::month(5),std::chrono::day(1));
  unixtime_t start_time = std::chrono::sys_days(day) + std::chrono::minutes(720);
  auto end_time = start_time+std::chrono::minutes(43200);
  interval<unixtime_t> interval;
  interval.from_ = start_time;
  interval.to_ = end_time;

  auto const c = motis::config{
    .server_ = {{.web_folder_ = "ui/build", .n_threads_ = 1U}},
    .osm_ = {"test/resources/test_case.osm.pbf"},
    .tiles_ = {{.profile_ = "deps/tiles/profile/full.lua",
                .db_size_ = 1024U * 1024U * 25U}},
    .timetable_ =
          motis::config::timetable{
            .first_day_ = "2019-05-01",
            .num_days_ = 2,
            .datasets_ = {{"test", {.path_ = std::string{kGTFS}}}}},
    .gbfs_ = {{.feeds_ = {{"CAB", {.url_ = "./test/resources/gbfs"}}}}},
    .street_routing_ = true,
    .osr_footpath_ = true,
    .geocoding_ = true,
    .reverse_geocoding_ = true};
  auto d = import(c, "test/data", true);
  auto const routing = utl::init_from<motis::ep::routing>(d).value();

  constexpr short max_partitions = 10;
  for (int i = 2; i <= max_partitions; ++i) {
    auto start = std::chrono::system_clock::now();
    many_to_many_routing(upper_left,lower_right,i,i,interval,routing);
    auto end = std::chrono::system_clock::now();
    std::cout << "Time for "<<std::to_string(i)<<" partitions:" << std::chrono::duration_cast<std::chrono::microseconds>(end-start)<<std::endl;
  }

}

}  // namespace origin_destination_matrix
