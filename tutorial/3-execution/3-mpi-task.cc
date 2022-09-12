#include <flecsi/execution.hh>
#include <flecsi/flog.hh>

#include "control.hh"

using namespace flecsi;

// Task with no arguments.

void
task() {
  flog(info) << "Hello World from process: " << process() << std::endl;
}

// Advance control point.

int
advance() {
  execute<task, mpi>();

  return 0;
} // advance()
control::action<advance, cp::advance> advance_action;
