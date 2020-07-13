/*
    @@@@@@@@  @@           @@@@@@   @@@@@@@@ @@
   /@@/////  /@@          @@////@@ @@////// /@@
   /@@       /@@  @@@@@  @@    // /@@       /@@
   /@@@@@@@  /@@ @@///@@/@@       /@@@@@@@@@/@@
   /@@////   /@@/@@@@@@@/@@       ////////@@/@@
   /@@       /@@/@@//// //@@    @@       /@@/@@
   /@@       @@@//@@@@@@ //@@@@@@  @@@@@@@@ /@@
   //       ///  //////   //////  ////////  //

   Copyright (c) 2016, Triad National Security, LLC
   All rights reserved.
                                                                              */
#pragma once

/*! @file */

#include <flecsi-config.h>

#if !defined(__FLECSI_PRIVATE__)
#error Do not include this file directly!
#endif

#include "flecsi/exec/launch.hh"
#include "flecsi/exec/leg/future.hh"
#include "flecsi/exec/leg/reduction_wrapper.hh"
#include "flecsi/exec/leg/task_prologue.hh"
#include "flecsi/exec/leg/task_wrapper.hh"
#include "flecsi/run/backend.hh"
#include "flecsi/util/demangle.hh"
#include "flecsi/util/function_traits.hh"
#include <flecsi/flog.hh>

#include <functional>
#include <memory>
#include <type_traits>

#if !defined(FLECSI_ENABLE_LEGION)
#error FLECSI_ENABLE_LEGION not defined! This file depends on Legion!
#endif

#include <legion.h>

namespace flecsi {

inline log::devel_tag execution_tag("execution");

namespace exec {
namespace detail {

// Remove const from under a reference, if there is one.
template<class T>
struct nonconst_ref {
  using type = T;
};

template<class T>
struct nonconst_ref<const T &> {
  using type = T &;
};

template<class T>
using nonconst_ref_t = typename nonconst_ref<T>::type;

// Serialize a tuple of converted arguments (or references to existing
// arguments where possible).  Note that is_constructible_v<const
// float&,const double&> is true, so we have to check
// is_constructible_v<float&,double&> instead.
template<class... PP, class... AA>
auto
serial_arguments(std::tuple<PP...> * /* to deduce PP */, AA &&... aa) {
  static_assert((std::is_const_v<std::remove_reference_t<const PP>> && ...),
    "Tasks cannot accept non-const references");
  return util::serial_put<std::tuple<std::conditional_t<
    std::is_constructible_v<nonconst_ref_t<PP> &, nonconst_ref_t<AA>>,
    const PP &,
    std::decay_t<PP>>...>>(
    {exec::replace_argument<PP>(std::forward<AA>(aa))...});
}

// Helper to deduce PP:
template<class... PP, class... AA>
void
mpi_arguments(std::optional<std::tuple<PP...>> & opt, AA &&... aa) {
  opt.emplace(exec::replace_argument<PP>(std::forward<AA>(aa))...);
}

template<class, class>
struct tuple_prepend;
template<class T, class... TT>
struct tuple_prepend<T, std::tuple<TT...>> {
  using type = std::tuple<T, TT...>;
};

} // namespace detail
} // namespace exec

template<auto & F, class REDUCTION, size_t ATTRIBUTES, typename... ARGS>
decltype(auto)
reduce(ARGS &&... args) {
  using namespace Legion;
  using namespace exec;

  using traits_t = util::function_traits<decltype(F)>;
  using RETURN = typename traits_t::return_type;
  using param_tuple = typename traits_t::arguments_type;

  // This will guard the entire method
  log::devel_guard guard(execution_tag);

  // Get the FleCSI runtime context
  auto & flecsi_context = run::context::instance();

  // Get the processor type.
  constexpr auto processor_type = mask_to_processor_type(ATTRIBUTES);

  // Get the Legion runtime and context from the current task.
  auto legion_runtime = Legion::Runtime::get_runtime();
  auto legion_context = Legion::Runtime::get_context();

#if defined(FLECSI_ENABLE_FLOG)
  const size_t tasks_executed = flecsi_context.tasks_executed();
  if((tasks_executed > 0) &&
     (tasks_executed % FLOG_SERIALIZATION_INTERVAL == 0)) {

    size_t processes = flecsi_context.processes();
    LegionRuntime::Arrays::Rect<1> launch_bounds(
      LegionRuntime::Arrays::Point<1>(0),
      LegionRuntime::Arrays::Point<1>(processes - 1));
    Domain launch_domain = Domain::from_rect<1>(launch_bounds);

    constexpr auto red = [] {
      return log::flog_t::instance().packets().size();
    };
    Legion::ArgumentMap arg_map;
    Legion::IndexLauncher reduction_launcher(leg::task_id<leg::verb<*red>>,
      launch_domain,
      Legion::TaskArgument(NULL, 0),
      arg_map);

    Legion::Future future = legion_runtime->execute_index_space(
      legion_context, reduction_launcher, reduction_op<fold::max<std::size_t>>);

    if(future.get_result<size_t>() > FLOG_SERIALIZATION_THRESHOLD) {
      constexpr auto send = [] {
        run::context::instance().set_mpi_task(log::send_to_one);
      };
      Legion::IndexLauncher flog_mpi_launcher(leg::task_id<leg::verb<*send>>,
        launch_domain,
        Legion::TaskArgument(NULL, 0),
        arg_map);

      flog_mpi_launcher.tag = run::FLECSI_MAPPER_FORCE_RANK_MATCH;

      // Launch the MPI task
      auto future_mpi =
        legion_runtime->execute_index_space(legion_context, flog_mpi_launcher);

      // Force synchronization
      future_mpi.wait_all_results(true);

      // Handoff to the MPI runtime.
      flecsi_context.handoff_to_mpi(legion_context, legion_runtime);

      // Wait for MPI to finish execution (synchronous).
      flecsi_context.wait_on_mpi(legion_context, legion_runtime);
    } // if
  } // if
#endif // FLECSI_ENABLE_FLOG

  const auto domain_size = [&args..., &flecsi_context] {
    if constexpr(processor_type == task_processor_type_t::mpi) {
      return launch_size<
        typename detail::tuple_prepend<launch_domain, param_tuple>::type>(
        launch_domain{flecsi_context.processes()}, args...);
    }
    else {
      (void)flecsi_context;
      return launch_size<param_tuple>(args...);
    }
  }();

  ++flecsi_context.tasks_executed();

  leg::task_prologue_t pro;
  pro.walk<param_tuple>(args...);

  std::optional<param_tuple> mpi_args;
  std::vector<std::byte> buf;
  if constexpr(processor_type == task_processor_type_t::mpi) {
    // MPI tasks must be invoked collectively from one task on each rank.
    // We therefore can transmit merely a pointer to a tuple of the arguments.
    // util::serial_put deliberately doesn't support this, so just memcpy it.
    detail::mpi_arguments(mpi_args, std::forward<ARGS>(args)...);
    const auto p = &*mpi_args;
    buf.resize(sizeof p);
    std::memcpy(buf.data(), &p, sizeof p);
  }
  else {
    buf = detail::serial_arguments(
      static_cast<param_tuple *>(nullptr), std::forward<ARGS>(args)...);
  }

  //------------------------------------------------------------------------//
  // Single launch
  //------------------------------------------------------------------------//

  using wrap = leg::task_wrapper<F, processor_type>;
  const auto task = leg::task_id<wrap::execute,
    (ATTRIBUTES & ~mpi) | 1 << static_cast<std::size_t>(wrap::LegionProcessor)>;

  if constexpr(std::is_same_v<decltype(domain_size), const std::monostate>) {
    {
      log::devel_guard guard(execution_tag);
      flog_devel(info) << "Executing single task" << std::endl;
    }

    TaskLauncher launcher(task, TaskArgument(buf.data(), buf.size()));

    // adding region requirements to the launcher
    for(auto & req : pro.region_requirements()) {
      launcher.add_region_requirement(req);
    } // for

    // adding futures to the launcher
    launcher.futures = std::move(pro).futures();

    static_assert(processor_type == task_processor_type_t::toc ||
                    processor_type == task_processor_type_t::loc,
      "Unknown launch type");
    return future<RETURN>{
      legion_runtime->execute_task(legion_context, launcher)};
  }

  //------------------------------------------------------------------------//
  // Index launch
  //------------------------------------------------------------------------//

  else {

    {
      log::devel_guard guard(execution_tag);
      flog_devel(info) << "Executing index task" << std::endl;
    }

    LegionRuntime::Arrays::Rect<1> launch_bounds(
      LegionRuntime::Arrays::Point<1>(0),
      LegionRuntime::Arrays::Point<1>(domain_size - 1));
    Domain launch_domain = Domain::from_rect<1>(launch_bounds);

    Legion::ArgumentMap arg_map;
    Legion::IndexLauncher launcher(
      task, launch_domain, TaskArgument(buf.data(), buf.size()), arg_map);

    // adding region requirement to the launcher
    for(auto & req : pro.region_requirements()) {
      launcher.add_region_requirement(req);
    } // for

    // adding futures to the launcher
    launcher.futures = std::move(pro).futures();
    launcher.point_futures.assign(
      pro.future_maps().begin(), pro.future_maps().end());

    if constexpr(processor_type == task_processor_type_t::toc ||
                 processor_type == task_processor_type_t::loc) {
      flog_devel(info) << "Executing index launch on loc" << std::endl;

      if constexpr(!std::is_void_v<REDUCTION>) {
        flog_devel(info) << "executing reduction logic for "
                         << util::type<REDUCTION>() << std::endl;

        return future<RETURN>{legion_runtime->execute_index_space(
          legion_context, launcher, reduction_op<REDUCTION>)};
      }
      else {
        // Enqueue the task.
        Legion::FutureMap future_map =
          legion_runtime->execute_index_space(legion_context, launcher);

        return future<RETURN, launch_type_t::index>{future_map};
      } // else
    }
    else {
      static_assert(
        processor_type == task_processor_type_t::mpi, "Unknown launch type");
      launcher.tag = run::FLECSI_MAPPER_FORCE_RANK_MATCH;

      // Launch the MPI task
      const auto ret = future<RETURN, launch_type_t::index>{
        legion_runtime->execute_index_space(legion_context, launcher)};
      // Force synchronization
      ret.wait(true);

      // Handoff to the MPI runtime.
      flecsi_context.handoff_to_mpi(legion_context, legion_runtime);

      // Wait for MPI to finish execution (synchronous).
      // We must keep mpi_args alive until then.
      flecsi_context.wait_on_mpi(legion_context, legion_runtime);

      if constexpr(!std::is_void_v<REDUCTION>) {
        // FIXME implement logic for reduction MPI task
        flog_fatal("there is no implementation for the mpi"
                   " reduction task");
      }
      return ret;
    }
  } // if constexpr

  // return 0;
} // execute_task

} // namespace flecsi
