#pragma once

#include <tuple>
#include <vector>

#include "utl/verify.h"

#include "motis/raptor/raptor_util.h"
#include "motis/raptor/types.h"

#if defined(MOTIS_CUDA)
#include "cooperative_groups.h"
#endif

namespace motis {
struct journey;
}

namespace motis::raptor {

template <typename... TraitData>
struct raptor_data : public TraitData... {
  route_id route_id_;
  trip_id trip_id_;

  trip_id departure_trait_id_;
};

template <typename... Trait>
struct traits;

template <typename FirstTrait, typename... RestTraits>
struct traits<FirstTrait, RestTraits...> {
  using TraitsData = raptor_data<FirstTrait, RestTraits...>;

  _mark_cuda_rel_ inline static trait_id size() {
    auto size = FirstTrait::index_range_size();
    return size * traits<RestTraits...>::size();
  }

  _mark_cuda_rel_ inline static trait_id get_write_to_trait_id(TraitsData& d) {
    auto const first_dimension_idx = FirstTrait::get_write_to_dimension_id(d);
    if (!valid(first_dimension_idx)) {
      return invalid<trait_id>;
    }

    auto const first_dim_step_width = traits<RestTraits...>::size();

    auto const rest_trait_offset =
        traits<RestTraits...>::get_write_to_trait_id(d);

    auto write_idx =
        first_dim_step_width * first_dimension_idx + rest_trait_offset;
    return write_idx;
  }

  _mark_cuda_rel_ inline static bool is_trait_satisfied(trait_id total_size,
                                                        TraitsData const& td,
                                                        trait_id t_offset) {
    auto const [rest_trait_size, first_dimension_idx, rest_trait_offset] =
        _trait_values(total_size, t_offset);

    return FirstTrait::is_trait_satisfied(td, first_dimension_idx) &&
           traits<RestTraits...>::is_trait_satisfied(rest_trait_size, td,
                                                     rest_trait_offset);
  }

  //****************************************************************************
  // Only used by CPU RAPTOR
  inline static bool is_rescan_from_stop_needed(trait_id total_size,
                                                TraitsData const& td,
                                                trait_id t_offset) {
    auto const [rest_trait_size, first_dimension_idx, rest_trait_offset] =
        _trait_values(total_size, t_offset);

    return FirstTrait::is_rescan_from_stop_needed(td, first_dimension_idx) ||
           traits<RestTraits...>::is_rescan_from_stop_needed(
               rest_trait_size, td, rest_trait_offset);
  }

  inline static bool is_forward_propagation_required() {
    return FirstTrait::is_forward_propagation_required() ||
           traits<RestTraits...>::is_forward_propagation_required();
  }
  //****************************************************************************

  // helper to aggregate values while progressing through the route stop by stop
  template <typename Timetable>
  _mark_cuda_rel_ inline static void update_aggregate(
      TraitsData& aggregate_dt, Timetable const& tt,
      time const* const previous_arrivals, stop_offset const s_offset,
      stop_times_index const current_sti, trait_id const total_trait_size) {

    FirstTrait::update_aggregate(aggregate_dt, tt, previous_arrivals, s_offset,
                                 current_sti, total_trait_size);

    traits<RestTraits...>::update_aggregate(aggregate_dt, tt, previous_arrivals,
                                            s_offset, current_sti,
                                            total_trait_size);
  }

  // reset the aggregate everytime the departure station changes
  _mark_cuda_rel_ inline static void reset_aggregate(
      trait_id const total_size, TraitsData& aggregate_dt,
      trait_id const initial_t_offset) {
    auto const [rest_trait_size, first_dimension_idx, rest_trait_offset] =
        _trait_values(total_size, initial_t_offset);

    FirstTrait::reset_aggregate(aggregate_dt, first_dimension_idx);
    traits<RestTraits...>::reset_aggregate(rest_trait_size, aggregate_dt,
                                           rest_trait_offset);
  }

#if defined(MOTIS_CUDA)
  __device__ inline static void propagate_and_merge_if_needed(
      unsigned const mask, TraitsData& aggregate, bool const predicate) {
    FirstTrait::propagate_and_merge_if_needed(aggregate, mask, predicate);
    traits<RestTraits...>::propagate_and_merge_if_needed(mask, aggregate,
                                                         predicate);
  }

  __device__ inline static void carry_to_next_stage(unsigned const mask,
                                                    TraitsData& aggregate) {
    FirstTrait::carry_to_next_stage(mask, aggregate);
    traits<RestTraits...>::carry_to_next_stage(mask, aggregate);
  }

#endif

  inline static std::vector<trait_id> get_feasible_trait_ids(
      trait_id const total_size, trait_id const initial_offset,
      TraitsData const& input) {

    auto const [rest_trait_size, first_dimension_idx, rest_trait_offset] =
        _trait_values(total_size, initial_offset);

    auto const first_dimensions =
        FirstTrait::get_feasible_dimensions(first_dimension_idx, input);
    auto const first_size = traits<RestTraits...>::size();
    auto const rest_feasible = traits<RestTraits...>::get_feasible_trait_ids(
        rest_trait_size, rest_trait_offset, input);

    std::vector<trait_id> total_feasible(first_dimensions.size() *
                                         rest_feasible.size());

    auto idx = 0;
    for (auto const first_dim : first_dimensions) {
      for (auto const rest_offset : rest_feasible) {
        total_feasible[idx] = first_dim * first_size + rest_offset;
        ++idx;
      }
    }

    return total_feasible;
  }

  inline static bool dominates(trait_id const total_size,
                               trait_id const to_dominate,
                               trait_id const dominating) {
    auto const [rest_size_todom, first_dim_todom, rest_trait_todom] =
        _trait_values(total_size, to_dominate);

    auto const [rest_size_domin, first_dim_domin, rest_trait_domin] =
        _trait_values(total_size, dominating);

    utl::verify_ex(rest_size_todom == rest_size_domin,
                   "Trait derivation failed!");

    return FirstTrait::dominates(first_dim_todom, first_dim_domin) &&
           traits<RestTraits...>::dominates(rest_size_domin, rest_trait_todom,
                                            rest_trait_domin);
  }

  inline static void fill_journey(trait_id const total_size, journey& journey,
                                  trait_id const t_offset) {
    auto const [rest_size, first_dimension, rest_offset] =
        _trait_values(total_size, t_offset);
    FirstTrait::fill_journey(journey, first_dimension);
    traits<RestTraits...>::fill_journey(rest_size, journey, rest_offset);
  }

private:
  _mark_cuda_rel_ inline static std::tuple<trait_id, dimension_id, trait_id>
  _trait_values(trait_id const total_size, trait_id const t_offset) {
    auto const first_value_size = FirstTrait::index_range_size();
    auto const rest_trait_size = total_size / first_value_size;

    auto const first_dimension_idx = t_offset / rest_trait_size;
    auto const rest_trait_offset = t_offset % rest_trait_size;

    return std::make_tuple(rest_trait_size, first_dimension_idx,
                           rest_trait_offset);
  }
};

template <>
struct traits<> {
  using TraitsData = raptor_data<>;

  _mark_cuda_rel_ inline static uint32_t size() { return 1; }

  template <typename Data>
  _mark_cuda_rel_ inline static trait_id get_write_to_trait_id(Data const& d) {
    return 0;
  }

  template <typename Data>
  _mark_cuda_rel_ inline static bool is_trait_satisfied(uint32_t _1,
                                                        Data const& _2,
                                                        uint32_t _3) {
    return true;  // return natural element of conjunction
  }

  template <typename Data>
  inline static bool is_rescan_from_stop_needed(uint32_t _1, Data const& _2,
                                                uint32_t _3) {
    return false;
  }

  inline static bool is_forward_propagation_required() {
    return false; //return natural element
  }

  template <typename Data, typename Timetable>
  _mark_cuda_rel_ inline static void update_aggregate(
      Data& aggregate_dt, Timetable const& tt,
      time const* const previous_arrivals, stop_offset const s_offset,
      stop_times_index const current_sti, trait_id const total_trait_size) {}

  template <typename Data>
  _mark_cuda_rel_ inline static void reset_aggregate(
      trait_id const total_size, Data& aggregate_dt,
      trait_id const initial_t_offset) {}

#if defined(MOTIS_CUDA)
  template <typename Data>
  __device__ inline static void propagate_and_merge_if_needed(unsigned const _1,
                                                              Data& _2,
                                                              bool const _3) {}

  template <typename Data>
  __device__ inline static void carry_to_next_stage(unsigned const _1,
                                                    Data& _2) {}
#endif

  template <typename Data>
  inline static std::vector<trait_id> get_feasible_trait_ids(
      trait_id const total_size, trait_id const initial_offset,
      Data const& input) {
    return std::vector<trait_id>{0};
  }

  // giving the neutral element of the conjunction
  inline static bool dominates(trait_id const _0, trait_id const& _1,
                               trait_id const& _2) {
    return true;
  }

  inline static void fill_journey(trait_id const _0, journey& _1,
                                  trait_id const _2) {}
};

}  // namespace motis::raptor
