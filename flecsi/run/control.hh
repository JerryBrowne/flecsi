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

#include <flecsi-config.h>

#include "flecsi/execution.hh"
#include "flecsi/flog.hh"
#include "flecsi/run/point_walker.hh"
#include "flecsi/util/constant.hh"
#include "flecsi/util/dag.hh"
#include "flecsi/util/demangle.hh"

#include <functional>
#include <map>
#include <vector>

namespace flecsi {
namespace run {
/// \defgroup control Control Model
/// Types for defining, extending, and executing control-flow graphs.
/// \ingroup runtime
/// \{

inline log::devel_tag control_tag("control");

#if defined(FLECSI_ENABLE_GRAPHVIZ)
inline program_option<bool> control_model_option("FleCSI Options",
  "control-model",
  "Output a dot file of the control model. This can be processed into a pdf "
  "using the dot command, like:\n\033[0;36m$ dot -Tpdf input.dot > "
  "output.pdf\033[0m",
  {{flecsi::option_implicit, true}, {flecsi::option_zero}});

inline program_option<bool> control_model_sorted_option("FleCSI Options",
  "control-model-sorted",
  "Output a dot file of the sorted control model actions.",
  {{flecsi::option_implicit, true}, {flecsi::option_zero}});
#endif

template<auto CP>
using control_point = util::constant<CP>;

/*!
 */

template<bool (*Predicate)(), typename... ControlPoints>
using cycle = run_impl::cycle<Predicate, ControlPoints...>;

#ifdef DOXYGEN
/// An example control policy that is not really implemented.
struct control_policy {
  /// The labels for the control-flow graph.
  enum control_points_enum {};
  /// The control-flow graph.
  /// Each element is a \c control_point or a \c cycle.
  using control_points = std::tuple<>;
  /// Base class for control point objects.
  struct node_policy {};
};
#endif

/*!
  The control type provides a control model for specifying a
  set of control points as a coarse-grained control flow graph,
  with each node of the graph specifying a set of actions as a
  directed acyclic graph (DAG). The actions under a control point
  DAG are topologically sorted to respect dependency edges, which can
  be specified through the dag interface.

  If Graphviz support is enabled, the control flow graph and its DAG nodes
  can be written to a graphviz file that can be compiled and viewed using
  the \em dot program.

  \tparam ControlPolicy policy type like \c control_policy
 */

template<typename ControlPolicy>
struct control : ControlPolicy {

  using target_type = int (*)();

private:
  friend ControlPolicy;

  using control_points = typename ControlPolicy::control_points;
  using control_points_enum = typename ControlPolicy::control_points_enum;
  using node_policy = typename ControlPolicy::node_policy;

  using point_walker = run_impl::point_walker<control<ControlPolicy>>;
  friend point_walker;

  using init_walker = run_impl::init_walker<control<ControlPolicy>>;
  friend init_walker;

#if defined(FLECSI_ENABLE_GRAPHVIZ)
  using point_writer = run_impl::point_writer<control<ControlPolicy>>;
  friend point_writer;
#endif

  /*
    Control node type. This just adds an executable target.
   */

  struct control_node : node_policy {

    template<typename... Args>
    control_node(target_type target, Args &&... args)
      : node_policy(std::forward<Args>(args)...), target_(target) {}

    int execute() const {
      return target_();
    }

  private:
    target_type target_;
  }; // struct control_node

  /*
    Use the node type that is defined by the specialized DAG.
   */

  using node_type = typename util::dag<control_node>::node_type;

  /*
    Initialize the control point dags. This is necessary in order to
    assign labels in the case that no actions are registered at one
    or more control points.
   */

  control() {
    run_impl::walk<control_points>(init_walker(registry_));
  }

  /*
    The singleton instance is private, and should only be accessed by internal
    types.
   */

  static control & instance() {
    static control c;
    return c;
  }

  /*
    Return the dag at the given control point.
   */

  util::dag<control_node> & control_point_dag(control_points_enum cp) {
    registry_.try_emplace(cp, *cp);
    return registry_[cp];
  }

  /*
    Return a map of the sorted dags under each control point.
   */

  std::map<control_points_enum, std::vector<node_type const *>> sort() {
    std::map<control_points_enum, std::vector<node_type const *>> sorted;
    for(auto & d : registry_) {
      sorted.try_emplace(d.first, d.second.sort());
    }
    return sorted;
  }

  /*
    Run the control model.
   */

  int run() {
    int status{flecsi::run::status::success};
    run_impl::walk<control_points>(point_walker(sort(), status));
    return status;
  } // run

  /*
    Output a graph of the control model.
   */

#if defined(FLECSI_ENABLE_GRAPHVIZ)
  int write() {
    flecsi::util::graphviz gv;
    run_impl::walk<control_points>(point_writer(registry_, gv));
    std::string file = program() + "-control-model.dot";
    gv.write(file);
    return flecsi::run::status::control_model;
  } // write

  int write_sorted() {
    flecsi::util::graphviz gv;
    point_writer::write_sorted(sort(), gv);
    std::string file = program() + "-control-model-sorted.dot";
    gv.write(file);
    return flecsi::run::status::control_model_sorted;
  } // write_sorted
#endif

  std::map<control_points_enum, util::dag<control_node>> registry_;

public:
  /*!
    Return the user's control state.

    @return The singleton instance of the user's control policy type. Users can
            add arbitrary data members and interfaces to this type that can be
            to store control state information.
   */

  static ControlPolicy & state() {
    return instance();
  }

  /*!
    The action type provides a mechanism to add execution elements to the
    FleCSI control model.

    @tparam Target       The execution target.
    @tparam ControlPoint The control point under which this action is
                         executed.
   */

  template<target_type Target, control_points_enum ControlPoint>
  struct action {

    template<target_type U, control_points_enum V>
    friend struct action;

    /*!
      Add a function to be executed under the specified control point.

      @param args   A variadic list of arguments that are forwarded to the
                    user-defined node type, as spcified in the control policy.
     */

    template<typename... Args>
    action(Args &&... args)
      : node_(util::symbol<*Target>(), Target, std::forward<Args>(args)...) {
      instance().control_point_dag(ControlPoint).push_back(&node_);
    }

    /*
      Dummy type used to force namespace scope execution.
     */

    struct dependency {};

    /*!
      Add a dependency on the given action.

      @param from The upstream node in the dependency.

      @note It is illegal to add depencdencies between actions under
            different  control  points. Attempting to do so will result
            in a compile-time error.
     */

    template<target_type U, control_points_enum V>
    dependency add(action<U, V> const & from) {
      static_assert(ControlPoint == V,
        "you cannot add dependencies between actions under different control "
        "points");
      node_.push_back(&from.node_);
      return {};
    }

    /*!
     */

    template<target_type F>
    void push_back(action<F, ControlPoint> const & from) {
      node_.push_back(&from.node_);
    }

  private:
    node_type node_;
  }; // struct action

  /*!
    Execute the control model. This method does a topological sort of the
    actions under each of the control points to determine a non-unique, but
    valid ordering, and executes the actions.
   */

  static int execute() {
    return instance().run();
  } // execute

  /*!
    Process control model command-line options.
   */

  static int check_status(int s) {
#if defined(FLECSI_ENABLE_GRAPHVIZ)
    switch(s) {
      case flecsi::run::status::control_model:
        return instance().write();
      case flecsi::run::status::control_model_sorted:
        return instance().write_sorted();
      default:
        break;
    } // switch
#endif
    return s;
  } // check_status

}; // struct control

/// \}
} // namespace run
} // namespace flecsi
