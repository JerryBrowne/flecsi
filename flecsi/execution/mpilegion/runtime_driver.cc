/*~-------------------------------------------------------------------------~~*
 * Copyright (c) 2014 Los Alamos National Security, LLC
 * All rights reserved.
 *~-------------------------------------------------------------------------~~*/

/*!
 * \file legion/runtime_driver.cc
 * \authors bergen
 * \date Initial file creation: Jul 26, 2016
 */

#include "flecsi/execution/mpilegion/runtime_driver.h"

#include "flecsi/utils/common.h"
#include "flecsi/execution/context.h"

#ifndef FLECSI_DRIVER
  #include "flecsi/execution/default_driver.h"
#else
  #include EXPAND_AND_STRINGIFY(FLECSI_DRIVER)
#endif

namespace flecsi {
namespace execution {

void mpilegion_runtime_driver(const LegionRuntime::HighLevel::Task * task,
	const std::vector<LegionRuntime::HighLevel::PhysicalRegion> & regions,
	LegionRuntime::HighLevel::Context ctx,
	LegionRuntime::HighLevel::HighLevelRuntime * runtime)
	{
		context_t::instance().set_state(ctx, runtime, task, regions);

    std::cout<<"insidr TLT" <<std::endl;
    MPILegionInterop *Interop =  MPILegionInterop::instance();
    Interop->connect_with_mpi(
         context_t::instance().context(), context_t::instance().runtime());

   std::cout<<"handshake is connected to Legion" <<std::endl;
   std::cout<<"some computations in Legion"<<std::endl;

    const LegionRuntime::HighLevel::InputArgs & args =
      LegionRuntime::HighLevel::HighLevelRuntime::get_input_args();

    driver(args.argc, args.argv); 

    Interop->call_mpi=false;
    Interop->handoff_to_mpi(
           context_t::instance().context(), context_t::instance().runtime());
    }// legion_runtime_driver

} // namespace execution
} // namespace flecsi

/*~------------------------------------------------------------------------~--*
 * Formatting options for vim.
 * vim: set tabstop=2 shiftwidth=2 expandtab :
 *~------------------------------------------------------------------------~--*/