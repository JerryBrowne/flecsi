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

#if !defined(__FLECSI_PRIVATE__)
#error Do not include this file directly!
#else
#include <flecsi/execution/common/launch.h>
#include <flecsi/execution/common/processor.h>
#include <flecsi/execution/context.h>
#include <flecsi/execution/legion/enactment/finalize_views.h>
#include <flecsi/execution/legion/enactment/init_views.h>
#include <flecsi/utils/common.h>
#include <flecsi/utils/flog.h>
#include <flecsi/utils/tuple_function.h>
#include <flecsi/utils/tuple_type_converter.h>
#endif

#include <flecsi-config.h>

#if !defined(FLECSI_ENABLE_LEGION)
#error FLECSI_ENABLE_LEGION not defined! This file depends on Legion!
#endif

#include <legion.h>

#include <string>

flog_register_tag(task_wrapper);

namespace flecsi {
namespace execution {
namespace legion {

/*!
  Pure Legion task wrapper.

  @tparam RETURN The return type of the task.
  @tparam TASK   The legion task.

  @ingroup legion-execution
 */

template<typename RETURN,
  RETURN (*TASK)(const Legion::Task *,
    const std::vector<Legion::PhysicalRegion> &,
    Legion::Context,
    Legion::Runtime *)>
struct pure_task_wrapper_u {

  /*!
    The task_id_t type is a unique identifier for Legion tasks.
   */

  using task_id_t = Legion::TaskID;

  /*!
   Registration callback function for pure Legion tasks.

   @param tid The task id to assign to the task.
   @param processor_type A valid Legion processor type.
   @param execution  A \ref task_execution_type_t with the task
              execution type parameters.
   @param A std::string containing the task name.
   */

  static void registration_callback(task_id_t tid,
    processor_type_t processor_type,
    task_execution_type_t execution,
    std::string & name) {
    {
      flog_tag_guard(task_wrapper);
      flog(info) << "registering pure Legion task " << name << std::endl;
    }

    Legion::TaskVariantRegistrar registrar(tid, name.c_str());
    Legion::Processor::Kind kind = processor_type == processor_type_t::toc
                                     ? Legion::Processor::TOC_PROC
                                     : Legion::Processor::LOC_PROC;
    registrar.add_constraint(Legion::ProcessorConstraint(kind));
    registrar.set_leaf(task_leaf(execution));
    registrar.set_inner(task_inner(execution));
    registrar.set_idempotent(task_idempotent(execution));

    /*
      This section of conditionals is necessary because there is still
      a distinction between void and non-void task registration with
      Legion, and we still have to use different execution tasks for
      normal and MPI tasks. MPI tasks are required to have void returns,
      so there is no mpi case for non-void tasks.
     */

    if constexpr(std::is_same_v<RETURN, void>) {
      if(processor_type == processor_type_t::mpi) {
        flog_fatal("MPI type passed to pure task registration");
      }
      else {
        Legion::Runtime::preregister_task_variant<TASK>(
          registrar, name.c_str());
      } // if
    }
    else {
      Legion::Runtime::preregister_task_variant<RETURN, TASK>(
        registrar, name.c_str());
    } // if
  } // registration_callback

}; // struct pure_task_wrapper_u

/*!
 The task_wrapper_u type provides registation callback and execution
 functions for user and MPI tasks.

 @tparam RETURN    The return type of the user task.
 @tparam ARG_TUPLE A std::tuple of the user task arguments.
 @tparam DELEGATE  The delegate function that invokes the user task.
 @tparam KEY       A hash key identifying the task.

 @ingroup legion-execution
 */

template<size_t KEY,
  typename RETURN,
  typename ARG_TUPLE,
  RETURN (*DELEGATE)(ARG_TUPLE)>
struct task_wrapper_u {

  /*!
    The task_id_t type is a unique identifier for Legion tasks.
   */

  using task_id_t = Legion::TaskID;

  /*!
   Registration callback function for user tasks.

   @param tid            The task id to assign to the task.
   @param processor_type A \ref processor_type_t with the processor type.
   @param execution      A \ref task_executin_type_t with the task
                          execution type parameters.
   @param name           A std::string containing the task name.
   */

  static void registration_callback(task_id_t tid,
    processor_type_t processor_type,
    task_execution_type_t execution,
    std::string & name) {
    {
      flog_tag_guard(task_wrapper);
      flog(info) << "registering task " << name << std::endl;
    }

    Legion::TaskVariantRegistrar registrar(tid, name.c_str());
    Legion::Processor::Kind kind = processor_type == processor_type_t::toc
                                     ? Legion::Processor::TOC_PROC
                                     : Legion::Processor::LOC_PROC;
    registrar.add_constraint(Legion::ProcessorConstraint(kind));
    registrar.set_leaf(task_leaf(execution));
    registrar.set_inner(task_inner(execution));
    registrar.set_idempotent(task_idempotent(execution));

    /*
      This section of conditionals is necessary because there is still
      a distinction between void and non-void task registration with
      Legion, and we still have to use different execution tasks for
      normal and MPI tasks. MPI tasks are required to have void returns,
      so there is no mpi case for non-void tasks.
     */

    if constexpr(std::is_same_v<RETURN, void>) {
      if(processor_type == processor_type_t::mpi) {
        Legion::Runtime::preregister_task_variant<execute_mpi_task>(
          registrar, name.c_str());
      }
      else {
        Legion::Runtime::preregister_task_variant<execute_user_task>(
          registrar, name.c_str());
      } // if
    }
    else {
      Legion::Runtime::preregister_task_variant<RETURN, execute_user_task>(
        registrar, name.c_str());
    } // if
  } // registration_callback

  /*!
    Execution wrapper method for user tasks.
   */

  static RETURN execute_user_task(const Legion::Task * task,
    const std::vector<Legion::PhysicalRegion> & regions,
    Legion::Context context,
    Legion::Runtime * runtime) {
    {
      flog_tag_guard(task_wrapper);
      flog(info) << "In execute_user_task" << std::endl;
    }

    // Unpack task arguments
    ARG_TUPLE & task_args = *(reinterpret_cast<ARG_TUPLE *>(task->args));

    init_views_t init_views(runtime, context, regions, task->futures);
    init_views.walk(task_args);

    if constexpr(std::is_same_v<RETURN, void>) {
      (*DELEGATE)(std::forward<ARG_TUPLE>(task_args));

      // FIXME: Refactor
      // finalize_handles_t finalize_handles;
      // finalize_handles.walk(task_args);
    }
    else {
      RETURN result = (*DELEGATE)(std::forward<ARG_TUPLE>(task_args));

      // FIXME: Refactor
      // finalize_handles_t finalize_handles;
      // finalize_handles.walk(task_args);

      return result;
    } // if
  } // execute_user_task

  /*!
    Execution wrapper method for MPI tasks.
   */

  static void execute_mpi_task(const Legion::Task * task,
    const std::vector<Legion::PhysicalRegion> & regions,
    Legion::Context context,
    Legion::Runtime * runtime) {
    // FIXME: Refactor
    //    {
    //      flog_tag_guard(task_wrapper);
    //      flog(info) << "In execute_mpi_task" << std::endl;
    //    }

    // Unpack task arguments.
    ARG_TUPLE & mpi_task_args = *(reinterpret_cast<ARG_TUPLE *>(task->args));

    // FIXME: Refactor
    // init_handles_t init_handles(runtime, context, regions, task->futures);
    // init_handles.walk(mpi_task_args);

    // Create bound function to pass to MPI runtime.
    std::function<void()> bound_mpi_task = std::bind(DELEGATE, mpi_task_args);

    // Set the MPI function and make the runtime active.
    context_t::instance().set_mpi_task(bound_mpi_task);
    context_t::instance().set_mpi_state(true);

    // FIXME: Refactor
    // finalize_handles_t finalize_handles;
    // finalize_handles.walk(mpi_task_args);

  } // execute_mpi_task

}; // struct task_wrapper_u

} // namespace legion
} // namespace execution
} // namespace flecsi
