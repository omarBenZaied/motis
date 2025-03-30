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

using test_strings = std::vector<std::vector<std::vector<std::string>>>;

constexpr auto const simple_GTFS = R"(
# agency.txt
agency_id,agency_name,agency_url,agency_timezone
DB,Deutsche Bahn,https://deutschebahn.com,Europe/Berlin

# stops.txt
stop_id,stop_name,stop_lat,stop_lon,location_type,parent_station,platform_code
DA,DA Hbf,49.87260,8.63085,1,,
FFM,FFM Hbf,50.10701,8.66341,1,,
LANGEN,Langen,49.99359,8.65677,1,,

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
S3,01:15:00,25:15:00,28800
ICE,00:35:00,24:35:00,28800
U4,01:05:00,25:05:00,28800
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
DA_10,DA Hbf,49.872797,8.631438,0,DA,10

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
S3,01:15:00,25:15:00,28800
ICE,00:35:00,24:35:00,28800
U4,01:05:00,25:05:00,28800
)"sv;

constexpr auto const simple_GTFS_Aachen = R"(
# agency.txt
agency_id,agency_name,agency_url,agency_timezone
DB,Deutsche Bahn,https://deutschebahn.com,Europe/Berlin

# stops.txt
stop_id,stop_name,stop_lat,stop_lon,location_type,parent_station,platform_code
ST1,Aachen Hbf,50.76769,6.091071,0,,
ST3,Schwelm,50.770202,6.116475,0,,
ST4,Duesseldorf Flughafen,50.769862,6.07384,0,,

# routes.txt
route_id,agency_id,route_short_name,route_long_name,route_desc,route_type
S3,DB,S3,,,109
S4,DB,S4,,,109
S5,DB,S5,,,109

# trips.txt
route_id,service_id,trip_id,trip_headsign,block_id
S3,S1,S3,,
S4,S1,S4,,
S5,S1,S5,,

# stop_times.txt
trip_id,arrival_time,departure_time,stop_id,stop_sequence,pickup_type,drop_off_type
S3,01:15:00,01:15:00,ST1,1,0,0
S3,01:20:00,01:20:00,ST3,2,0,0
S4,01:20:00,01:20:00,ST3,1,0,0
S4,01:25:00,01:25:00,ST4,2,0,0
S5,00:20:00,00:20:00,ST4,1,0,0
S5,00:25:00,00:25:00,ST1,2,0,0

# calendar_dates.txt
service_id,date,exception_type
S1,20190501,1

# frequencies.txt
trip_id,start_time,end_time,headway_secs
S3,01:15:00,25:15:00,28800
S4,01:20:00,25:20:00,28800
S5,00:20:00,24:20:00,28800
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
    ASSERT_LE(interval.from_,itinerary.startTime_);
    ASSERT_GE(interval.to_,itinerary.startTime_);

    ASSERT_NEAR(itinerary.legs_.front().from_.lat_,from.y,DOUBLE_DIFFERENCE);
    ASSERT_NEAR(itinerary.legs_.front().from_.lon_,from.x,DOUBLE_DIFFERENCE);

    ASSERT_NEAR(itinerary.legs_.back().to_.lat_,to.y,DOUBLE_DIFFERENCE);
    ASSERT_NEAR(itinerary.legs_.back().to_.lon_,to.x,DOUBLE_DIFFERENCE);
  }
}

void print_routes(std::vector<Coordinates> const& coordinates,std::string const& gtfs,
  interval<unixtime_t> const& interval,const char* street_file) {
  auto ec = std::error_code{};
  std::filesystem::remove_all("test/data", ec);

  auto const c = config{
    .server_ = {{.web_folder_ = "ui/build", .n_threads_ = 1U}},
    .osm_ = {street_file},
    .tiles_ = {{.profile_ = "deps/tiles/profile/full.lua",
                .db_size_ = 1024U * 1024U * 25U}},
    .timetable_ =
          motis::config::timetable{
            .first_day_ = "2019-05-01",
            .num_days_ = 1,
            .datasets_ = {{"test", {.path_ = gtfs}}}},
    .gbfs_ = {{.feeds_ = {{"CAB", {.url_ = "./test/resources/gbfs"}}}}},
    .street_routing_ = true,
    .osr_footpath_ = true,
    .geocoding_ = true,
    .reverse_geocoding_ = true};
  auto d = import(c, "test/data", true);
  auto const routing = utl::init_from<ep::routing>(d).value();

  for(int i=0;i<coordinates.size();++i) {
    for(int j=0;j<coordinates.size();++j) {
      if(i==j) continue;
      auto result = route(coordinates[i],coordinates[j],interval,routing);
      for(auto const& itinerary : result) {
        auto ss = std::stringstream{};
        print_short(ss, itinerary);
        std::cout << "---\n" << ss.str() << "\n---\n" << std::endl;
      }
    }
  }
}

void compare_with_expected(test_strings const& strings,origin_destination_matrix const& matrix) {
  ASSERT_EQ(strings.size(),matrix.size());
  for(int i=0;i<strings.size();++i) {
    ASSERT_EQ(strings[i].size(),matrix[i].size());
    for(int j=0;j<matrix[i].size();++j) {
      ASSERT_EQ(strings[i][j].size(),matrix[i][j].size());
      if(i==j) continue;
      for(int k=0;k<matrix[i][j].size();++k) {
        auto ss = std::stringstream{};
        print_short(ss, matrix[i][j][k]);
        ASSERT_EQ(strings[i][j][k],ss.str());
      }
    }
  }
}

void test_route(std::vector<Coordinates> const& coordinates,std::string const& gtfs,
  interval<unixtime_t> const& interval,test_strings const& expected_strings, const char* street_file) {
  auto ec = std::error_code{};
  std::filesystem::remove_all("test/data", ec);

  auto const c = config{
    .server_ = {{.web_folder_ = "ui/build", .n_threads_ = 1U}},
    .osm_ = {street_file},
    .tiles_ = {{.profile_ = "deps/tiles/profile/full.lua",
                .db_size_ = 1024U * 1024U * 25U}},
    .timetable_ =
          motis::config::timetable{
            .first_day_ = "2019-05-01",
            .num_days_ = 1,
            .datasets_ = {{"test", {.path_ = gtfs}}}},
    .gbfs_ = {{.feeds_ = {{"CAB", {.url_ = "./test/resources/gbfs"}}}}},
    .street_routing_ = true,
    .osr_footpath_ = true,
    .geocoding_ = true,
    .reverse_geocoding_ = true};
  auto d = import(c, "test/data", true);
  auto const routing = utl::init_from<ep::routing>(d).value();

  for(int i=0;i<coordinates.size();++i) {
    for(int j=0;j<coordinates.size();++j) {
      if(i==j) continue;
      auto result = route(coordinates[i],coordinates[j],interval,routing);
      validate_result(result,coordinates[i],coordinates[j],interval);
      ASSERT_EQ(result.size(),expected_strings[i][j].size());
      for(int k=0;k<result.size();++k) {
        auto ss = std::stringstream{};
        print_short(ss, result[k]);
        ASSERT_EQ(ss.str(),expected_strings[i][j][k]);
      }
    }
  }
}

test_strings make_test_strings(unsigned long long const& size) {
  return {size,std::vector<std::vector<std::string>>(size)};
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
          .datasets_ = {{"test", {.path_ = std::string{simple_GTFS}}}}},
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
  Coordinates north_west(6.069215,50.780202);
  Coordinates south_east(6.107975,50.76269);
  constexpr int PARTITIONS = 2;
  auto coordinates = get_center_points(north_west,south_east,PARTITIONS,PARTITIONS);
  std::chrono::year_month_day day(std::chrono::year(2019),std::chrono::month(5),std::chrono::day(1));
  unixtime_t start_time = std::chrono::sys_days(day);
  auto end_time = start_time + std::chrono::minutes(1440);
  interval<unixtime_t> interval;
  interval.from_ = start_time;
  interval.to_ = end_time;

  auto expected_strings = make_test_strings(PARTITIONS*PARTITIONS);

  expected_strings[0][1] = {R"(date=2019-05-01, start=00:00, end=00:27, duration=00:27, transfers=0, legs=[
    (from=- [track=-, scheduled_track=-, level=0], to=- [track=-, scheduled_track=-, level=0], start=2019-05-01 00:00, mode="WALK", trip="-", end=2019-05-01 00:27)
])"};
  expected_strings[0][2] = {R"(date=2019-05-01, start=00:00, end=00:18, duration=00:18, transfers=0, legs=[
    (from=- [track=-, scheduled_track=-, level=0], to=- [track=-, scheduled_track=-, level=0], start=2019-05-01 00:00, mode="WALK", trip="-", end=2019-05-01 00:18)
])"};
  expected_strings[0][3] = {R"(date=2019-05-01, start=00:00, end=00:29, duration=00:29, transfers=0, legs=[
    (from=- [track=-, scheduled_track=-, level=0], to=- [track=-, scheduled_track=-, level=0], start=2019-05-01 00:00, mode="WALK", trip="-", end=2019-05-01 00:29)
])"};
  expected_strings[1][0] = {R"(date=2019-05-01, start=00:00, end=00:27, duration=00:27, transfers=0, legs=[
    (from=- [track=-, scheduled_track=-, level=0], to=- [track=-, scheduled_track=-, level=0], start=2019-05-01 00:00, mode="WALK", trip="-", end=2019-05-01 00:27)
])"};
  expected_strings[1][3] = {R"(date=2019-05-01, start=00:00, end=00:19, duration=00:19, transfers=0, legs=[
    (from=- [track=-, scheduled_track=-, level=0], to=- [track=-, scheduled_track=-, level=0], start=2019-05-01 00:00, mode="WALK", trip="-", end=2019-05-01 00:19)
])"};
  expected_strings[2][0] = {R"(date=2019-05-01, start=00:00, end=00:18, duration=00:18, transfers=0, legs=[
    (from=- [track=-, scheduled_track=-, level=0], to=- [track=-, scheduled_track=-, level=0], start=2019-05-01 00:00, mode="WALK", trip="-", end=2019-05-01 00:18)
])"};
  expected_strings[2][3] = {R"(date=2019-05-01, start=00:00, end=00:28, duration=00:28, transfers=0, legs=[
    (from=- [track=-, scheduled_track=-, level=0], to=- [track=-, scheduled_track=-, level=0], start=2019-05-01 00:00, mode="WALK", trip="-", end=2019-05-01 00:28)
])",R"(date=2019-05-01, start=06:11, end=06:38, duration=00:27, transfers=0, legs=[
    (from=- [track=-, scheduled_track=-, level=0], to=test_ST4 [track=-, scheduled_track=-, level=1], start=2019-05-01 06:11, mode="WALK", trip="-", end=2019-05-01 06:20),
    (from=test_ST4 [track=-, scheduled_track=-, level=1], to=test_ST1 [track=-, scheduled_track=-, level=1], start=2019-05-01 06:20, mode="METRO", trip="S5", end=2019-05-01 06:25),
    (from=test_ST1 [track=-, scheduled_track=-, level=1], to=- [track=-, scheduled_track=-, level=0], start=2019-05-01 06:25, mode="WALK", trip="-", end=2019-05-01 06:38)
])",R"(date=2019-05-01, start=14:11, end=14:38, duration=00:27, transfers=0, legs=[
    (from=- [track=-, scheduled_track=-, level=0], to=test_ST4 [track=-, scheduled_track=-, level=1], start=2019-05-01 14:11, mode="WALK", trip="-", end=2019-05-01 14:20),
    (from=test_ST4 [track=-, scheduled_track=-, level=1], to=test_ST1 [track=-, scheduled_track=-, level=1], start=2019-05-01 14:20, mode="METRO", trip="S5", end=2019-05-01 14:25),
    (from=test_ST1 [track=-, scheduled_track=-, level=1], to=- [track=-, scheduled_track=-, level=0], start=2019-05-01 14:25, mode="WALK", trip="-", end=2019-05-01 14:38)
])"};
  expected_strings[3][0] = {R"(date=2019-05-01, start=00:00, end=00:29, duration=00:29, transfers=0, legs=[
    (from=- [track=-, scheduled_track=-, level=0], to=- [track=-, scheduled_track=-, level=0], start=2019-05-01 00:00, mode="WALK", trip="-", end=2019-05-01 00:29)
])"};
  expected_strings[3][1] = {R"(date=2019-05-01, start=00:00, end=00:19, duration=00:19, transfers=0, legs=[
    (from=- [track=-, scheduled_track=-, level=0], to=- [track=-, scheduled_track=-, level=0], start=2019-05-01 00:00, mode="WALK", trip="-", end=2019-05-01 00:19)
])"};
  expected_strings[3][2] = {R"(date=2019-05-01, start=00:00, end=00:28, duration=00:28, transfers=0, legs=[
    (from=- [track=-, scheduled_track=-, level=0], to=- [track=-, scheduled_track=-, level=0], start=2019-05-01 00:00, mode="WALK", trip="-", end=2019-05-01 00:28)
])"};

  auto ec = std::error_code{};
  std::filesystem::remove_all("test/data", ec);

  auto const c = config{
    .server_ = {{.web_folder_ = "ui/build", .n_threads_ = 1U}},
    .osm_ = {"test/resources/aachen.osm.pbf"},
    .tiles_ = {{.profile_ = "deps/tiles/profile/full.lua",
                .db_size_ = 1024U * 1024U * 25U}},
    .timetable_ =
          config::timetable{
            .first_day_ = "2019-05-01",
            .num_days_ = 1,
            .datasets_ = {{"test", {.path_ = std::string{simple_GTFS_Aachen}}}}},
    .gbfs_ = {{.feeds_ = {{"CAB", {.url_ = "./test/resources/gbfs"}}}}},
    .street_routing_ = true,
    .osr_footpath_ = true,
    .geocoding_ = true,
    .reverse_geocoding_ = true};
  auto d = import(c, "test/data", true);
  auto const routing = utl::init_from<ep::routing>(d).value();
  auto result = many_to_many_routing(north_west,south_east,PARTITIONS,PARTITIONS,interval,routing);
  auto expected_size = PARTITIONS*PARTITIONS;
  ASSERT_EQ(result.size(),expected_size);
  for(auto const& subvector: result) {
    ASSERT_EQ(subvector.size(),expected_size);
  }
  for(int i=0;i<expected_size;++i) {
    for(int j=0;j<expected_size;++j) {
      if(i==j) continue;
      validate_result(result[i][j],coordinates[i],coordinates[j],interval);
    }
  }
  compare_with_expected(expected_strings,result);

}

TEST(motis,route_simple_plan) {

  std::vector<Coordinates> coordinates{Coordinates(8.63085,49.87260),
    Coordinates(8.66341,50.10701),
  Coordinates(8.65677,49.99359)};

  auto expected_strings = make_test_strings(coordinates.size());

  expected_strings[0][1] = {R"(date=2019-05-01, start=07:04, end=14:47, duration=07:43, transfers=1, legs=[
    (from=- [track=-, scheduled_track=-, level=0], to=test_DA [track=-, scheduled_track=-, level=0], start=2019-05-01 07:04, mode="WALK", trip="-", end=2019-05-01 07:05),
    (from=test_DA [track=-, scheduled_track=-, level=0], to=test_LANGEN [track=-, scheduled_track=-, level=0], start=2019-05-01 07:05, mode="SUBWAY", trip="U4", end=2019-05-01 07:10),
    (from=test_LANGEN [track=-, scheduled_track=-, level=0], to=test_LANGEN [track=-, scheduled_track=-, level=0], start=2019-05-01 07:10, mode="WALK", trip="-", end=2019-05-01 07:12),
    (from=test_LANGEN [track=-, scheduled_track=-, level=0], to=test_FFM [track=-, scheduled_track=-, level=-3], start=2019-05-01 14:35, mode="HIGHSPEED_RAIL", trip="ICE ", end=2019-05-01 14:45),
    (from=test_FFM [track=-, scheduled_track=-, level=-3], to=- [track=-, scheduled_track=-, level=0], start=2019-05-01 14:45, mode="WALK", trip="-", end=2019-05-01 14:47)
])"};

  expected_strings[0][2] = {R"(date=2019-05-01, start=07:04, end=07:10, duration=00:06, transfers=0, legs=[
    (from=- [track=-, scheduled_track=-, level=0], to=test_DA [track=-, scheduled_track=-, level=0], start=2019-05-01 07:04, mode="WALK", trip="-", end=2019-05-01 07:05),
    (from=test_DA [track=-, scheduled_track=-, level=0], to=test_LANGEN [track=-, scheduled_track=-, level=0], start=2019-05-01 07:05, mode="SUBWAY", trip="U4", end=2019-05-01 07:10),
    (from=test_LANGEN [track=-, scheduled_track=-, level=0], to=- [track=-, scheduled_track=-, level=0], start=2019-05-01 07:10, mode="WALK", trip="-", end=2019-05-01 07:10)
])",
    R"(date=2019-05-01, start=15:04, end=15:10, duration=00:06, transfers=0, legs=[
    (from=- [track=-, scheduled_track=-, level=0], to=test_DA [track=-, scheduled_track=-, level=0], start=2019-05-01 15:04, mode="WALK", trip="-", end=2019-05-01 15:05),
    (from=test_DA [track=-, scheduled_track=-, level=0], to=test_LANGEN [track=-, scheduled_track=-, level=0], start=2019-05-01 15:05, mode="SUBWAY", trip="U4", end=2019-05-01 15:10),
    (from=test_LANGEN [track=-, scheduled_track=-, level=0], to=- [track=-, scheduled_track=-, level=0], start=2019-05-01 15:10, mode="WALK", trip="-", end=2019-05-01 15:10)
])"};

  expected_strings[1][0] = {R"(date=2019-05-01, start=07:13, end=07:21, duration=00:08, transfers=0, legs=[
    (from=- [track=-, scheduled_track=-, level=0], to=test_FFM [track=-, scheduled_track=-, level=-3], start=2019-05-01 07:13, mode="WALK", trip="-", end=2019-05-01 07:15),
    (from=test_FFM [track=-, scheduled_track=-, level=-3], to=test_DA [track=-, scheduled_track=-, level=0], start=2019-05-01 07:15, mode="METRO", trip="S3", end=2019-05-01 07:20),
    (from=test_DA [track=-, scheduled_track=-, level=0], to=- [track=-, scheduled_track=-, level=0], start=2019-05-01 07:20, mode="WALK", trip="-", end=2019-05-01 07:21)
])",
    R"(date=2019-05-01, start=15:13, end=15:21, duration=00:08, transfers=0, legs=[
    (from=- [track=-, scheduled_track=-, level=0], to=test_FFM [track=-, scheduled_track=-, level=-3], start=2019-05-01 15:13, mode="WALK", trip="-", end=2019-05-01 15:15),
    (from=test_FFM [track=-, scheduled_track=-, level=-3], to=test_DA [track=-, scheduled_track=-, level=0], start=2019-05-01 15:15, mode="METRO", trip="S3", end=2019-05-01 15:20),
    (from=test_DA [track=-, scheduled_track=-, level=0], to=- [track=-, scheduled_track=-, level=0], start=2019-05-01 15:20, mode="WALK", trip="-", end=2019-05-01 15:21)
])"};

  expected_strings[1][2] = {R"(date=2019-05-01, start=07:13, end=15:10, duration=07:57, transfers=1, legs=[
    (from=- [track=-, scheduled_track=-, level=0], to=test_FFM [track=-, scheduled_track=-, level=-3], start=2019-05-01 07:13, mode="WALK", trip="-", end=2019-05-01 07:15),
    (from=test_FFM [track=-, scheduled_track=-, level=-3], to=test_DA [track=-, scheduled_track=-, level=0], start=2019-05-01 07:15, mode="METRO", trip="S3", end=2019-05-01 07:20),
    (from=test_DA [track=-, scheduled_track=-, level=0], to=test_DA [track=-, scheduled_track=-, level=0], start=2019-05-01 07:20, mode="WALK", trip="-", end=2019-05-01 07:22),
    (from=test_DA [track=-, scheduled_track=-, level=0], to=test_LANGEN [track=-, scheduled_track=-, level=0], start=2019-05-01 15:05, mode="SUBWAY", trip="U4", end=2019-05-01 15:10),
    (from=test_LANGEN [track=-, scheduled_track=-, level=0], to=- [track=-, scheduled_track=-, level=0], start=2019-05-01 15:10, mode="WALK", trip="-", end=2019-05-01 15:10)
])"};

  expected_strings[2][0] = {R"(date=2019-05-01, start=06:35, end=07:21, duration=00:46, transfers=1, legs=[
    (from=- [track=-, scheduled_track=-, level=0], to=test_LANGEN [track=-, scheduled_track=-, level=0], start=2019-05-01 06:35, mode="WALK", trip="-", end=2019-05-01 06:35),
    (from=test_LANGEN [track=-, scheduled_track=-, level=0], to=test_FFM [track=-, scheduled_track=-, level=-3], start=2019-05-01 06:35, mode="HIGHSPEED_RAIL", trip="ICE ", end=2019-05-01 06:45),
    (from=test_FFM [track=-, scheduled_track=-, level=-3], to=test_FFM [track=-, scheduled_track=-, level=-3], start=2019-05-01 06:45, mode="WALK", trip="-", end=2019-05-01 06:47),
    (from=test_FFM [track=-, scheduled_track=-, level=-3], to=test_DA [track=-, scheduled_track=-, level=0], start=2019-05-01 07:15, mode="METRO", trip="S3", end=2019-05-01 07:20),
    (from=test_DA [track=-, scheduled_track=-, level=0], to=- [track=-, scheduled_track=-, level=0], start=2019-05-01 07:20, mode="WALK", trip="-", end=2019-05-01 07:21)
])",R"(date=2019-05-01, start=14:35, end=15:21, duration=00:46, transfers=1, legs=[
    (from=- [track=-, scheduled_track=-, level=0], to=test_LANGEN [track=-, scheduled_track=-, level=0], start=2019-05-01 14:35, mode="WALK", trip="-", end=2019-05-01 14:35),
    (from=test_LANGEN [track=-, scheduled_track=-, level=0], to=test_FFM [track=-, scheduled_track=-, level=-3], start=2019-05-01 14:35, mode="HIGHSPEED_RAIL", trip="ICE ", end=2019-05-01 14:45),
    (from=test_FFM [track=-, scheduled_track=-, level=-3], to=test_FFM [track=-, scheduled_track=-, level=-3], start=2019-05-01 14:45, mode="WALK", trip="-", end=2019-05-01 14:47),
    (from=test_FFM [track=-, scheduled_track=-, level=-3], to=test_DA [track=-, scheduled_track=-, level=0], start=2019-05-01 15:15, mode="METRO", trip="S3", end=2019-05-01 15:20),
    (from=test_DA [track=-, scheduled_track=-, level=0], to=- [track=-, scheduled_track=-, level=0], start=2019-05-01 15:20, mode="WALK", trip="-", end=2019-05-01 15:21)
])"};

  expected_strings[2][1] = {R"(date=2019-05-01, start=06:35, end=06:47, duration=00:12, transfers=0, legs=[
    (from=- [track=-, scheduled_track=-, level=0], to=test_LANGEN [track=-, scheduled_track=-, level=0], start=2019-05-01 06:35, mode="WALK", trip="-", end=2019-05-01 06:35),
    (from=test_LANGEN [track=-, scheduled_track=-, level=0], to=test_FFM [track=-, scheduled_track=-, level=-3], start=2019-05-01 06:35, mode="HIGHSPEED_RAIL", trip="ICE ", end=2019-05-01 06:45),
    (from=test_FFM [track=-, scheduled_track=-, level=-3], to=- [track=-, scheduled_track=-, level=0], start=2019-05-01 06:45, mode="WALK", trip="-", end=2019-05-01 06:47)
])",R"(date=2019-05-01, start=14:35, end=14:47, duration=00:12, transfers=0, legs=[
    (from=- [track=-, scheduled_track=-, level=0], to=test_LANGEN [track=-, scheduled_track=-, level=0], start=2019-05-01 14:35, mode="WALK", trip="-", end=2019-05-01 14:35),
    (from=test_LANGEN [track=-, scheduled_track=-, level=0], to=test_FFM [track=-, scheduled_track=-, level=-3], start=2019-05-01 14:35, mode="HIGHSPEED_RAIL", trip="ICE ", end=2019-05-01 14:45),
    (from=test_FFM [track=-, scheduled_track=-, level=-3], to=- [track=-, scheduled_track=-, level=0], start=2019-05-01 14:45, mode="WALK", trip="-", end=2019-05-01 14:47)
])"};
  std::chrono::year_month_day day(std::chrono::year(2019),std::chrono::month(5),std::chrono::day(1));
  unixtime_t start_time = std::chrono::sys_days(day);
  auto end_time = start_time + std::chrono::minutes(1440);
  interval<unixtime_t> interval;
  interval.from_ = start_time;
  interval.to_ = end_time;

  test_route(coordinates,std::string{simple_GTFS},interval,expected_strings,"test/resources/darmstadt_langen_frankfurt.osm.pbf");
}

TEST(motis,route_simple_plan_2) {
  std::chrono::year_month_day day(std::chrono::year(2019),std::chrono::month(5),std::chrono::day(1));
  unixtime_t start_time = std::chrono::sys_days(day);
  auto end_time = start_time + std::chrono::minutes(1440);
  interval<unixtime_t> interval;
  interval.from_ = start_time;
  interval.to_ = end_time;

  std::vector<Coordinates> coordinates{Coordinates(8.63085,49.87260), //DA
  Coordinates(8.66341,50.10701), //FFM
  Coordinates(8.67824,50.11404), //FFM_HAUPT_S
  Coordinates(8.631438,49.872797)}; //self made coordinates close to DA

  auto expected_strings = make_test_strings(coordinates.size());

  expected_strings[0][1] = {R"(date=2019-05-01, start=07:04, end=07:12, duration=00:08, transfers=0, legs=[
    (from=- [track=-, scheduled_track=-, level=0], to=test_DA [track=-, scheduled_track=-, level=0], start=2019-05-01 07:04, mode="WALK", trip="-", end=2019-05-01 07:05),
    (from=test_DA [track=-, scheduled_track=-, level=0], to=test_FFM [track=-, scheduled_track=-, level=-3], start=2019-05-01 07:05, mode="SUBWAY", trip="U4", end=2019-05-01 07:10),
    (from=test_FFM [track=-, scheduled_track=-, level=-3], to=- [track=-, scheduled_track=-, level=0], start=2019-05-01 07:10, mode="WALK", trip="-", end=2019-05-01 07:12)
])",R"(date=2019-05-01, start=15:04, end=15:12, duration=00:08, transfers=0, legs=[
    (from=- [track=-, scheduled_track=-, level=0], to=test_DA [track=-, scheduled_track=-, level=0], start=2019-05-01 15:04, mode="WALK", trip="-", end=2019-05-01 15:05),
    (from=test_DA [track=-, scheduled_track=-, level=0], to=test_FFM [track=-, scheduled_track=-, level=-3], start=2019-05-01 15:05, mode="SUBWAY", trip="U4", end=2019-05-01 15:10),
    (from=test_FFM [track=-, scheduled_track=-, level=-3], to=- [track=-, scheduled_track=-, level=0], start=2019-05-01 15:10, mode="WALK", trip="-", end=2019-05-01 15:12)
])"};

  expected_strings[0][2] = {R"(date=2019-05-01, start=07:04, end=07:22, duration=00:18, transfers=1, legs=[
    (from=- [track=-, scheduled_track=-, level=0], to=test_DA [track=-, scheduled_track=-, level=0], start=2019-05-01 07:04, mode="WALK", trip="-", end=2019-05-01 07:05),
    (from=test_DA [track=-, scheduled_track=-, level=0], to=test_FFM [track=-, scheduled_track=-, level=-3], start=2019-05-01 07:05, mode="SUBWAY", trip="U4", end=2019-05-01 07:10),
    (from=test_FFM [track=-, scheduled_track=-, level=-3], to=test_FFM [track=-, scheduled_track=-, level=-3], start=2019-05-01 07:10, mode="WALK", trip="-", end=2019-05-01 07:12),
    (from=test_FFM [track=-, scheduled_track=-, level=-3], to=test_FFM_HAUPT_S [track=-, scheduled_track=-, level=-3], start=2019-05-01 07:15, mode="METRO", trip="S3", end=2019-05-01 07:20),
    (from=test_FFM_HAUPT_S [track=-, scheduled_track=-, level=-3], to=- [track=-, scheduled_track=-, level=0], start=2019-05-01 07:20, mode="WALK", trip="-", end=2019-05-01 07:22)
])",R"(date=2019-05-01, start=15:04, end=15:22, duration=00:18, transfers=1, legs=[
    (from=- [track=-, scheduled_track=-, level=0], to=test_DA [track=-, scheduled_track=-, level=0], start=2019-05-01 15:04, mode="WALK", trip="-", end=2019-05-01 15:05),
    (from=test_DA [track=-, scheduled_track=-, level=0], to=test_FFM [track=-, scheduled_track=-, level=-3], start=2019-05-01 15:05, mode="SUBWAY", trip="U4", end=2019-05-01 15:10),
    (from=test_FFM [track=-, scheduled_track=-, level=-3], to=test_FFM [track=-, scheduled_track=-, level=-3], start=2019-05-01 15:10, mode="WALK", trip="-", end=2019-05-01 15:12),
    (from=test_FFM [track=-, scheduled_track=-, level=-3], to=test_FFM_HAUPT_S [track=-, scheduled_track=-, level=-3], start=2019-05-01 15:15, mode="METRO", trip="S3", end=2019-05-01 15:20),
    (from=test_FFM_HAUPT_S [track=-, scheduled_track=-, level=-3], to=- [track=-, scheduled_track=-, level=0], start=2019-05-01 15:20, mode="WALK", trip="-", end=2019-05-01 15:22)
])"};

  expected_strings[0][3] = {R"(date=2019-05-01, start=00:00, end=00:01, duration=00:01, transfers=0, legs=[
    (from=- [track=-, scheduled_track=-, level=0], to=- [track=-, scheduled_track=-, level=0], start=2019-05-01 00:00, mode="WALK", trip="-", end=2019-05-01 00:01)
])"};

  expected_strings[1][0] = {R"(date=2019-05-01, start=07:13, end=14:48, duration=07:35, transfers=1, legs=[
    (from=- [track=-, scheduled_track=-, level=0], to=test_FFM [track=-, scheduled_track=-, level=-3], start=2019-05-01 07:13, mode="WALK", trip="-", end=2019-05-01 07:15),
    (from=test_FFM [track=-, scheduled_track=-, level=-3], to=test_FFM_HAUPT_S [track=-, scheduled_track=-, level=-3], start=2019-05-01 07:15, mode="METRO", trip="S3", end=2019-05-01 07:20),
    (from=test_FFM_HAUPT_S [track=-, scheduled_track=-, level=-3], to=test_FFM_HAUPT_S [track=-, scheduled_track=-, level=-3], start=2019-05-01 07:20, mode="WALK", trip="-", end=2019-05-01 07:22),
    (from=test_FFM_HAUPT_S [track=-, scheduled_track=-, level=-3], to=test_DA_10 [track=10, scheduled_track=10, level=-1], start=2019-05-01 14:35, mode="HIGHSPEED_RAIL", trip="ICE ", end=2019-05-01 14:45),
    (from=test_DA_10 [track=10, scheduled_track=10, level=-1], to=- [track=-, scheduled_track=-, level=0], start=2019-05-01 14:45, mode="WALK", trip="-", end=2019-05-01 14:48)
])"};

  expected_strings[1][2] = {R"(date=2019-05-01, start=07:13, end=07:22, duration=00:09, transfers=0, legs=[
    (from=- [track=-, scheduled_track=-, level=0], to=test_FFM [track=-, scheduled_track=-, level=-3], start=2019-05-01 07:13, mode="WALK", trip="-", end=2019-05-01 07:15),
    (from=test_FFM [track=-, scheduled_track=-, level=-3], to=test_FFM_HAUPT_S [track=-, scheduled_track=-, level=-3], start=2019-05-01 07:15, mode="METRO", trip="S3", end=2019-05-01 07:20),
    (from=test_FFM_HAUPT_S [track=-, scheduled_track=-, level=-3], to=- [track=-, scheduled_track=-, level=0], start=2019-05-01 07:20, mode="WALK", trip="-", end=2019-05-01 07:22)
])",
    R"(date=2019-05-01, start=15:13, end=15:22, duration=00:09, transfers=0, legs=[
    (from=- [track=-, scheduled_track=-, level=0], to=test_FFM [track=-, scheduled_track=-, level=-3], start=2019-05-01 15:13, mode="WALK", trip="-", end=2019-05-01 15:15),
    (from=test_FFM [track=-, scheduled_track=-, level=-3], to=test_FFM_HAUPT_S [track=-, scheduled_track=-, level=-3], start=2019-05-01 15:15, mode="METRO", trip="S3", end=2019-05-01 15:20),
    (from=test_FFM_HAUPT_S [track=-, scheduled_track=-, level=-3], to=- [track=-, scheduled_track=-, level=0], start=2019-05-01 15:20, mode="WALK", trip="-", end=2019-05-01 15:22)
])"};

  expected_strings[1][3] = {R"(date=2019-05-01, start=07:13, end=14:49, duration=07:36, transfers=1, legs=[
    (from=- [track=-, scheduled_track=-, level=0], to=test_FFM [track=-, scheduled_track=-, level=-3], start=2019-05-01 07:13, mode="WALK", trip="-", end=2019-05-01 07:15),
    (from=test_FFM [track=-, scheduled_track=-, level=-3], to=test_FFM_HAUPT_S [track=-, scheduled_track=-, level=-3], start=2019-05-01 07:15, mode="METRO", trip="S3", end=2019-05-01 07:20),
    (from=test_FFM_HAUPT_S [track=-, scheduled_track=-, level=-3], to=test_FFM_HAUPT_S [track=-, scheduled_track=-, level=-3], start=2019-05-01 07:20, mode="WALK", trip="-", end=2019-05-01 07:22),
    (from=test_FFM_HAUPT_S [track=-, scheduled_track=-, level=-3], to=test_DA_10 [track=10, scheduled_track=10, level=-1], start=2019-05-01 14:35, mode="HIGHSPEED_RAIL", trip="ICE ", end=2019-05-01 14:45),
    (from=test_DA_10 [track=10, scheduled_track=10, level=-1], to=- [track=-, scheduled_track=-, level=0], start=2019-05-01 14:45, mode="WALK", trip="-", end=2019-05-01 14:49)
])"};

  expected_strings[2][0] = {R"(date=2019-05-01, start=06:33, end=06:48, duration=00:15, transfers=0, legs=[
    (from=- [track=-, scheduled_track=-, level=0], to=test_FFM_HAUPT_S [track=-, scheduled_track=-, level=-3], start=2019-05-01 06:33, mode="WALK", trip="-", end=2019-05-01 06:35),
    (from=test_FFM_HAUPT_S [track=-, scheduled_track=-, level=-3], to=test_DA_10 [track=10, scheduled_track=10, level=-1], start=2019-05-01 06:35, mode="HIGHSPEED_RAIL", trip="ICE ", end=2019-05-01 06:45),
    (from=test_DA_10 [track=10, scheduled_track=10, level=-1], to=- [track=-, scheduled_track=-, level=0], start=2019-05-01 06:45, mode="WALK", trip="-", end=2019-05-01 06:48)
])",R"(date=2019-05-01, start=14:33, end=14:48, duration=00:15, transfers=0, legs=[
    (from=- [track=-, scheduled_track=-, level=0], to=test_FFM_HAUPT_S [track=-, scheduled_track=-, level=-3], start=2019-05-01 14:33, mode="WALK", trip="-", end=2019-05-01 14:35),
    (from=test_FFM_HAUPT_S [track=-, scheduled_track=-, level=-3], to=test_DA_10 [track=10, scheduled_track=10, level=-1], start=2019-05-01 14:35, mode="HIGHSPEED_RAIL", trip="ICE ", end=2019-05-01 14:45),
    (from=test_DA_10 [track=10, scheduled_track=10, level=-1], to=- [track=-, scheduled_track=-, level=0], start=2019-05-01 14:45, mode="WALK", trip="-", end=2019-05-01 14:48)
])"};

  expected_strings[2][1] = {R"(date=2019-05-01, start=06:33, end=07:12, duration=00:39, transfers=1, legs=[
    (from=- [track=-, scheduled_track=-, level=0], to=test_FFM_HAUPT_S [track=-, scheduled_track=-, level=-3], start=2019-05-01 06:33, mode="WALK", trip="-", end=2019-05-01 06:35),
    (from=test_FFM_HAUPT_S [track=-, scheduled_track=-, level=-3], to=test_DA_10 [track=10, scheduled_track=10, level=-1], start=2019-05-01 06:35, mode="HIGHSPEED_RAIL", trip="ICE ", end=2019-05-01 06:45),
    (from=test_DA_10 [track=10, scheduled_track=10, level=-1], to=test_DA [track=-, scheduled_track=-, level=0], start=2019-05-01 06:45, mode="WALK", trip="-", end=2019-05-01 06:49),
    (from=test_DA [track=-, scheduled_track=-, level=0], to=test_FFM [track=-, scheduled_track=-, level=-3], start=2019-05-01 07:05, mode="SUBWAY", trip="U4", end=2019-05-01 07:10),
    (from=test_FFM [track=-, scheduled_track=-, level=-3], to=- [track=-, scheduled_track=-, level=0], start=2019-05-01 07:10, mode="WALK", trip="-", end=2019-05-01 07:12)
])",R"(date=2019-05-01, start=14:33, end=15:12, duration=00:39, transfers=1, legs=[
    (from=- [track=-, scheduled_track=-, level=0], to=test_FFM_HAUPT_S [track=-, scheduled_track=-, level=-3], start=2019-05-01 14:33, mode="WALK", trip="-", end=2019-05-01 14:35),
    (from=test_FFM_HAUPT_S [track=-, scheduled_track=-, level=-3], to=test_DA_10 [track=10, scheduled_track=10, level=-1], start=2019-05-01 14:35, mode="HIGHSPEED_RAIL", trip="ICE ", end=2019-05-01 14:45),
    (from=test_DA_10 [track=10, scheduled_track=10, level=-1], to=test_DA [track=-, scheduled_track=-, level=0], start=2019-05-01 14:45, mode="WALK", trip="-", end=2019-05-01 14:49),
    (from=test_DA [track=-, scheduled_track=-, level=0], to=test_FFM [track=-, scheduled_track=-, level=-3], start=2019-05-01 15:05, mode="SUBWAY", trip="U4", end=2019-05-01 15:10),
    (from=test_FFM [track=-, scheduled_track=-, level=-3], to=- [track=-, scheduled_track=-, level=0], start=2019-05-01 15:10, mode="WALK", trip="-", end=2019-05-01 15:12)
])"};
  expected_strings[2][3] = {R"(date=2019-05-01, start=06:33, end=06:49, duration=00:16, transfers=0, legs=[
    (from=- [track=-, scheduled_track=-, level=0], to=test_FFM_HAUPT_S [track=-, scheduled_track=-, level=-3], start=2019-05-01 06:33, mode="WALK", trip="-", end=2019-05-01 06:35),
    (from=test_FFM_HAUPT_S [track=-, scheduled_track=-, level=-3], to=test_DA_10 [track=10, scheduled_track=10, level=-1], start=2019-05-01 06:35, mode="HIGHSPEED_RAIL", trip="ICE ", end=2019-05-01 06:45),
    (from=test_DA_10 [track=10, scheduled_track=10, level=-1], to=- [track=-, scheduled_track=-, level=0], start=2019-05-01 06:45, mode="WALK", trip="-", end=2019-05-01 06:49)
])",R"(date=2019-05-01, start=14:33, end=14:49, duration=00:16, transfers=0, legs=[
    (from=- [track=-, scheduled_track=-, level=0], to=test_FFM_HAUPT_S [track=-, scheduled_track=-, level=-3], start=2019-05-01 14:33, mode="WALK", trip="-", end=2019-05-01 14:35),
    (from=test_FFM_HAUPT_S [track=-, scheduled_track=-, level=-3], to=test_DA_10 [track=10, scheduled_track=10, level=-1], start=2019-05-01 14:35, mode="HIGHSPEED_RAIL", trip="ICE ", end=2019-05-01 14:45),
    (from=test_DA_10 [track=10, scheduled_track=10, level=-1], to=- [track=-, scheduled_track=-, level=0], start=2019-05-01 14:45, mode="WALK", trip="-", end=2019-05-01 14:49)
])"};
  expected_strings[3][0] = {R"(date=2019-05-01, start=00:00, end=00:01, duration=00:01, transfers=0, legs=[
    (from=- [track=-, scheduled_track=-, level=0], to=- [track=-, scheduled_track=-, level=0], start=2019-05-01 00:00, mode="WALK", trip="-", end=2019-05-01 00:01)
])"};
  expected_strings[3][1] = {R"(date=2019-05-01, start=07:04, end=07:12, duration=00:08, transfers=0, legs=[
    (from=- [track=-, scheduled_track=-, level=0], to=test_DA [track=-, scheduled_track=-, level=0], start=2019-05-01 07:04, mode="WALK", trip="-", end=2019-05-01 07:05),
    (from=test_DA [track=-, scheduled_track=-, level=0], to=test_FFM [track=-, scheduled_track=-, level=-3], start=2019-05-01 07:05, mode="SUBWAY", trip="U4", end=2019-05-01 07:10),
    (from=test_FFM [track=-, scheduled_track=-, level=-3], to=- [track=-, scheduled_track=-, level=0], start=2019-05-01 07:10, mode="WALK", trip="-", end=2019-05-01 07:12)
])",
    R"(date=2019-05-01, start=15:04, end=15:12, duration=00:08, transfers=0, legs=[
    (from=- [track=-, scheduled_track=-, level=0], to=test_DA [track=-, scheduled_track=-, level=0], start=2019-05-01 15:04, mode="WALK", trip="-", end=2019-05-01 15:05),
    (from=test_DA [track=-, scheduled_track=-, level=0], to=test_FFM [track=-, scheduled_track=-, level=-3], start=2019-05-01 15:05, mode="SUBWAY", trip="U4", end=2019-05-01 15:10),
    (from=test_FFM [track=-, scheduled_track=-, level=-3], to=- [track=-, scheduled_track=-, level=0], start=2019-05-01 15:10, mode="WALK", trip="-", end=2019-05-01 15:12)
])"};
  expected_strings[3][2] = {R"(date=2019-05-01, start=07:04, end=07:22, duration=00:18, transfers=1, legs=[
    (from=- [track=-, scheduled_track=-, level=0], to=test_DA [track=-, scheduled_track=-, level=0], start=2019-05-01 07:04, mode="WALK", trip="-", end=2019-05-01 07:05),
    (from=test_DA [track=-, scheduled_track=-, level=0], to=test_FFM [track=-, scheduled_track=-, level=-3], start=2019-05-01 07:05, mode="SUBWAY", trip="U4", end=2019-05-01 07:10),
    (from=test_FFM [track=-, scheduled_track=-, level=-3], to=test_FFM [track=-, scheduled_track=-, level=-3], start=2019-05-01 07:10, mode="WALK", trip="-", end=2019-05-01 07:12),
    (from=test_FFM [track=-, scheduled_track=-, level=-3], to=test_FFM_HAUPT_S [track=-, scheduled_track=-, level=-3], start=2019-05-01 07:15, mode="METRO", trip="S3", end=2019-05-01 07:20),
    (from=test_FFM_HAUPT_S [track=-, scheduled_track=-, level=-3], to=- [track=-, scheduled_track=-, level=0], start=2019-05-01 07:20, mode="WALK", trip="-", end=2019-05-01 07:22)
])",R"(date=2019-05-01, start=15:04, end=15:22, duration=00:18, transfers=1, legs=[
    (from=- [track=-, scheduled_track=-, level=0], to=test_DA [track=-, scheduled_track=-, level=0], start=2019-05-01 15:04, mode="WALK", trip="-", end=2019-05-01 15:05),
    (from=test_DA [track=-, scheduled_track=-, level=0], to=test_FFM [track=-, scheduled_track=-, level=-3], start=2019-05-01 15:05, mode="SUBWAY", trip="U4", end=2019-05-01 15:10),
    (from=test_FFM [track=-, scheduled_track=-, level=-3], to=test_FFM [track=-, scheduled_track=-, level=-3], start=2019-05-01 15:10, mode="WALK", trip="-", end=2019-05-01 15:12),
    (from=test_FFM [track=-, scheduled_track=-, level=-3], to=test_FFM_HAUPT_S [track=-, scheduled_track=-, level=-3], start=2019-05-01 15:15, mode="METRO", trip="S3", end=2019-05-01 15:20),
    (from=test_FFM_HAUPT_S [track=-, scheduled_track=-, level=-3], to=- [track=-, scheduled_track=-, level=0], start=2019-05-01 15:20, mode="WALK", trip="-", end=2019-05-01 15:22)
])"};

  test_route(coordinates,std::string{simple_GTFS2},interval,expected_strings,"test/resources/test_case.osm.pbf");
}
}// namespace origin_destination_matrix