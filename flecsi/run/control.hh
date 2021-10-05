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

#include "flecsi/execution.hh"
#include "flecsi/flog.hh"
#include "flecsi/run/types.hh"
#include "flecsi/util/constant.hh"
#include "flecsi/util/dag.hh"
#include "flecsi/util/demangle.hh"

#include <functional>
#include <map>
#include <vector>

namespace flecsi {
namespace run {

inline flog::devel_tag control_tag("control");

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
using control_point = run_impl::control_point<CP>;

template<auto CP>
using meta_point = run_impl::meta_point<CP>;

template<bool (*Predicate)(), typename... ControlPoints>
using cycle = run_impl::cycle<Predicate, ControlPoints...>;

/*!
  Base class for providing default implementations for optional interfaces.
 */

struct control_base {
  int initialize() {
    return success;
  }
  int finalize(int run) {
    return run;
  }
};

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

  @ingroup control
 */

template<typename P>
struct control : P {

  using target_type = int (*)();

private:
  friend P;

  using control_points = typename P::control_points;
  using control_points_enum = typename P::control_points_enum;
  using node_policy = typename P::node_policy;

  using point_walker = run_impl::point_walker<control<P>>;
  friend point_walker;

  using init_walker = run_impl::init_walker<control<P>>;
  friend init_walker;

#if defined(FLECSI_ENABLE_GRAPHVIZ)
  using point_writer = run_impl::point_writer<control<P>>;
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
    Use the ndoe type that is defined by the specialized DAG.
   */

  using node_type = typename util::dag<control_node>::node_type;

  /*
    Initialize the control point dags. This is necessary in order to
    assign labels in the case that no actions are registered at one
    or more control points.
   */

  control() {
    init_walker(registry_).template walk_types<control_points>();
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
    point_walker(sort(), status).template walk_types<control_points>();
    return status;
  } // run

  /*
    Output a graph of the control model.
   */

#if defined(FLECSI_ENABLE_GRAPHVIZ)
  int write() {
    flecsi::util::graphviz gv;
    point_writer(registry_, gv).template walk_types<control_points>();
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
    Return the user's control policy.

    @return The singleton instance of the user's control policy type. Users can
            add arbitrary data members and interfaces to this type that can be
            to store control state information.
   */

  static P & policy() {
    return instance();
  }

  /*!
    The action type provides a mechanism to add execution elements to the
    FleCSI control model.

    @tparam T  The execution target.
    @tparam CP The control point under which this action is executed.
    @tparam M  Boolean indicating whether or not the action is a meta action.
   */

  template<target_type T, control_points_enum CP, bool M = false>
  struct action {

    template<target_type, control_points_enum, bool>
    friend struct action;

    /*!
      Add a function to be executed under the specified control point.

      @param target The target function.
      @param label  The label for the function. This is used to label the node
                    when the control model is printed with graphviz.
      @param args   A variadic list of arguments that are forwarded to the
                    user-defined node type, as spcified in the control policy.
     */

    template<typename... Args>
    action(Args &&... args)
      : node_(util::symbol<*T>(), T, std::forward<Args>(args)...) {
      static_assert(M == run_impl::is_meta<control_points>(CP),
        "you cannot use this interface for internal control points!");
      instance().control_point_dag(CP).push_back(&node_);
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
      static_assert(CP == V,
        "you cannot add dependencies between actions under different control "
        "points");
      node_.push_back(&from.node_);
      return {};
    }

    /*!
     */

    template<target_type F>
    void push_back(action<F, CP, M> const & from) {
      node_.push_back(&from.node_);
    }

  protected:
    node_type node_;
  }; // struct action

  template<target_type T, control_points_enum CP>
  using meta = action<T, CP, true>;

  /*!
    Execute the control model. This method does a topological sort of the
    actions under each of the control points to determine a non-unique, but
    valid ordering, and executes the actions.
   */

  static int execute() {
    if constexpr(std::is_base_of_v<control_base, P>) {
      const int r = instance().initialize();
      return r == success ? instance().finalize(instance().run()) : r;
    }
    else {
      return instance().run();
    }
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

} // namespace run
} // namespace flecsi
