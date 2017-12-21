
#include <Maestro.H>

using namespace amrex;

void
Maestro::MakeVelForce (Vector<MultiFab>& vel_force,
                       const Vector<std::array< MultiFab, AMREX_SPACEDIM > >& uedge,
                       int is_final_update,
                       int do_add_utilde_force)
{
    for (int lev=0; lev<=finest_level; ++lev) {

        // get references to the MultiFabs at level lev
        MultiFab& vel_force_mf = vel_force[lev];
        const MultiFab& gpi_mf = gpi[lev];
        const MultiFab& sold_mf = sold[lev];
        const MultiFab& uedge_mf = uedge[0][lev];
        const MultiFab& vedge_mf = uedge[1][lev];
#if (AMREX_SPACEDIM == 3)
        const MultiFab& wedge_mf = uedge[2][lev];
#endif

        // Loop over boxes (make sure mfi takes a cell-centered multifab as an argument)
        for ( MFIter mfi(sold_mf); mfi.isValid(); ++mfi ) {

            // Get the index space of the valid region
            const Box& validBox = mfi.validbox();

            // call fortran subroutine
            // use macros in AMReX_ArrayLim.H to pass in each FAB's data, 
            // lo/hi coordinates (including ghost cells), and/or the # of components
            // We will also pass "validBox", which specifies the "valid" region.
            make_vel_force(lev,ARLIM_3D(validBox.loVect()), ARLIM_3D(validBox.hiVect()),
                           BL_TO_FORTRAN_FAB(vel_force_mf[mfi]),
                           BL_TO_FORTRAN_FAB(gpi_mf[mfi]),
                           BL_TO_FORTRAN_N_3D(sold_mf[mfi],Rho),
                           BL_TO_FORTRAN_3D(uedge_mf[mfi]),
                           BL_TO_FORTRAN_3D(vedge_mf[mfi]),
#if (AMREX_SPACEDIM == 3)
                           BL_TO_FORTRAN_3D(wedge_mf[mfi]),
#endif
                           w0.dataPtr(),
                           rho0_old.dataPtr(),
                           grav_cell_old.dataPtr(),
                           is_final_update,
                           do_add_utilde_force);
        }
    }



    for (int lev=finest_level-1; lev>=0; --lev)
    {
        AverageDownTo(lev,vel_force,0,AMREX_SPACEDIM); // average lev+1 down to lev
    }

}