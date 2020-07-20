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

#include <tuple>
#include <type_traits>

// Description of FLECSI_MEMBER_CHECKER.
// The macro invocation FLECSI_MEMBER_CHECKER(foo) produces a class template
// has_member_foo<T>, with a static bool data member ::value. The value will
// be true if T has a member called foo, or false if T does not.

//! Macro to check if the object has a member _*
#ifndef FLECSI_MEMBER_CHECKER
#define FLECSI_MEMBER_CHECKER(X)                                               \
  template<typename T>                                                         \
  class has_member_##X                                                         \
  {                                                                            \
    struct F {                                                                 \
      int X;                                                                   \
    };                                                                         \
    struct D : T, F {};                                                        \
    template<typename C, C>                                                    \
    struct ChT;                                                                \
    template<typename C>                                                       \
    static char (&f(ChT<int F::*, &C::X> *))[1];                               \
    template<typename C>                                                       \
    static char (&f(...))[2];                                                  \
                                                                               \
  public: /* all outsiders need... */                                          \
    static constexpr bool value = sizeof(f<D>(0)) == 2;                        \
  }
#endif

namespace flecsi {
namespace util {

//! Check if the object is a tuple
template<typename T>
struct is_tuple : std::false_type {};

//! Check if the object is a tuple
template<typename... T>
struct is_tuple<std::tuple<T...>> : std::true_type {};

} // namespace util
} // namespace flecsi
