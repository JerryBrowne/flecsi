// Copyright (C) 2016, Triad National Security, LLC
// All rights reserved.

#ifndef FLECSI_UTIL_MPI_HH
#define FLECSI_UTIL_MPI_HH

#include "flecsi/config.hh"
#include "flecsi/util/array_ref.hh" // span
#include "flecsi/util/serialize.hh"

#include <algorithm>
#include <complex>
#include <cstddef> // byte
#include <cstdint>
#include <iterator>
#include <memory>
#include <numeric>
#include <optional>
#include <stack>
#include <type_traits>

#include <mpi.h>

namespace flecsi {
namespace util {
namespace mpi {
/// \defgroup mpi-utils MPI
/// These require MPI tasks, not the MPI backend.
/// \ingroup utils
/// \{

inline void
test(int err) {
  if(err != MPI_SUCCESS) {
    char msg[MPI_MAX_ERROR_STRING + 1];
    int len;
    if(MPI_Error_string(err, msg, &len) != MPI_SUCCESS)
      len = 0;
    msg[len] = 0;
    flog_fatal("MPI error " << err << ": " << msg);
  }
}

namespace detail {
struct guard {
  guard() = default;
  guard(guard &&) = delete; // MPI knows our address
  ~guard() {
    if(!v.empty())
      flog_fatal("MPI datatypes list destroyed too early");
  }

  void commit(MPI_Datatype & d) {
    if(!setup) {
      int keyval;
      test(MPI_Comm_create_keyval(
        MPI_COMM_NULL_COPY_FN, destroy, &keyval, nullptr));
      test(MPI_Comm_set_attr(MPI_COMM_SELF, keyval, this));
      test(MPI_Comm_free_keyval(&keyval));
      setup = true;
    }
    test(MPI_Type_commit(&d));
    v.push_back(d);
  }

private:
  static int destroy(MPI_Comm, int, void * attr, void *) {
    int e = MPI_SUCCESS;
    for(auto & v = static_cast<guard *>(attr)->v;
        !v.empty() && e == MPI_SUCCESS;) {
      e = MPI_Type_free(&v.back());
      v.pop_back();
    }
    return e;
  }

  bool setup = false;
  std::vector<MPI_Datatype> v;
} inline datatypes;

struct vector { // for *v functions
  explicit vector(int n) {
    off.reserve(n);
    sz.reserve(n);
  }

  std::vector<std::byte> data;
  std::vector<int> off, sz;

  void skip() {
    sz.push_back(0);
    off.push_back(data.size());
  }
  template<class T>
  void put(const T & t) {
    const auto n = off.emplace_back(data.size());
    data.resize(n + sz.emplace_back(serial::size(t)));
    auto * p = data.data() + n;
    serial::put(p, t);
  }
};
} // namespace detail

struct init {
  init(int & argc, char **& argv) {
#if defined(GASNET_CONDUIT_IBV) || defined(GASNET_CONDUIT_UCX)
    // work around GASNet issues during application cleanup:
    // See line 149 of mpi_interop.cc as of Legion version 23.06.0
    setenv("GASNET_CATCH_EXIT", "0", 0);
#endif

    // The MPI backend always requires this, as does the Legion backend with
    // the MPI GASNet conduit, and it's become very common, so just use it:
    int provided;
    test(MPI_Init_thread(&argc, &argv, MPI_THREAD_MULTIPLE, &provided));
    if(provided < MPI_THREAD_MULTIPLE)
      flog_fatal("MPI_THREAD_MULTIPLE unavailable");
  }
  init(init &&) = delete;
  ~init() {
#ifndef GASNET_CONDUIT_MPI
    test(MPI_Finalize());
#endif
  }
};

struct comm {
  MPI_Comm c = MPI_COMM_NULL;

  comm() = default;
  comm(comm && o) noexcept {
    std::swap(c, o.c);
  }
  ~comm() {
    if(*this)
      test(MPI_Comm_free(&c));
  }
  comm & operator=(comm o) & noexcept {
    std::swap(c, o.c);
    return *this;
  }
  explicit operator bool() const noexcept {
    return c != MPI_COMM_NULL;
  }

  static comm split(MPI_Comm c0, int c, int k = 0) {
    comm ret;
    test(MPI_Comm_split(c0, c, k, &ret.c));
    return ret;
  }
};

// NB: OpenMPI's predefined handles are not constant expressions.
template<class TYPE>
auto
maybe_static() {
  using namespace std;
  static_assert(is_same_v<TYPE, remove_cv_t<remove_reference_t<TYPE>>>);
  // List is from MPI 3.2 draft.
  // TIP: Specializations would collide for, say, int32_t==int.
  if constexpr(is_arithmetic_v<TYPE>) { // save on template instantiations
    if constexpr(is_same_v<TYPE, char>)
      return MPI_CHAR;
    else if constexpr(is_same_v<TYPE, short>)
      return MPI_SHORT;
    else if constexpr(is_same_v<TYPE, int>)
      return MPI_INT;
    else if constexpr(is_same_v<TYPE, long>)
      return MPI_LONG;
    else if constexpr(is_same_v<TYPE, long long>)
      return MPI_LONG_LONG;
    else if constexpr(is_same_v<TYPE, signed char>)
      return MPI_SIGNED_CHAR;
    else if constexpr(is_same_v<TYPE, unsigned char>)
      return MPI_UNSIGNED_CHAR;
    else if constexpr(is_same_v<TYPE, unsigned short>)
      return MPI_UNSIGNED_SHORT;
    else if constexpr(is_same_v<TYPE, unsigned>)
      return MPI_UNSIGNED;
    else if constexpr(is_same_v<TYPE, unsigned long>)
      return MPI_UNSIGNED_LONG;
    else if constexpr(is_same_v<TYPE, unsigned long long>)
      return MPI_UNSIGNED_LONG_LONG;
    else if constexpr(is_same_v<TYPE, float>)
      return MPI_FLOAT;
    else if constexpr(is_same_v<TYPE, double>)
      return MPI_DOUBLE;
    else if constexpr(is_same_v<TYPE, long double>)
      return MPI_LONG_DOUBLE;
    else if constexpr(is_same_v<TYPE, wchar_t>)
      return MPI_WCHAR;
#ifdef INT8_MIN
    else if constexpr(is_same_v<TYPE, int8_t>)
      return MPI_INT8_T;
    else if constexpr(is_same_v<TYPE, uint8_t>)
      return MPI_UINT8_T;
#endif
#ifdef INT16_MIN
    else if constexpr(is_same_v<TYPE, int16_t>)
      return MPI_INT16_T;
    else if constexpr(is_same_v<TYPE, uint16_t>)
      return MPI_UINT16_T;
#endif
#ifdef INT32_MIN
    else if constexpr(is_same_v<TYPE, int32_t>)
      return MPI_INT32_T;
    else if constexpr(is_same_v<TYPE, uint32_t>)
      return MPI_UINT32_T;
#endif
#ifdef INT64_MIN
    else if constexpr(is_same_v<TYPE, int64_t>)
      return MPI_INT64_T;
    else if constexpr(is_same_v<TYPE, uint64_t>)
      return MPI_UINT64_T;
#endif
    else if constexpr(is_same_v<TYPE, MPI_Aint>)
      return MPI_AINT;
    else if constexpr(is_same_v<TYPE, MPI_Offset>)
      return MPI_OFFSET;
    else if constexpr(is_same_v<TYPE, MPI_Count>)
      return MPI_COUNT;
    else if constexpr(is_same_v<TYPE, bool>)
      return MPI_CXX_BOOL;
  }
  else if constexpr(is_same_v<TYPE, complex<float>>)
    return MPI_CXX_FLOAT_COMPLEX;
  else if constexpr(is_same_v<TYPE, complex<double>>)
    return MPI_CXX_DOUBLE_COMPLEX;
  else if constexpr(is_same_v<TYPE, complex<long double>>)
    return MPI_CXX_LONG_DOUBLE_COMPLEX;
  else if constexpr(is_same_v<TYPE, byte>)
    return MPI_BYTE;
  // else: void
}

template<class T>
MPI_Datatype
type() {
  if constexpr(!std::is_void_v<decltype(maybe_static<T>())>)
    return maybe_static<T>();
  else {
    static_assert(bit_assignable_v<T>);
    static const MPI_Datatype ret = [] {
      MPI_Datatype data_type;
      test(MPI_Type_contiguous(sizeof(T), MPI_BYTE, &data_type));
      detail::datatypes.commit(data_type);
      return data_type;
    }();
    return ret;
  }
}
// Use this to restrict to predefined types before MPI_Init:
template<class T,
  class = std::enable_if_t<!std::is_void_v<decltype(maybe_static<T>())>>>
MPI_Datatype
static_type() {
  return maybe_static<T>();
}

struct auto_requests {
  auto_requests() = default;
  explicit auto_requests(int n) {
    v.reserve(n);
  }
  auto_requests(auto_requests &&) = default;
  ~auto_requests() {
    test(MPI_Waitall(v.size(), v.data(), MPI_STATUSES_IGNORE));
  }

  auto_requests & operator=(auto_requests a) & {
    v.swap(a.v);
    return *this;
  }

  MPI_Request * operator()() {
    return &v.emplace_back();
  }

  std::vector<MPI_Request> v;
};

// A value still being received.
template<class T>
struct future {
  future() = default;
  future(T t) : t(std::move(t)) {}
  template<class F>
  future(F && f, bool root) : t(root ? T(std::forward<F>(f)()) : T()) {}
  future(future &&) = delete; // data() stability

  T & operator()() & {
    this->req = {}; // this-> avoids Clang bug #62818
    return t;
  }
  T && operator()() && {
    return std::move((*this)());
  }

  T * data() {
    return &t;
  }
  MPI_Request * request() {
    return req();
  }

private:
  T t;
  auto_requests req;
};

/*!
  Convenience function to get basic MPI communicator information.
 */

inline int
size(MPI_Comm comm = MPI_COMM_WORLD) {
  int ret;
  test(MPI_Comm_size(comm, &ret));
  return ret;
}
inline int
rank(MPI_Comm comm = MPI_COMM_WORLD) {
  int ret;
  test(MPI_Comm_rank(comm, &ret));
  return ret;
}

inline auto
info(MPI_Comm comm = MPI_COMM_WORLD) {
  return std::make_pair(rank(comm), size(comm));
} // info

namespace detail {
template<class R, class = void>
struct make_range {
  // maybe_unused needed due to unused-but-set-error with GCC 9.4
  static decltype(auto) get(R && r, [[maybe_unused]] int n) {
    if constexpr(std::is_invocable_v<R, int>)
      return util::transform_view(util::iota_view(0, n), std::forward<R>(r));
    else
      return std::forward<R>(r);
  }
};
template<class T>
decltype(auto)
range(T && t, int n) {
  return make_range<T>::get(std::forward<T>(t), n);
}
template<class F>
struct make_range<F, decltype(void(std::declval<F &>()(1, 1)))> {
  [[deprecated("remove second parameter or pass a range")]] static auto
  get(F && f, int n) {
    // NB: we assume that f is not a temporary in the immediate caller.
    return range([&f, n](int r) -> decltype(auto) { return f(r, n); }, n);
  }
};

template<class R>
using value_type = std::decay_t<decltype(*std::declval<R>().begin())>;
} // namespace detail

/*!
  One-to-All (variable) communication pattern.

  This function uses the FleCSI serialization interface to communicate data
  from the root rank (0) to all
  other ranks.

  \param r range or functor with signature (rank) or (\b deprecated) (rank,
    size), read/invoked only on rank 0 and in recipient order
  @param comm An MPI communicator.

  \return the value from \a r for the current rank
 */

template<typename R>
inline auto
one_to_allv(R && r, MPI_Comm comm = MPI_COMM_WORLD) {
  auto [rank, size] = info(comm);
  auto && rng = detail::range(std::forward<R>(r), size);
  using return_type = detail::value_type<decltype(rng)>;

  if constexpr(bit_copyable_v<return_type>) {
    std::vector<return_type> send;
    if(!rank) {
      send.reserve(size);
      std::copy_n(rng.begin(), size, std::back_inserter(send));
    }
    else
      send.resize(1);
    test(MPI_Scatter(send.data(),
      1,
      type<return_type>(),
      rank ? send.data() : MPI_IN_PLACE,
      1,
      type<return_type>(),
      0,
      comm));
    return send.front();
  }
  else {
    std::optional<return_type> mine;
    detail::vector v(size);
    v.skip(); // v.sz used even off-root
    if(rank == 0) {
      auto i = rng.begin();
      mine.emplace(*i);
      ++i;
      for(auto n = size; --n; ++i)
        v.put(*i);
    }

    test(MPI_Scatter(v.sz.data(),
      1,
      MPI_INT,
      rank ? v.sz.data() : MPI_IN_PLACE,
      1,
      MPI_INT,
      0,
      comm));
    if(rank)
      v.data.resize(v.sz.front());
    test(MPI_Scatterv(v.data.data(),
      v.sz.data(),
      v.off.data(),
      MPI_BYTE,
      v.data.data(),
      v.sz.front(),
      MPI_BYTE,
      0,
      comm));

    if(rank) {
      auto const * p = v.data.data();
      return serial::get<return_type>(p);
    }
    else
      return std::move(*mine);
  }
} // one_to_allv

namespace detail {
// We allocate each message separately to control memory usage.
// These two class templates serialize or not with the same interface.
template<class T>
struct bit_message {
  bit_message(int, int, MPI_Comm) : p(new T) {}
  template<class I> // deferred to support prvalues
  explicit bit_message(I && i) : p(new T(*std::forward<I>(i))) {}

  void * data() {
    return p.get();
  }
  const void * data() const {
    return p.get();
  }
  int count() const {
    return 1;
  }
  std::size_t bytes() const {
    return sizeof(T);
  }
  T get() && {
    return std::move(*p);
  }
  void reset() {
    p.reset();
  }

  static MPI_Datatype type() {
    return mpi::type<T>();
  }

private:
  std::unique_ptr<T> p;
};

template<class T>
struct serial_message {
  serial_message(int s, int t, MPI_Comm comm)
    : v([&] {
        MPI_Status st;
        test(MPI_Probe(s, t, comm, &st));
        int ret;
        test(MPI_Get_count(&st, type(), &ret));
        return ret;
      }()) {}
  template<class I>
  explicit serial_message(I && i)
    : v(serial::put_tuple<T>(*std::forward<I>(i))) {}

  void * data() {
    return v.data();
  }
  const void * data() const {
    return v.data();
  }
  int count() const {
    return v.size();
  }
  std::size_t bytes() const {
    return v.size();
  }
  T get() const {
    return serial::get1<T>(v.data());
  }
  void reset() {
    decltype(v)().swap(v);
  }

  static MPI_Datatype type() {
    return MPI_BYTE;
  }

private:
  std::vector<std::byte> v;
};
} // namespace detail

/// Send data from rank 0 to all others, controlling memory usage.
/// No messages are constructed while data in transit exceeds \a mem
/// (transmission occurs, at least serially, even if it is 0).
/// \param r range or functor with signature (rank) or (\b deprecated) (rank,
///   size), read/invoked only on rank 0 and in recipient order
/// \param mem bytes of memory to use before waiting
template<class R>
auto
one_to_alli(R && r, std::size_t mem = 1 << 20, MPI_Comm comm = MPI_COMM_WORLD) {
  const auto [rank, size] = info(comm);
  auto && rng = detail::range(std::forward<R>(r), size);
  using T = detail::value_type<decltype(rng)>;
  using M = std::conditional_t<bit_copyable_v<T>,
    detail::bit_message<T>,
    detail::serial_message<T>>;

  if(!rank) {
    auto it = rng.begin();
    auto ret = *it;
    ++it;
    {
      const auto n = size - 1;
      auto_requests areq;
      auto & req = areq.v;
      {
        std::vector<M> val; // parallel to req
        std::vector<int> done(n);
        std::stack<int> free; // inactive requests
        std::size_t used = 0;
        for(int r = 1; r < size; ++r, ++it) {
          const int i = [&] {
            if(free.empty()) {
              val.emplace_back(it);
              return int(req.size()); // created below
            }
            else {
              const auto ret = free.top();
              free.pop();
              val[ret] = M(it);
              return ret;
            }
          }();
          {
            const auto & v = val[i];
            if(i == int(req.size()))
              req.emplace_back();
            // Discourage buffering (at the cost of needless synchronization):
            test(
              MPI_Issend(v.data(), v.count(), M::type(), r, 0, comm, &req[i]));
            used += v.bytes();
          }
          while(used > mem) {
            int count;
            test(MPI_Waitsome(req.size(),
              req.data(),
              &count,
              done.data(),
              MPI_STATUSES_IGNORE));
            // Clear data for completed sends:
            for(auto j : util::span(done).first(count)) {
              auto & w = val[j];
              used -= w.bytes();
              w.reset();
              free.push(j);
            }
          }
        }
      }
    }
    return ret;
  }
  else {
    M ret(0, 0, comm);
    test(MPI_Recv(
      ret.data(), ret.count(), M::type(), 0, 0, comm, MPI_STATUS_IGNORE));
    return std::move(ret).get();
  }
}

/*!
  All-to-All (variable) communication pattern.

  This function uses the FleCSI serialization interface with a packing
  callable object to communicate data from all ranks to all other ranks.

  \param r range or functor with signature (rank) or (\b deprecated) (rank,
    size), read/invoked in recipient order
  @param comm An MPI communicator.

  \return a \c std::vector of the values for the current rank from each rank's
    \a r
 */

template<typename R>
inline auto
all_to_allv(R && r, MPI_Comm comm = MPI_COMM_WORLD) {
  auto [rank, size] = info(comm);
  auto && rng = detail::range(std::forward<R>(r), size);
  using return_type = detail::value_type<decltype(rng)>;

  std::vector<return_type> result;
  result.reserve(size);
  if constexpr(bit_copyable_v<return_type>) {
    std::copy_n(rng.begin(), size, std::back_inserter(result));
    test(MPI_Alltoall(MPI_IN_PLACE,
      0,
      MPI_DATATYPE_NULL,
      result.data(),
      1,
      type<return_type>(),
      comm));
  }
  else {
    detail::vector recv(size);
    std::optional<return_type> mine;
    {
      detail::vector send(size);

      {
        auto i = rng.begin();
        for(int r = 0; r < size; ++r, ++i) {
          if(r == rank) {
            mine.emplace(*i);
            send.skip();
          }
          else
            send.put(*i);
        } // for
      }

      recv.sz.resize(size);
      test(MPI_Alltoall(
        send.sz.data(), 1, MPI_INT, recv.sz.data(), 1, MPI_INT, comm));

      {
        int o = 0;
        for(const auto n : recv.sz) {
          recv.off.push_back(o);
          o += n;
        }
        recv.data.resize(o);
      }

      test(MPI_Alltoallv(send.data.data(),
        send.sz.data(),
        send.off.data(),
        MPI_BYTE,
        recv.data.data(),
        recv.sz.data(),
        recv.off.data(),
        MPI_BYTE,
        comm));
    }

    const std::byte * const p = recv.data.data();
    for(int r = 0; r < size; ++r) {
      if(r == rank)
        result.push_back(std::move(*mine));
      else
        result.push_back(serial::get1<return_type>(p + recv.off[r]));
    } // for
  }

  return result;
} // all_to_allv

/*!
  All gather communication pattern implemented using MPI_Allgather. This
  function is convenient for passing more complicated types. Otherwise,
  it may make more sense to use MPI_Allgather directly.

  This function uses the FleCSI serialization interface to copy data from all
  ranks to all other ranks.

  @tparam T serializable data type

  @param t object to send
  @param comm An MPI communicator.

  @return the values from each rank
 */

template<typename T>
std::vector<T>
all_gatherv(const T & t, MPI_Comm comm = MPI_COMM_WORLD) {
  auto [rank, size] = info(comm);
  std::vector<T> result;
  if constexpr(bit_copyable_v<T>) {
    const auto typ = type<T>();
    result.resize(size);
    test(MPI_Allgather(&t, 1, typ, result.data(), 1, typ, comm));
  }
  else {
    detail::vector v(size); // just a struct here
    v.sz.resize(size);
    v.sz[rank] = serial::size(t);

    test(MPI_Allgather(
      MPI_IN_PLACE, 0, MPI_DATATYPE_NULL, v.sz.data(), 1, MPI_INT, comm));

    v.off.resize(size);
    v.off[0] = 0;
    std::partial_sum(v.sz.begin(), v.sz.end() - 1, v.off.begin() + 1);

    v.data.resize(v.off.back() + v.sz.back());
    {
      std::byte * p = v.data.data() + v.off[rank];
      serial::put(p, t);
    }

    test(MPI_Allgatherv(MPI_IN_PLACE,
      0,
      MPI_DATATYPE_NULL,
      v.data.data(),
      v.sz.data(),
      v.off.data(),
      MPI_BYTE,
      comm));

    result.reserve(size);

    auto p = std::as_const(v).data.data();
    for(int r = 0; r < size; ++r) {
      result.push_back(serial::get<T>(p));
    } // for
  }

  return result;
} // all_gatherv

/// \}
} // namespace mpi
} // namespace util
} // namespace flecsi

#endif
