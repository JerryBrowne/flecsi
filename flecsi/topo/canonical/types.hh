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
#endif

#include "flecsi/execution.hh"
#include "flecsi/topo/index.hh"

#include <cstddef>
#include <vector>

namespace flecsi {
namespace topo {

struct canonical_base : with_meta<canonical_base> {

  struct coloring {
    std::size_t colors;
    std::vector<std::size_t> is_allocs;
    std::vector<std::vector<std::size_t>> cn_allocs;
  }; // struct coloring

  struct Meta {
    util::id column_size, column_offset;
  };

  using with_meta::with_meta;

  static inline const field<Meta,
    data::single>::definition<topo::meta<canonical_base>>
    meta_field;

  static std::size_t
  is_size(std::size_t size, std::size_t colors, std::size_t) {
    return size / colors;
  }

  static void cn_size(std::size_t size, resize::Field::accessor<wo> a) {
    a = data::partition::make_row(flecsi::color(), size);
  }
}; // struct canonical_base

} // namespace topo
} // namespace flecsi
