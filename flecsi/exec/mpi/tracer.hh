#ifndef FLECSI_MPI_EXEC_TRACER_HH
#define FLECSI_MPI_EXEC_TRACER_HH

namespace flecsi::exec {

struct trace {

  struct guard;
  using id_t = int;

  inline guard make_guard();

  trace() {}
  explicit trace(id_t) {}

  trace(trace &&) = default;

  void skip() {}

public:
  static bool is_tracing() {
    return false;
  }

private:
  void start() {}
  void stop() {}
}; // struct trace

} // namespace flecsi::exec

#endif // FLECSI_MPI_EXEC_TRACER_HH