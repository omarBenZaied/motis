#include <nigiri/shapes_storage.h>
#include <utl/init_from.h>

#include "gtest/gtest.h"
#include "motis/origin_destination_matrix/origin_destination_matrix.h"
#include "nigiri/routing/limits.h"

#include "boost/json.hpp"
#include "motis/import.h"

#include "fmt/xchar.h"

namespace origin_destination_matrix {
namespace json = boost::json;
using namespace std::string_view_literals;
using namespace motis;

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


constexpr auto const simple_GTFS = R"(
# agency.txt
agency_id,agency_name,agency_url,agency_timezone
DB,Deutsche Bahn,https://deutschebahn.com,Europe/Berlin

# stops.txt
stop_id,stop_name,stop_lat,stop_lon,location_type,parent_station,platform_code
DA,DA Hbf,49.87260,8.63085,1,,
FFM,FFM Hbf,50.10701,8.66341,1,,
LANGEN,Langen,49.99359,8.65677,1,,1

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
S3,01:15:00,01:15:00,FFM,1,0,0
S3,01:20:00,01:20:00,DA,2,0,0
U4,01:05:00,01:05:00,DA,0,0,0
U4,01:10:00,01:10:00,LANGEN,1,0,0
ICE,00:35:00,00:35:00,LANGEN,0,0,0
ICE,00:45:00,00:45:00,FFM,1,0,0

# calendar_dates.txt
service_id,date,exception_type
S1,20190501,1

# frequencies.txt
trip_id,start_time,end_time,headway_secs
S3,01:15:00,25:15:00,7200
ICE,00:35:00,24:35:00,7200
U4,01:05:00,25:05:00,7200
)"sv;


constexpr auto const simple_GTFS2 = R"(
# agency.txt
agency_id,agency_name,agency_url,agency_timezone
DB,Deutsche Bahn,https://deutschebahn.com,Europe/Berlin

# stops.txt
stop_id,stop_name,stop_lat,stop_lon,location_type,parent_station,platform_code
DA,DA Hbf,49.87260,8.63085,1,,
FFM,FFM Hbf,50.10701,8.66341,1,,
FFM_HAUPT_S,FFM Hauptwache S,50.11404,8.67824,0,FFM_HAUPT,
DA_10,DA Hbf,49.87336,8.62926,0,DA,10

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
S3,01:15:00,01:15:00,FFM,1,0,0
S3,01:20:00,01:20:00,FFM_HAUPT_S,2,0,0
U4,01:05:00,01:05:00,DA,0,0,0
U4,01:10:00,01:10:00,FFM,1,0,0
ICE,00:35:00,00:35:00,FFM_HAUPT_S,0,0,0
ICE,00:45:00,00:45:00,DA_10,1,0,0

# calendar_dates.txt
service_id,date,exception_type
S1,20190501,1

# frequencies.txt
trip_id,start_time,end_time,headway_secs
S3,01:15:00,25:15:00,14400
ICE,00:35:00,24:35:00,14400
U4,01:05:00,25:05:00,14400
)"sv;

void print_short(std::ostream& out, api::Itinerary const& j) {
  auto const format_time = [&](auto&& t, char const* fmt = "%F %H:%M") {
    out << date::format(fmt, *t);
  };
  auto const format_duration = [&](auto&& t, char const* fmt = "%H:%M") {
    out << date::format(fmt, std::chrono::milliseconds{t});
  };

  out << "date=";
  format_time(j.startTime_, "%F");
  out << ", start=";
  format_time(j.startTime_, "%H:%M");
  out << ", end=";
  format_time(j.endTime_, "%H:%M");

  out << ", duration=";
  format_duration(j.duration_ * 1000U);
  out << ", transfers=" << j.transfers_;

  out << ", legs=[\n";
  auto first = true;
  for (auto const& leg : j.legs_) {
    if (!first) {
      out << ",\n    ";
    } else {
      out << "    ";
    }
    first = false;
    out << "(";
    out << "from=" << leg.from_.stopId_.value_or("-")
        << " [track=" << leg.from_.track_.value_or("-")
        << ", scheduled_track=" << leg.from_.scheduledTrack_.value_or("-")
        << ", level=" << leg.from_.level_ << "]"
        << ", to=" << leg.to_.stopId_.value_or("-")
        << " [track=" << leg.to_.track_.value_or("-")
        << ", scheduled_track=" << leg.to_.scheduledTrack_.value_or("-")
        << ", level=" << leg.to_.level_ << "], ";
    out << "start=";
    format_time(leg.startTime_);
    out << ", mode=";
    out << json::serialize(json::value_from(leg.mode_));
    out << ", trip=\"" << leg.routeShortName_.value_or("-") << "\"";
    out << ", end=";
    format_time(leg.endTime_);
    out << ")";
  }
  out << "\n]";
}

void validate_result(std::vector<route_result> const& results,Coordinates const& from, Coordinates const& to,interval<unixtime_t> const& interval) {
  double constexpr DOUBLE_DIFFERENCE = 0.0000001;
  for(auto const& itinerary : results) {
    ASSERT_LT(interval.from_,itinerary.startTime_);
    ASSERT_GE(interval.to_,itinerary.startTime_);

    ASSERT_NEAR(itinerary.legs_.front().from_.lat_,from.y,DOUBLE_DIFFERENCE);
    ASSERT_NEAR(itinerary.legs_.front().from_.lon_,from.x,DOUBLE_DIFFERENCE);

    ASSERT_NEAR(itinerary.legs_.back().to_.lat_,to.y,DOUBLE_DIFFERENCE);
    ASSERT_NEAR(itinerary.legs_.back().to_.lon_,to.x,DOUBLE_DIFFERENCE);
  }
}

TEST(origin_destination_matrix,coordinates) {
  double constexpr DOUBLE_DIFFERENCE = 0.0000001;
  Coordinates upper_left(8.62771903392948,49.87526849014631);
  Coordinates lower_right(8.629724234688751,49.87253873915287);
  auto center_points = get_center_points(upper_left,lower_right,10,10);

  ASSERT_EQ(center_points.size(),100);
  auto x_difference = center_points[0].x-center_points[1].x;
  auto y_difference = center_points[0].y-center_points[10].y;

  for(int i=0;i<10;++i) {
    for(int j=0;j<10;++j) {
      auto current_point = center_points[j+10*i];
      auto left_point = j>0?center_points[j-1+10*i]:Coordinates(current_point.x-1,current_point.y);
      auto right_point = j<9?center_points[j+1+10*i]:Coordinates(current_point.x+1,current_point.y);
      auto upper_point = i>0?center_points[j+10*(i-1)]:Coordinates(current_point.x,current_point.y+1);
      auto lower_point = i<9?center_points[j+10*(i+1)]:Coordinates(current_point.x,current_point.y-1);

      ASSERT_EQ(left_point.y,current_point.y);
      ASSERT_EQ(current_point.y,right_point.y);
      ASSERT_LT(left_point.x,current_point.x);
      ASSERT_LT(current_point.x,right_point.x);

      ASSERT_EQ(upper_point.x,current_point.x);
      ASSERT_EQ(current_point.x,lower_point.x);
      ASSERT_GT(upper_point.y,current_point.y);
      ASSERT_GT(current_point.y,lower_point.y);

      if(j>0) {
        ASSERT_NEAR(left_point.x-current_point.x,x_difference,DOUBLE_DIFFERENCE);
      }
      if(i>0) {
        ASSERT_NEAR(upper_point.y-current_point.y,y_difference,DOUBLE_DIFFERENCE);
      }
    }
  }

  ASSERT_NEAR(center_points.front().x,upper_left.x-0.5*x_difference,DOUBLE_DIFFERENCE);
  ASSERT_NEAR(center_points.front().y,upper_left.y-0.5*y_difference,DOUBLE_DIFFERENCE);
  ASSERT_NEAR(center_points.back().x,lower_right.x+0.5*x_difference,DOUBLE_DIFFERENCE);
  ASSERT_NEAR(center_points.back().y,lower_right.y+0.5*y_difference,DOUBLE_DIFFERENCE);
}

TEST(origin_destination_matrix,intervals) {
  constexpr auto maximal_interval_minutes = std::chrono::duration_cast<std::chrono::minutes>(routing::kMaxSearchIntervalSize);
  auto now = std::chrono::system_clock::now();
  unixtime_t start_time(std::chrono::duration_cast<std::chrono::minutes>(now.time_since_epoch()));
  unixtime_t end_time = start_time+std::chrono::minutes(43200);
  interval<unixtime_t> interval{start_time,end_time};
  auto intervals = make_intervals(interval);
  ASSERT_FALSE(intervals.empty());
  ASSERT_EQ(intervals.front().from_,interval.from_);
  ASSERT_EQ(intervals.back().to_,interval.to_);
  for(auto i=0;i<intervals.size();++i) {
    auto part_int = intervals[i];
    ASSERT_LE(part_int.from_,part_int.to_);
    ASSERT_LE(part_int.size(),maximal_interval_minutes);
    if(i<intervals.size()-1) ASSERT_EQ(part_int.to_,intervals[i+1].from_);
  }
}

TEST(motis,origin_destination_matrix) {
  Coordinates upper_left(8.62771903392948,49.87526849014631);
  Coordinates lower_right(8.629724234688751,49.87253873915287);
  std::chrono::year_month_day day(std::chrono::year(2019),std::chrono::month(5),std::chrono::day(1));
  unixtime_t start_time = std::chrono::sys_days(day);
  /*auto now = std::chrono::system_clock::now();
  unixtime_t start_time(std::chrono::duration_cast<std::chrono::minutes>(now.time_since_epoch()));*/
  // 30 Days
  auto end_time = start_time+std::chrono::minutes(43200);
  interval<unixtime_t> interval;
  interval.from_ = start_time;
  interval.to_ = end_time;

  auto ec = std::error_code{};
  std::filesystem::remove_all("test/data", ec);

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
  auto const routing = utl::init_from<ep::routing>(d).value();

  constexpr int partitions = 2;
  auto result = many_to_many_routing(upper_left,lower_right,partitions,partitions,interval,routing);
  EXPECT_EQ(result.size(),partitions*partitions);
  for(auto const& r : result) {
    EXPECT_EQ(r.size(),partitions*partitions);
  }
  auto coordinates = get_center_points(upper_left,lower_right,partitions,partitions);
  double constexpr DOUBLE_DIFFERENCE = 0.0000001;
  for(int i=0;i<partitions*partitions;++i) {
    for(int j=0;j<partitions*partitions;++j) {
      if(i==j) ASSERT_TRUE(result[i][j].empty());
      for(auto const& itinerary:result[i][j]) {
        EXPECT_LE(start_time,itinerary.startTime_);
        EXPECT_GE(end_time,itinerary.startTime_);
        auto const& from = coordinates[i];
        auto const& to = coordinates[j];
        EXPECT_NEAR(itinerary.legs_.front().from_.lat_,from.y,DOUBLE_DIFFERENCE);
        EXPECT_NEAR(itinerary.legs_.front().from_.lon_,from.x,DOUBLE_DIFFERENCE);
        EXPECT_NEAR(itinerary.legs_.back().to_.lat_,to.y,DOUBLE_DIFFERENCE);
        EXPECT_NEAR(itinerary.legs_.back().to_.lon_,to.x,DOUBLE_DIFFERENCE);
      }
    }
  }
}

TEST(motis,origin_destination_matrix_simple_plan) {
  /*
  DA,DA Hbf,49.87260,8.63085,1,,
FFM,FFM Hbf,50.10701,8.66341,1,,
LANGEN,Langen,49.99359,8.65677,1,,1
   */
  /*Coordinates north_west_corner(8,51);
  Coordinates south_east_corner(9,49);*/
  Coordinates north_west_corner(8.63084,50.10702);
  Coordinates south_east_corner(8.66342,49.87259);
  auto coordinates = get_center_points(north_west_corner,south_east_corner,2,2);
  std::chrono::year_month_day day(std::chrono::year(2019),std::chrono::month(5),std::chrono::day(1));
  unixtime_t start_time = std::chrono::sys_days(day);
  auto end_time = start_time + std::chrono::minutes(1440);
  interval<unixtime_t> interval;
  interval.from_ = start_time;
  interval.to_ = end_time;

  auto ec = std::error_code{};
  std::filesystem::remove_all("test/data", ec);

  auto const c = config{
    .server_ = {{.web_folder_ = "ui/build", .n_threads_ = 1U}},
    .osm_ = {"test/resources/test_case.osm.pbf"},
    .tiles_ = {{.profile_ = "deps/tiles/profile/full.lua",
                .db_size_ = 1024U * 1024U * 25U}},
    .timetable_ =
          config::timetable{
            .first_day_ = "2019-05-01",
            .num_days_ = 1,
            .datasets_ = {{"test", {.path_ = std::string{simple_GTFS}}}}},
    .gbfs_ = {{.feeds_ = {{"CAB", {.url_ = "./test/resources/gbfs"}}}}},
    .street_routing_ = true,
    .osr_footpath_ = true,
    .geocoding_ = true,
    .reverse_geocoding_ = true};
  auto d = import(c, "test/data", true);
  auto const routing = utl::init_from<ep::routing>(d).value();
  auto result = many_to_many_routing(north_west_corner,south_east_corner,2,2,interval,routing);
  ASSERT_EQ(result.size(),4);
  ASSERT_EQ(result[0].size(),4);
  ASSERT_EQ(result[1].size(),4);
  for(int i=0;i<4;++i) {
    for(int j=0;j<4;++j) {
      std::cout << i<<','<< j <<' '<<result[i][j].empty()<< std::endl;
      validate_result(result[i][j],coordinates[i],coordinates[j],interval);
      for(auto const& itinerary:result[i][j]) {
        auto ss = std::stringstream{};
        print_short(ss, itinerary);
        std::cout << "---\n" << ss.str() << "\n---\n" << std::endl;
      }
    }
  }
}

TEST(motis,route_simple_plan) {

  std::vector<std::string> expected_strings[2];

  expected_strings[0] = {R"(date=2019-05-01, start=01:04, end=02:47, duration=01:43, transfers=1, legs=[
    (from=- [track=-, scheduled_track=-, level=0], to=test_DA [track=-, scheduled_track=-, level=0], start=2019-05-01 01:04, mode="WALK", trip="-", end=2019-05-01 01:05),
    (from=test_DA [track=-, scheduled_track=-, level=0], to=test_LANGEN [track=-, scheduled_track=-, level=0], start=2019-05-01 01:05, mode="SUBWAY", trip="U4", end=2019-05-01 01:10),
    (from=test_LANGEN [track=-, scheduled_track=-, level=0], to=test_LANGEN [track=-, scheduled_track=-, level=0], start=2019-05-01 01:10, mode="WALK", trip="-", end=2019-05-01 01:12),
    (from=test_LANGEN [track=-, scheduled_track=-, level=0], to=test_FFM [track=-, scheduled_track=-, level=-3], start=2019-05-01 02:35, mode="HIGHSPEED_RAIL", trip="ICE ", end=2019-05-01 02:45),
    (from=test_FFM [track=-, scheduled_track=-, level=-3], to=- [track=-, scheduled_track=-, level=0], start=2019-05-01 02:45, mode="WALK", trip="-", end=2019-05-01 02:47)
])",

  R"(date=2019-05-01, start=03:04, end=04:47, duration=01:43, transfers=1, legs=[
    (from=- [track=-, scheduled_track=-, level=0], to=test_DA [track=-, scheduled_track=-, level=0], start=2019-05-01 03:04, mode="WALK", trip="-", end=2019-05-01 03:05),
    (from=test_DA [track=-, scheduled_track=-, level=0], to=test_LANGEN [track=-, scheduled_track=-, level=0], start=2019-05-01 03:05, mode="SUBWAY", trip="U4", end=2019-05-01 03:10),
    (from=test_LANGEN [track=-, scheduled_track=-, level=0], to=test_LANGEN [track=-, scheduled_track=-, level=0], start=2019-05-01 03:10, mode="WALK", trip="-", end=2019-05-01 03:12),
    (from=test_LANGEN [track=-, scheduled_track=-, level=0], to=test_FFM [track=-, scheduled_track=-, level=-3], start=2019-05-01 04:35, mode="HIGHSPEED_RAIL", trip="ICE ", end=2019-05-01 04:45),
    (from=test_FFM [track=-, scheduled_track=-, level=-3], to=- [track=-, scheduled_track=-, level=0], start=2019-05-01 04:45, mode="WALK", trip="-", end=2019-05-01 04:47)
])",

  R"(date=2019-05-01, start=05:04, end=06:47, duration=01:43, transfers=1, legs=[
    (from=- [track=-, scheduled_track=-, level=0], to=test_DA [track=-, scheduled_track=-, level=0], start=2019-05-01 05:04, mode="WALK", trip="-", end=2019-05-01 05:05),
    (from=test_DA [track=-, scheduled_track=-, level=0], to=test_LANGEN [track=-, scheduled_track=-, level=0], start=2019-05-01 05:05, mode="SUBWAY", trip="U4", end=2019-05-01 05:10),
    (from=test_LANGEN [track=-, scheduled_track=-, level=0], to=test_LANGEN [track=-, scheduled_track=-, level=0], start=2019-05-01 05:10, mode="WALK", trip="-", end=2019-05-01 05:12),
    (from=test_LANGEN [track=-, scheduled_track=-, level=0], to=test_FFM [track=-, scheduled_track=-, level=-3], start=2019-05-01 06:35, mode="HIGHSPEED_RAIL", trip="ICE ", end=2019-05-01 06:45),
    (from=test_FFM [track=-, scheduled_track=-, level=-3], to=- [track=-, scheduled_track=-, level=0], start=2019-05-01 06:45, mode="WALK", trip="-", end=2019-05-01 06:47)
])",

  R"(date=2019-05-01, start=07:04, end=08:47, duration=01:43, transfers=1, legs=[
    (from=- [track=-, scheduled_track=-, level=0], to=test_DA [track=-, scheduled_track=-, level=0], start=2019-05-01 07:04, mode="WALK", trip="-", end=2019-05-01 07:05),
    (from=test_DA [track=-, scheduled_track=-, level=0], to=test_LANGEN [track=-, scheduled_track=-, level=0], start=2019-05-01 07:05, mode="SUBWAY", trip="U4", end=2019-05-01 07:10),
    (from=test_LANGEN [track=-, scheduled_track=-, level=0], to=test_LANGEN [track=-, scheduled_track=-, level=0], start=2019-05-01 07:10, mode="WALK", trip="-", end=2019-05-01 07:12),
    (from=test_LANGEN [track=-, scheduled_track=-, level=0], to=test_FFM [track=-, scheduled_track=-, level=-3], start=2019-05-01 08:35, mode="HIGHSPEED_RAIL", trip="ICE ", end=2019-05-01 08:45),
    (from=test_FFM [track=-, scheduled_track=-, level=-3], to=- [track=-, scheduled_track=-, level=0], start=2019-05-01 08:45, mode="WALK", trip="-", end=2019-05-01 08:47)
])",

  R"(date=2019-05-01, start=09:04, end=10:47, duration=01:43, transfers=1, legs=[
    (from=- [track=-, scheduled_track=-, level=0], to=test_DA [track=-, scheduled_track=-, level=0], start=2019-05-01 09:04, mode="WALK", trip="-", end=2019-05-01 09:05),
    (from=test_DA [track=-, scheduled_track=-, level=0], to=test_LANGEN [track=-, scheduled_track=-, level=0], start=2019-05-01 09:05, mode="SUBWAY", trip="U4", end=2019-05-01 09:10),
    (from=test_LANGEN [track=-, scheduled_track=-, level=0], to=test_LANGEN [track=-, scheduled_track=-, level=0], start=2019-05-01 09:10, mode="WALK", trip="-", end=2019-05-01 09:12),
    (from=test_LANGEN [track=-, scheduled_track=-, level=0], to=test_FFM [track=-, scheduled_track=-, level=-3], start=2019-05-01 10:35, mode="HIGHSPEED_RAIL", trip="ICE ", end=2019-05-01 10:45),
    (from=test_FFM [track=-, scheduled_track=-, level=-3], to=- [track=-, scheduled_track=-, level=0], start=2019-05-01 10:45, mode="WALK", trip="-", end=2019-05-01 10:47)
])",

  R"(date=2019-05-01, start=11:04, end=12:47, duration=01:43, transfers=1, legs=[
    (from=- [track=-, scheduled_track=-, level=0], to=test_DA [track=-, scheduled_track=-, level=0], start=2019-05-01 11:04, mode="WALK", trip="-", end=2019-05-01 11:05),
    (from=test_DA [track=-, scheduled_track=-, level=0], to=test_LANGEN [track=-, scheduled_track=-, level=0], start=2019-05-01 11:05, mode="SUBWAY", trip="U4", end=2019-05-01 11:10),
    (from=test_LANGEN [track=-, scheduled_track=-, level=0], to=test_LANGEN [track=-, scheduled_track=-, level=0], start=2019-05-01 11:10, mode="WALK", trip="-", end=2019-05-01 11:12),
    (from=test_LANGEN [track=-, scheduled_track=-, level=0], to=test_FFM [track=-, scheduled_track=-, level=-3], start=2019-05-01 12:35, mode="HIGHSPEED_RAIL", trip="ICE ", end=2019-05-01 12:45),
    (from=test_FFM [track=-, scheduled_track=-, level=-3], to=- [track=-, scheduled_track=-, level=0], start=2019-05-01 12:45, mode="WALK", trip="-", end=2019-05-01 12:47)
])",

  R"(date=2019-05-01, start=13:04, end=14:47, duration=01:43, transfers=1, legs=[
    (from=- [track=-, scheduled_track=-, level=0], to=test_DA [track=-, scheduled_track=-, level=0], start=2019-05-01 13:04, mode="WALK", trip="-", end=2019-05-01 13:05),
    (from=test_DA [track=-, scheduled_track=-, level=0], to=test_LANGEN [track=-, scheduled_track=-, level=0], start=2019-05-01 13:05, mode="SUBWAY", trip="U4", end=2019-05-01 13:10),
    (from=test_LANGEN [track=-, scheduled_track=-, level=0], to=test_LANGEN [track=-, scheduled_track=-, level=0], start=2019-05-01 13:10, mode="WALK", trip="-", end=2019-05-01 13:12),
    (from=test_LANGEN [track=-, scheduled_track=-, level=0], to=test_FFM [track=-, scheduled_track=-, level=-3], start=2019-05-01 14:35, mode="HIGHSPEED_RAIL", trip="ICE ", end=2019-05-01 14:45),
    (from=test_FFM [track=-, scheduled_track=-, level=-3], to=- [track=-, scheduled_track=-, level=0], start=2019-05-01 14:45, mode="WALK", trip="-", end=2019-05-01 14:47)
])",

  R"(date=2019-05-01, start=15:04, end=16:47, duration=01:43, transfers=1, legs=[
    (from=- [track=-, scheduled_track=-, level=0], to=test_DA [track=-, scheduled_track=-, level=0], start=2019-05-01 15:04, mode="WALK", trip="-", end=2019-05-01 15:05),
    (from=test_DA [track=-, scheduled_track=-, level=0], to=test_LANGEN [track=-, scheduled_track=-, level=0], start=2019-05-01 15:05, mode="SUBWAY", trip="U4", end=2019-05-01 15:10),
    (from=test_LANGEN [track=-, scheduled_track=-, level=0], to=test_LANGEN [track=-, scheduled_track=-, level=0], start=2019-05-01 15:10, mode="WALK", trip="-", end=2019-05-01 15:12),
    (from=test_LANGEN [track=-, scheduled_track=-, level=0], to=test_FFM [track=-, scheduled_track=-, level=-3], start=2019-05-01 16:35, mode="HIGHSPEED_RAIL", trip="ICE ", end=2019-05-01 16:45),
    (from=test_FFM [track=-, scheduled_track=-, level=-3], to=- [track=-, scheduled_track=-, level=0], start=2019-05-01 16:45, mode="WALK", trip="-", end=2019-05-01 16:47)
])",

  R"(date=2019-05-01, start=17:04, end=18:47, duration=01:43, transfers=1, legs=[
    (from=- [track=-, scheduled_track=-, level=0], to=test_DA [track=-, scheduled_track=-, level=0], start=2019-05-01 17:04, mode="WALK", trip="-", end=2019-05-01 17:05),
    (from=test_DA [track=-, scheduled_track=-, level=0], to=test_LANGEN [track=-, scheduled_track=-, level=0], start=2019-05-01 17:05, mode="SUBWAY", trip="U4", end=2019-05-01 17:10),
    (from=test_LANGEN [track=-, scheduled_track=-, level=0], to=test_LANGEN [track=-, scheduled_track=-, level=0], start=2019-05-01 17:10, mode="WALK", trip="-", end=2019-05-01 17:12),
    (from=test_LANGEN [track=-, scheduled_track=-, level=0], to=test_FFM [track=-, scheduled_track=-, level=-3], start=2019-05-01 18:35, mode="HIGHSPEED_RAIL", trip="ICE ", end=2019-05-01 18:45),
    (from=test_FFM [track=-, scheduled_track=-, level=-3], to=- [track=-, scheduled_track=-, level=0], start=2019-05-01 18:45, mode="WALK", trip="-", end=2019-05-01 18:47)
])",

  R"(date=2019-05-01, start=19:04, end=20:47, duration=01:43, transfers=1, legs=[
    (from=- [track=-, scheduled_track=-, level=0], to=test_DA [track=-, scheduled_track=-, level=0], start=2019-05-01 19:04, mode="WALK", trip="-", end=2019-05-01 19:05),
    (from=test_DA [track=-, scheduled_track=-, level=0], to=test_LANGEN [track=-, scheduled_track=-, level=0], start=2019-05-01 19:05, mode="SUBWAY", trip="U4", end=2019-05-01 19:10),
    (from=test_LANGEN [track=-, scheduled_track=-, level=0], to=test_LANGEN [track=-, scheduled_track=-, level=0], start=2019-05-01 19:10, mode="WALK", trip="-", end=2019-05-01 19:12),
    (from=test_LANGEN [track=-, scheduled_track=-, level=0], to=test_FFM [track=-, scheduled_track=-, level=-3], start=2019-05-01 20:35, mode="HIGHSPEED_RAIL", trip="ICE ", end=2019-05-01 20:45),
    (from=test_FFM [track=-, scheduled_track=-, level=-3], to=- [track=-, scheduled_track=-, level=0], start=2019-05-01 20:45, mode="WALK", trip="-", end=2019-05-01 20:47)
])"};

  expected_strings[1] = {R"(date=2019-05-01, start=01:13, end=01:21, duration=00:08, transfers=0, legs=[
    (from=- [track=-, scheduled_track=-, level=0], to=test_FFM [track=-, scheduled_track=-, level=-3], start=2019-05-01 01:13, mode="WALK", trip="-", end=2019-05-01 01:15),
    (from=test_FFM [track=-, scheduled_track=-, level=-3], to=test_DA [track=-, scheduled_track=-, level=0], start=2019-05-01 01:15, mode="METRO", trip="S3", end=2019-05-01 01:20),
    (from=test_DA [track=-, scheduled_track=-, level=0], to=- [track=-, scheduled_track=-, level=0], start=2019-05-01 01:20, mode="WALK", trip="-", end=2019-05-01 01:21)
])",

  R"(date=2019-05-01, start=03:13, end=03:21, duration=00:08, transfers=0, legs=[
    (from=- [track=-, scheduled_track=-, level=0], to=test_FFM [track=-, scheduled_track=-, level=-3], start=2019-05-01 03:13, mode="WALK", trip="-", end=2019-05-01 03:15),
    (from=test_FFM [track=-, scheduled_track=-, level=-3], to=test_DA [track=-, scheduled_track=-, level=0], start=2019-05-01 03:15, mode="METRO", trip="S3", end=2019-05-01 03:20),
    (from=test_DA [track=-, scheduled_track=-, level=0], to=- [track=-, scheduled_track=-, level=0], start=2019-05-01 03:20, mode="WALK", trip="-", end=2019-05-01 03:21)
])",

  R"(date=2019-05-01, start=05:13, end=05:21, duration=00:08, transfers=0, legs=[
    (from=- [track=-, scheduled_track=-, level=0], to=test_FFM [track=-, scheduled_track=-, level=-3], start=2019-05-01 05:13, mode="WALK", trip="-", end=2019-05-01 05:15),
    (from=test_FFM [track=-, scheduled_track=-, level=-3], to=test_DA [track=-, scheduled_track=-, level=0], start=2019-05-01 05:15, mode="METRO", trip="S3", end=2019-05-01 05:20),
    (from=test_DA [track=-, scheduled_track=-, level=0], to=- [track=-, scheduled_track=-, level=0], start=2019-05-01 05:20, mode="WALK", trip="-", end=2019-05-01 05:21)
])",

  R"(date=2019-05-01, start=07:13, end=07:21, duration=00:08, transfers=0, legs=[
    (from=- [track=-, scheduled_track=-, level=0], to=test_FFM [track=-, scheduled_track=-, level=-3], start=2019-05-01 07:13, mode="WALK", trip="-", end=2019-05-01 07:15),
    (from=test_FFM [track=-, scheduled_track=-, level=-3], to=test_DA [track=-, scheduled_track=-, level=0], start=2019-05-01 07:15, mode="METRO", trip="S3", end=2019-05-01 07:20),
    (from=test_DA [track=-, scheduled_track=-, level=0], to=- [track=-, scheduled_track=-, level=0], start=2019-05-01 07:20, mode="WALK", trip="-", end=2019-05-01 07:21)
])",

  R"(date=2019-05-01, start=09:13, end=09:21, duration=00:08, transfers=0, legs=[
    (from=- [track=-, scheduled_track=-, level=0], to=test_FFM [track=-, scheduled_track=-, level=-3], start=2019-05-01 09:13, mode="WALK", trip="-", end=2019-05-01 09:15),
    (from=test_FFM [track=-, scheduled_track=-, level=-3], to=test_DA [track=-, scheduled_track=-, level=0], start=2019-05-01 09:15, mode="METRO", trip="S3", end=2019-05-01 09:20),
    (from=test_DA [track=-, scheduled_track=-, level=0], to=- [track=-, scheduled_track=-, level=0], start=2019-05-01 09:20, mode="WALK", trip="-", end=2019-05-01 09:21)
])",

  R"(date=2019-05-01, start=11:13, end=11:21, duration=00:08, transfers=0, legs=[
    (from=- [track=-, scheduled_track=-, level=0], to=test_FFM [track=-, scheduled_track=-, level=-3], start=2019-05-01 11:13, mode="WALK", trip="-", end=2019-05-01 11:15),
    (from=test_FFM [track=-, scheduled_track=-, level=-3], to=test_DA [track=-, scheduled_track=-, level=0], start=2019-05-01 11:15, mode="METRO", trip="S3", end=2019-05-01 11:20),
    (from=test_DA [track=-, scheduled_track=-, level=0], to=- [track=-, scheduled_track=-, level=0], start=2019-05-01 11:20, mode="WALK", trip="-", end=2019-05-01 11:21)
])",

  R"(date=2019-05-01, start=13:13, end=13:21, duration=00:08, transfers=0, legs=[
    (from=- [track=-, scheduled_track=-, level=0], to=test_FFM [track=-, scheduled_track=-, level=-3], start=2019-05-01 13:13, mode="WALK", trip="-", end=2019-05-01 13:15),
    (from=test_FFM [track=-, scheduled_track=-, level=-3], to=test_DA [track=-, scheduled_track=-, level=0], start=2019-05-01 13:15, mode="METRO", trip="S3", end=2019-05-01 13:20),
    (from=test_DA [track=-, scheduled_track=-, level=0], to=- [track=-, scheduled_track=-, level=0], start=2019-05-01 13:20, mode="WALK", trip="-", end=2019-05-01 13:21)
])",

  R"(date=2019-05-01, start=15:13, end=15:21, duration=00:08, transfers=0, legs=[
    (from=- [track=-, scheduled_track=-, level=0], to=test_FFM [track=-, scheduled_track=-, level=-3], start=2019-05-01 15:13, mode="WALK", trip="-", end=2019-05-01 15:15),
    (from=test_FFM [track=-, scheduled_track=-, level=-3], to=test_DA [track=-, scheduled_track=-, level=0], start=2019-05-01 15:15, mode="METRO", trip="S3", end=2019-05-01 15:20),
    (from=test_DA [track=-, scheduled_track=-, level=0], to=- [track=-, scheduled_track=-, level=0], start=2019-05-01 15:20, mode="WALK", trip="-", end=2019-05-01 15:21)
])",

  R"(date=2019-05-01, start=17:13, end=17:21, duration=00:08, transfers=0, legs=[
    (from=- [track=-, scheduled_track=-, level=0], to=test_FFM [track=-, scheduled_track=-, level=-3], start=2019-05-01 17:13, mode="WALK", trip="-", end=2019-05-01 17:15),
    (from=test_FFM [track=-, scheduled_track=-, level=-3], to=test_DA [track=-, scheduled_track=-, level=0], start=2019-05-01 17:15, mode="METRO", trip="S3", end=2019-05-01 17:20),
    (from=test_DA [track=-, scheduled_track=-, level=0], to=- [track=-, scheduled_track=-, level=0], start=2019-05-01 17:20, mode="WALK", trip="-", end=2019-05-01 17:21)
])",

  R"(date=2019-05-01, start=19:13, end=19:21, duration=00:08, transfers=0, legs=[
    (from=- [track=-, scheduled_track=-, level=0], to=test_FFM [track=-, scheduled_track=-, level=-3], start=2019-05-01 19:13, mode="WALK", trip="-", end=2019-05-01 19:15),
    (from=test_FFM [track=-, scheduled_track=-, level=-3], to=test_DA [track=-, scheduled_track=-, level=0], start=2019-05-01 19:15, mode="METRO", trip="S3", end=2019-05-01 19:20),
    (from=test_DA [track=-, scheduled_track=-, level=0], to=- [track=-, scheduled_track=-, level=0], start=2019-05-01 19:20, mode="WALK", trip="-", end=2019-05-01 19:21)
])",

  R"(date=2019-05-01, start=21:13, end=21:21, duration=00:08, transfers=0, legs=[
    (from=- [track=-, scheduled_track=-, level=0], to=test_FFM [track=-, scheduled_track=-, level=-3], start=2019-05-01 21:13, mode="WALK", trip="-", end=2019-05-01 21:15),
    (from=test_FFM [track=-, scheduled_track=-, level=-3], to=test_DA [track=-, scheduled_track=-, level=0], start=2019-05-01 21:15, mode="METRO", trip="S3", end=2019-05-01 21:20),
    (from=test_DA [track=-, scheduled_track=-, level=0], to=- [track=-, scheduled_track=-, level=0], start=2019-05-01 21:20, mode="WALK", trip="-", end=2019-05-01 21:21)
])"};

  auto ec = std::error_code{};
  std::filesystem::remove_all("test/data", ec);

  auto const c = config{
    .server_ = {{.web_folder_ = "ui/build", .n_threads_ = 1U}},
    .osm_ = {"test/resources/test_case.osm.pbf"},
    .tiles_ = {{.profile_ = "deps/tiles/profile/full.lua",
                .db_size_ = 1024U * 1024U * 25U}},
    .timetable_ =
          motis::config::timetable{
            .first_day_ = "2019-05-01",
            .num_days_ = 1,
            .datasets_ = {{"test", {.path_ = std::string{simple_GTFS}}}}},
    .gbfs_ = {{.feeds_ = {{"CAB", {.url_ = "./test/resources/gbfs"}}}}},
    .street_routing_ = true,
    .osr_footpath_ = true,
    .geocoding_ = true,
    .reverse_geocoding_ = true};
  auto d = import(c, "test/data", true);
  auto const routing = utl::init_from<ep::routing>(d).value();

  std::chrono::year_month_day day(std::chrono::year(2019),std::chrono::month(5),std::chrono::day(1));
  unixtime_t start_time = std::chrono::sys_days(day);
  auto end_time = start_time + std::chrono::minutes(1440);
  interval<unixtime_t> interval;
  interval.from_ = start_time;
  interval.to_ = end_time;

  /*
  DA,DA Hbf,49.87260,8.63085,1,,
FFM,FFM Hbf,50.10701,8.66341,1,,
LANGEN,Langen,49.99359,8.65677,1,,1
   */
  std::vector<Coordinates> coordinates{Coordinates(8.63085,49.87260),
    Coordinates(8.66341,50.10701)};
  for(int i=0;i<coordinates.size();++i) {
    for(int j=0;j<coordinates.size();++j) {
      if(i==j) continue;
      auto result = route(coordinates[i],coordinates[j],interval,routing);
      validate_result(result,coordinates[i],coordinates[j],interval);
      ASSERT_EQ(result.size(),expected_strings[i].size());
      for(int k=0;k<result.size();++k) {
        auto ss = std::stringstream{};
        print_short(ss, result[k]);
        ASSERT_EQ(ss.str(),expected_strings[i][k]);
      }
    }
  }
}
}// namespace origin_destination_matrix