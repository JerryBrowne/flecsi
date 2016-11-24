/*~--------------------------------------------------------------------------~*
 * Copyright (c) 2015 Los Alamos National Security, LLC
 * All rights reserved.
 *~--------------------------------------------------------------------------~*/

#ifndef flecsi_dmp_partitioner_h
#define flecsi_dmp_partitioner_h

///
// \file partitioner.h
// \authors bergen
// \date Initial file creation: Nov 24, 2016
///

namespace flecsi {
namespace dmp {

///
// \class partitioner_t partitioner.h
// \brief partitioner_t provides...
///
class partitioner_t
{
public:

  /// Default constructor
  partitioner_t() {}

  /// Copy constructor (disabled)
  partitioner_t(const partitioner_t &) = delete;

  /// Assignment operator (disabled)
  partitioner_t & operator = (const partitioner_t &) = delete;

  /// Destructor
   virtual ~partitioner_t() {}

  virtual std::set<size_t> partition(dcrs_t & mesh) = 0;

private:

}; // class partitioner_t

} // namespace dmp
} // namespace flecsi

#endif // flecsi_dmp_partitioner_h

/*~-------------------------------------------------------------------------~-*
 * Formatting options for vim.
 * vim: set tabstop=2 shiftwidth=2 expandtab :
 *~-------------------------------------------------------------------------~-*/
