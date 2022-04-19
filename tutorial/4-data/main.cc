// Copyright (c) 2016, Triad National Security, LLC
// All rights reserved.

#include <flecsi/execution.hh>
#include <flecsi/flog.hh>

#include "control.hh"

int
main(int argc, char ** argv) {
  auto status = flecsi::initialize(argc, argv);
  status = control::check_status(status);

  if(status != flecsi::run::status::success) {
    return status < flecsi::run::status::clean ? 0 : status;
  }

  flecsi::flog::add_output_stream("clog", std::clog, true);

  status = flecsi::start(control::execute);

  flecsi::finalize();

  return status;
} // main
