/*
    @@@@@@@@  @@           @@@@@@   @@@@@@@@ @@
   /@@/////  /@@          @@////@@ @@////// /@@
   /@@       /@@  @@@@@  @@    // /@@       /@@
   /@@@@@@@  /@@ @@///@@/@@       /@@@@@@@@@/@@
   /@@////   /@@/@@@@@@@/@@       ////////@@/@@
   /@@       /@@/@@//// //@@    @@       /@@/@@
   /@@       @@@//@@@@@@ //@@@@@@  @@@@@@@@ /@@
   //       ///  //////   //////  ////////  //

   Copyright (c) 2016, Los Alamos National Security, LLC
   All rights reserved.
                                                                              */
#pragma once

/*! @file */

#include <flecsi-config.h>
#include <cinch-config.h>

#if defined(FLECSI_ENABLE_FLOG)
  #include <flecsi/utils/flog.h>
#endif

#include <flecsi/execution/context.h>

#include <cinch/runtime.h>

#if !defined(FLECSI_ENABLE_MPI)
  #error FLECSI_ENABLE_MPI not defined! This file depends on MPI!
#endif

#include <mpi.h>

#include <cstdlib>
#include <iostream>

#if defined(CINCH_ENABLE_BOOST)
  #include <boost/program_options.hpp>
  using namespace boost::program_options;
#endif

inline std::string __flecsi_tags = "all";

#if defined(CINCH_ENABLE_BOOST)
inline void add_options(options_description & desc) {
#if defined(FLECSI_ENABLE_FLOG)
  desc.add_options()
    ("tags,t", value(&__flecsi_tags)->implicit_value("0"),
    "Enable the specified output tags, e.g., --tags=tag1,tag2."
    " Passing --tags by itself will print the available tags.")
  ;
#endif
} // add_options
#endif

#if defined(CINCH_ENABLE_BOOST)
inline int initialize(int argc, char ** argv, parsed_options & parsed) {
#else
inline int initialize(int argc, char ** argv) {
  std::cout << "Executing initialize" << std::endl;
#endif

#if defined(FLECSI_ENABLE_FLOG)
  if(__flecsi_tags == "0") {
    std::cout << "Available tags (FLOG):" << std::endl;

    for(auto t: flog_tag_map()) {
      std::cout << " " << t.first << std::endl;
    } // for

    return 1;
  } // if
#endif

  int version, subversion;
  MPI_Get_version(&version, &subversion);

#if defined(GASNET_CONDUIT_MPI)
  if(version==3 && subversion>0) {
    int provided;
    MPI_Init_thread(&argc, &argv, MPI_THREAD_MULTIPLE, &provided);

    if(provided < MPI_THREAD_MULTIPLE) {
      std::cerr <<
        "Your implementation of MPI does not support "
        "MPI_THREAD_MULTIPLE which is required for use of the "
        "GASNet MPI conduit with the Legion-MPI Interop!"
        << std::endl;
      std::abort();
    } // if
  }
  else {
    // Initialize the MPI runtime
    MPI_Init(&argc, &argv);
  } // if
#else
  MPI_Init(&argc, &argv);
#endif

  int rank{0};
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  flecsi::execution::context_t::instance().color() = rank;

#if defined(FLECSI_ENABLE_FLOG)
  flog_init(__flecsi_tags);
#endif

  return 0;
} // initialize

inline int finalize(int argc, char ** argv, cinch::exit_mode_t mode) {

  // Shutdown the MPI runtime
#ifndef GASNET_CONDUIT_MPI
  MPI_Finalize();
#endif

  return 0;
} // initialize

inline cinch::runtime_handler_t handler{
  initialize,
  finalize
#if defined(CINCH_ENABLE_BOOST)
  , add_options
#endif
};

cinch_append_runtime_handler(handler);

inline int runtime_driver(int argc, char ** argv) {
  return flecsi::execution::context_t::instance().start(argc, argv);
} // runtime_driver

cinch_register_runtime_driver(runtime_driver);