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

#include "flecsi/flog.hh"

#include <fstream>
#include <iterator>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

namespace flecsi {
namespace topo {
namespace unstructured_impl {

class ugm_definition
{
public:
  using point = std::array<double, 3>;
  static constexpr Dimension dimension() {
    return 3;
  }

  ugm_definition(const char * filename) {
    file_.open(filename, std::ifstream::in);

    if(file_.good()) {
      std::string line;
      std::getline(file_, line);
      std::istringstream iss(line);

      // Read the number of vertices and cells
      iss >> num_vertices_ >> num_cells_;

      // Get the offset to the beginning of the vertices
      vertex_start_ = file_.tellg();

      for(size_t i(0); i < num_vertices_; ++i) {
        std::getline(file_, line);
      } // for

      cell_start_ = file_.tellg();
    }
    else {
      flog_fatal("failed opening " << filename);
    } // if

    // Go to the start of the cells.
    std::string line;
    file_.seekg(cell_start_);
    for(size_t l(0); l < num_cells_; ++l) {
      std::getline(file_, line);
      std::istringstream iss(line);
      ids_.push_back(std::vector<size_t>(
        std::istream_iterator<size_t>(iss), std::istream_iterator<size_t>()));
    }

  } // ugm_definition

  ugm_definition(const ugm_definition &) = delete;
  ugm_definition & operator=(const ugm_definition &) = delete;

  std::size_t num_entities(Dimension dimension) const {
    flog_assert(dimension == 0 || dimension == 3, "invalid dimension");
    return dimension == 0 ? num_vertices_ : num_cells_;
  }

  /*!
    Return the set of vertices that make up all cells

    @param [in] from_dim the entity dimension to query
    @param [in] to_dim the dimension of entities we wish to return
   */

  const std::vector<std::vector<std::size_t>> & entities(Dimension from_dim,
    Dimension to_dim) const {
    flog_assert(from_dim == 3, "invalid dimension " << from_dim);
    flog_assert(to_dim == 0, "invalid dimension " << to_dim);
    return ids_;
  }

  /*!
    Return the set of vertices of a particular entity.

    @param [in] dimension  the entity dimension to query.
    @param [in] entity_id  the id of the entity in question.
   */

  std::vector<size_t>
  entities(Dimension from_dim, Dimension to_dim, std::size_t entity_id) const {
    flog_assert(from_dim == 3, "invalid dimension " << from_dim);
    flog_assert(to_dim == 0, "invalid dimension " << to_dim);

    std::string line;
    std::vector<size_t> ids;
    size_t v0, v1, v2, v3;

    // Go to the start of the cells.
    file_.seekg(cell_start_);

    // Walk to the line with the requested id.
    for(size_t l(0); l < entity_id; ++l) {
      std::getline(file_, line);
    } // for

    // Get the line with the information for the requested id.
    std::getline(file_, line);
    std::istringstream iss(line);

    std::size_t vertices;
    iss >> vertices;

    flog_assert(vertices == 4, "tetrahedra only");

    if(vertices == 4) {
      iss >> v0 >> v1 >> v2 >> v3;
      ids.push_back(v0);
      ids.push_back(v1);
      ids.push_back(v2);
      ids.push_back(v3);
    }

    return ids;
  } // vertices

  /*
    Return the vertex with the given id.

    @param id The vertex id.
   */

  point vertex(size_t id) const {
    std::string line;
    point v;

    // Go to the start of the vertices.
    file_.seekg(vertex_start_);

    // Walk to the line with the requested id.
    for(size_t l(0); l < id; ++l) {
      std::getline(file_, line);
    } // for

    // Get the line with the information for the requested id.
    std::getline(file_, line);
    std::istringstream iss(line);

    // Read the vertex coordinates.
    iss >> v[0] >> v[1] >> v[2];

    return v;
  } // vertex

private:
  mutable std::ifstream file_;
  std::vector<std::vector<size_t>> ids_;

  size_t num_vertices_;
  size_t num_cells_;

  mutable std::iostream::pos_type vertex_start_;
  mutable std::iostream::pos_type cell_start_;

}; // class ugm_definition

} // namespace unstructured_impl
} // namespace topo
} // namespace flecsi
