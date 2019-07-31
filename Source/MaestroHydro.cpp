
#include <Maestro.H>
#include <MaestroHydro_F.H>
#include <MaestroBCThreads.H>

using namespace amrex;

void
Maestro::MakeUtrans (const Vector<MultiFab>& utilde,
                     const Vector<MultiFab>& ufull,
                     Vector<std::array< MultiFab, AMREX_SPACEDIM > >& utrans,
                     const Vector<std::array< MultiFab, AMREX_SPACEDIM > >& w0mac)
{
    // timer for profiling
    BL_PROFILE_VAR("Maestro::MakeUtrans()", MakeUtrans);

#ifdef AMREX_USE_CUDA
    auto not_launched = Gpu::notInLaunchRegion();
    // turn on GPU
    if (not_launched) Gpu::setLaunchRegion(true);
#endif

    for (int lev=0; lev<=finest_level; ++lev) {

        // Get the index space and grid spacing of the domain
        const Box& domainBox = geom[lev].Domain();
        const Real* dx = geom[lev].CellSize();

        // get references to the MultiFabs at level lev
        const MultiFab& utilde_mf  = utilde[lev];
        const MultiFab& ufull_mf   = ufull[lev];
        MultiFab& utrans_mf  = utrans[lev][0];
#if (AMREX_SPACEDIM >= 2)
        MultiFab& vtrans_mf  = utrans[lev][1];
        MultiFab Ip, Im;
        Ip.define(grids[lev],dmap[lev],AMREX_SPACEDIM,1);
        Im.define(grids[lev],dmap[lev],AMREX_SPACEDIM,1);
#if (AMREX_SPACEDIM == 3)
        MultiFab& wtrans_mf  = utrans[lev][2];
        const MultiFab& w0macx_mf  = w0mac[lev][0];
        const MultiFab& w0macy_mf  = w0mac[lev][1];
        const MultiFab& w0macz_mf  = w0mac[lev][2];
#endif
#endif

        // loop over boxes (make sure mfi takes a cell-centered multifab as an argument)

        // NOTE: don't tile, but threaded in fortran subroutine
#if (AMREX_SPACEDIM == 1)
        for ( MFIter mfi(utilde_mf); mfi.isValid(); ++mfi ) {

            // Get the index space of the valid region
            const Box& tileBox = mfi.tilebox();

            // call fortran subroutine
            // use macros in AMReX_ArrayLim.H to pass in each FAB's data,
            // lo/hi coordinates (including ghost cells), and/or the # of components
            // We will also pass "validBox", which specifies the "valid" region.
            mkutrans_1d(&lev, AMREX_ARLIM_ANYD(domainBox.loVect()),
                        AMREX_ARLIM_ANYD(domainBox.hiVect()),
                        AMREX_ARLIM_ANYD(tileBox.loVect()), AMREX_ARLIM_ANYD(tileBox.hiVect()),
                        BL_TO_FORTRAN_FAB(utilde_mf[mfi]), utilde_mf.nGrow(),
                        BL_TO_FORTRAN_FAB(ufull_mf[mfi]), ufull_mf.nGrow(),
                        BL_TO_FORTRAN_3D(utrans_mf[mfi]),
                        w0.dataPtr(), dx, &dt, bcs_u[0].data(), phys_bc.dataPtr());

        } // end MFIter loop

#elif (AMREX_SPACEDIM == 2)

#ifdef AMREX_USE_CUDA
        int* bc_f = prepare_bc(bcs_u[0].data(), 1);
#else
        const int* bc_f = bcs_u[0].data();
#endif
        MultiFab u_mf, v_mf;

        if (ppm_type == 0) {
            u_mf.define(grids[lev],dmap[lev],1,utilde[lev].nGrow());
            v_mf.define(grids[lev],dmap[lev],1,utilde[lev].nGrow());

            MultiFab::Copy(u_mf, utilde[lev], 0, 0, 1, utilde[lev].nGrow());
            MultiFab::Copy(v_mf, utilde[lev], 1, 0, 1, utilde[lev].nGrow());

        } else if (ppm_type == 1 || ppm_type == 2) {

            u_mf.define(grids[lev],dmap[lev],1,ufull[lev].nGrow());
            v_mf.define(grids[lev],dmap[lev],1,ufull[lev].nGrow());

            MultiFab::Copy(u_mf, ufull[lev], 0, 0, 1, ufull[lev].nGrow());
            MultiFab::Copy(v_mf, ufull[lev], 1, 0, 1, ufull[lev].nGrow());
        }


        // NOTE: don't tile, but threaded in fortran subroutine
        for ( MFIter mfi(utilde_mf, true); mfi.isValid(); ++mfi ) {

            // Get the index space of the valid region
            const Box& tileBox = mfi.tilebox();
            const Box& obx = amrex::grow(tileBox, 1);
            const Box& xbx = amrex::growHi(tileBox,0, 1);
            const Box& ybx = amrex::growHi(tileBox,1, 1);

            if (ppm_type == 0) {
                // we're going to reuse Ip here as slopex as it has the
                // correct number of ghost zones
#pragma gpu box(obx)
                slopex_2d(AMREX_INT_ANYD(obx.loVect()),
                          AMREX_INT_ANYD(obx.hiVect()),
                          BL_TO_FORTRAN_ANYD(u_mf[mfi]),
                          u_mf.nComp(),
                          BL_TO_FORTRAN_ANYD(Ip[mfi]),Ip.nComp(),
                          AMREX_INT_ANYD(domainBox.loVect()),
                          AMREX_INT_ANYD(domainBox.hiVect()),
                          1,bc_f,AMREX_SPACEDIM,1);

            } else {

#pragma gpu box(obx)
                ppm_2d(AMREX_INT_ANYD(obx.loVect()),
                       AMREX_INT_ANYD(obx.hiVect()),
                       BL_TO_FORTRAN_ANYD(utilde_mf[mfi]),
                       utilde_mf.nComp(),
                       BL_TO_FORTRAN_ANYD(u_mf[mfi]),
                       BL_TO_FORTRAN_ANYD(v_mf[mfi]),
                       BL_TO_FORTRAN_ANYD(Ip[mfi]),
                       BL_TO_FORTRAN_ANYD(Im[mfi]),
                       AMREX_INT_ANYD(domainBox.loVect()),
                       AMREX_INT_ANYD(domainBox.hiVect()),
                       bc_f, AMREX_REAL_ANYD(dx), dt, false,
                       1,1);

            }

            // call fortran subroutine
            // use macros in AMReX_ArrayLim.H to pass in each FAB's data,
            // lo/hi coordinates (including ghost cells), and/or the # of components
            // We will also pass "validBox", which specifies the "valid" region.
#pragma gpu box(xbx)
            mkutrans_2d(AMREX_INT_ANYD(xbx.loVect()),
                        AMREX_INT_ANYD(xbx.hiVect()),
                        lev, 1,
                        AMREX_INT_ANYD(domainBox.loVect()),
                        AMREX_INT_ANYD(domainBox.hiVect()),
                        BL_TO_FORTRAN_ANYD(utilde_mf[mfi]), utilde_mf.nComp(),
                        BL_TO_FORTRAN_ANYD(ufull_mf[mfi]), ufull_mf.nComp(),
                        BL_TO_FORTRAN_ANYD(utrans_mf[mfi]),
                        BL_TO_FORTRAN_ANYD(vtrans_mf[mfi]),
                        BL_TO_FORTRAN_ANYD(Ip[mfi]),
                        BL_TO_FORTRAN_ANYD(Im[mfi]),
                        w0.dataPtr(), AMREX_REAL_ANYD(dx), dt, bc_f,
                        phys_bc.dataPtr());

            if (ppm_type == 0) {
                // we're going to reuse Im here as slopey as it has the
                // correct number of ghost zones
#pragma gpu box(obx)
                slopey_2d(AMREX_INT_ANYD(obx.loVect()),
                          AMREX_INT_ANYD(obx.hiVect()),
                          BL_TO_FORTRAN_ANYD(v_mf[mfi]),
                          v_mf.nComp(),
                          BL_TO_FORTRAN_ANYD(Im[mfi]),Im.nComp(),
                          AMREX_INT_ANYD(domainBox.loVect()),
                          AMREX_INT_ANYD(domainBox.hiVect()),
                          1,bc_f,AMREX_SPACEDIM,2);

            } else {

#pragma gpu box(obx)
                ppm_2d(AMREX_INT_ANYD(obx.loVect()),
                       AMREX_INT_ANYD(obx.hiVect()),
                       BL_TO_FORTRAN_ANYD(utilde_mf[mfi]),
                       utilde_mf.nComp(),
                       BL_TO_FORTRAN_ANYD(u_mf[mfi]),
                       BL_TO_FORTRAN_ANYD(v_mf[mfi]),
                       BL_TO_FORTRAN_ANYD(Ip[mfi]),
                       BL_TO_FORTRAN_ANYD(Im[mfi]),
                       AMREX_INT_ANYD(domainBox.loVect()),
                       AMREX_INT_ANYD(domainBox.hiVect()),
                       bc_f, AMREX_REAL_ANYD(dx), dt, false,
                       2,2);
            }

#pragma gpu box(ybx)
            mkutrans_2d(AMREX_INT_ANYD(ybx.loVect()),
                        AMREX_INT_ANYD(ybx.hiVect()),
                        lev, 2,
                        AMREX_INT_ANYD(domainBox.loVect()),
                        AMREX_INT_ANYD(domainBox.hiVect()),
                        BL_TO_FORTRAN_ANYD(utilde_mf[mfi]), utilde_mf.nComp(),
                        BL_TO_FORTRAN_ANYD(ufull_mf[mfi]), ufull_mf.nComp(),
                        BL_TO_FORTRAN_ANYD(utrans_mf[mfi]),
                        BL_TO_FORTRAN_ANYD(vtrans_mf[mfi]),
                        BL_TO_FORTRAN_ANYD(Ip[mfi]),
                        BL_TO_FORTRAN_ANYD(Im[mfi]),
                        w0.dataPtr(), AMREX_REAL_ANYD(dx), dt, bc_f,
                        phys_bc.dataPtr());

        } // end MFIter loop

#ifdef AMREX_USE_CUDA
        clean_bc(bc_f);
#endif

# else
// AMREX_SPACEDIM == 3

#ifdef AMREX_USE_CUDA
        int* bc_f = prepare_bc(bcs_u[0].data(), 1);
#else
        const int* bc_f = bcs_u[0].data();
#endif

        MultiFab u_mf, v_mf, w_mf;

        if (ppm_type == 0) {
            u_mf.define(grids[lev],dmap[lev],1,utilde[lev].nGrow());
            v_mf.define(grids[lev],dmap[lev],1,utilde[lev].nGrow());
            w_mf.define(grids[lev],dmap[lev],1,utilde[lev].nGrow());

            MultiFab::Copy(u_mf, utilde[lev], 0, 0, 1, utilde[lev].nGrow());
            MultiFab::Copy(v_mf, utilde[lev], 1, 0, 1, utilde[lev].nGrow());
            MultiFab::Copy(w_mf, utilde[lev], 2, 0, 1, utilde[lev].nGrow());

        } else if (ppm_type == 1 || ppm_type == 2) {

            u_mf.define(grids[lev],dmap[lev],1,ufull[lev].nGrow());
            v_mf.define(grids[lev],dmap[lev],1,ufull[lev].nGrow());
            w_mf.define(grids[lev],dmap[lev],1,ufull[lev].nGrow());

            MultiFab::Copy(u_mf, ufull[lev], 0, 0, 1, ufull[lev].nGrow());
            MultiFab::Copy(v_mf, ufull[lev], 1, 0, 1, ufull[lev].nGrow());
            MultiFab::Copy(w_mf, ufull[lev], 2, 0, 1, ufull[lev].nGrow());
        }

        for ( MFIter mfi(utilde_mf); mfi.isValid(); ++mfi ) {

            // Get the index space of the valid region
            const Box& tileBox = mfi.tilebox();
            const Box& obx = amrex::grow(tileBox, 1);
            const Box& xbx = amrex::growHi(tileBox, 0, 1);
            const Box& ybx = amrex::growHi(tileBox, 1, 1);
            const Box& zbx = amrex::growHi(tileBox, 2, 1);

            // call fortran subroutine
            // use macros in AMReX_ArrayLim.H to pass in each FAB's data,
            // lo/hi coordinates (including ghost cells), and/or the # of components
            // We will also pass "validBox", which specifies the "valid" region.

            // x-direction
            if (ppm_type == 0) {
                // we're going to reuse Ip here as slopex as it has the
                // correct number of ghost zones

#pragma gpu box(obx)
                slopex_2d(AMREX_INT_ANYD(obx.loVect()),
                          AMREX_INT_ANYD(obx.hiVect()),
                          BL_TO_FORTRAN_ANYD(u_mf[mfi]),
                          u_mf.nComp(),
                          BL_TO_FORTRAN_ANYD(Ip[mfi]),Ip.nComp(),
                          AMREX_INT_ANYD(domainBox.loVect()),
                          AMREX_INT_ANYD(domainBox.hiVect()),
                          1,bc_f,AMREX_SPACEDIM,1);
            } else {
#pragma gpu box(obx)
                ppm_3d(AMREX_INT_ANYD(obx.loVect()),
                       AMREX_INT_ANYD(obx.hiVect()),
                       BL_TO_FORTRAN_ANYD(utilde_mf[mfi]),
                       utilde_mf.nComp(),
                       BL_TO_FORTRAN_ANYD(u_mf[mfi]),
                       BL_TO_FORTRAN_ANYD(v_mf[mfi]),
                       BL_TO_FORTRAN_ANYD(w_mf[mfi]),
                       BL_TO_FORTRAN_ANYD(Ip[mfi]),
                       BL_TO_FORTRAN_ANYD(Im[mfi]),
                       AMREX_INT_ANYD(domainBox.loVect()),
                       AMREX_INT_ANYD(domainBox.hiVect()),
                       bc_f, AMREX_REAL_ANYD(dx), dt, false,
                       1,1);
            }

#pragma gpu box(xbx)
            mkutrans_3d(AMREX_INT_ANYD(xbx.loVect()),
                        AMREX_INT_ANYD(xbx.hiVect()),
                        lev, 1,
                        AMREX_INT_ANYD(domainBox.loVect()),
                        AMREX_INT_ANYD(domainBox.hiVect()),
                        BL_TO_FORTRAN_ANYD(utilde_mf[mfi]), utilde_mf.nComp(),
                        BL_TO_FORTRAN_ANYD(ufull_mf[mfi]), ufull_mf.nComp(),
                        BL_TO_FORTRAN_ANYD(utrans_mf[mfi]),
                        BL_TO_FORTRAN_ANYD(vtrans_mf[mfi]),
                        BL_TO_FORTRAN_ANYD(wtrans_mf[mfi]),
                        BL_TO_FORTRAN_ANYD(Ip[mfi]),
                        BL_TO_FORTRAN_ANYD(Im[mfi]),
                        BL_TO_FORTRAN_ANYD(w0macx_mf[mfi]),
                        BL_TO_FORTRAN_ANYD(w0macy_mf[mfi]),
                        BL_TO_FORTRAN_ANYD(w0macz_mf[mfi]),
                        w0.dataPtr(), AMREX_REAL_ANYD(dx), dt, bc_f, phys_bc.dataPtr());

            // y-direction
            if (ppm_type == 0) {
                // we're going to reuse Im here as slopey as it has the
                // correct number of ghost zones

#pragma gpu box(obx)
                slopey_2d(AMREX_INT_ANYD(obx.loVect()),
                          AMREX_INT_ANYD(obx.hiVect()),
                          BL_TO_FORTRAN_ANYD(v_mf[mfi]),
                          v_mf.nComp(),
                          BL_TO_FORTRAN_ANYD(Im[mfi]),Im.nComp(),
                          AMREX_INT_ANYD(domainBox.loVect()),
                          AMREX_INT_ANYD(domainBox.hiVect()),
                          1,bc_f,AMREX_SPACEDIM,2);
            } else {
#pragma gpu box(obx)
                ppm_3d(AMREX_INT_ANYD(obx.loVect()),
                       AMREX_INT_ANYD(obx.hiVect()),
                       BL_TO_FORTRAN_ANYD(utilde_mf[mfi]),
                       utilde_mf.nComp(),
                       BL_TO_FORTRAN_ANYD(u_mf[mfi]),
                       BL_TO_FORTRAN_ANYD(v_mf[mfi]),
                       BL_TO_FORTRAN_ANYD(w_mf[mfi]),
                       BL_TO_FORTRAN_ANYD(Ip[mfi]),
                       BL_TO_FORTRAN_ANYD(Im[mfi]),
                       AMREX_INT_ANYD(domainBox.loVect()),
                       AMREX_INT_ANYD(domainBox.hiVect()),
                       bc_f, AMREX_REAL_ANYD(dx), dt, false,
                       2,2);
            }

#pragma gpu box(ybx)
            mkutrans_3d(AMREX_INT_ANYD(ybx.loVect()),
                        AMREX_INT_ANYD(ybx.hiVect()),
                        lev, 2,
                        AMREX_INT_ANYD(domainBox.loVect()),
                        AMREX_INT_ANYD(domainBox.hiVect()),
                        BL_TO_FORTRAN_ANYD(utilde_mf[mfi]), utilde_mf.nComp(),
                        BL_TO_FORTRAN_ANYD(ufull_mf[mfi]), ufull_mf.nComp(),
                        BL_TO_FORTRAN_ANYD(utrans_mf[mfi]),
                        BL_TO_FORTRAN_ANYD(vtrans_mf[mfi]),
                        BL_TO_FORTRAN_ANYD(wtrans_mf[mfi]),
                        BL_TO_FORTRAN_ANYD(Ip[mfi]),
                        BL_TO_FORTRAN_ANYD(Im[mfi]),
                        BL_TO_FORTRAN_ANYD(w0macx_mf[mfi]),
                        BL_TO_FORTRAN_ANYD(w0macy_mf[mfi]),
                        BL_TO_FORTRAN_ANYD(w0macz_mf[mfi]),
                        w0.dataPtr(), AMREX_REAL_ANYD(dx), dt, bc_f, phys_bc.dataPtr());

            // z-direction
            if (ppm_type == 0) {
                // we're going to reuse Im here as slopez as it has the
                // correct number of ghost zones

#pragma gpu box(obx)
                slopez_3d(AMREX_INT_ANYD(obx.loVect()),
                          AMREX_INT_ANYD(obx.hiVect()),
                          BL_TO_FORTRAN_ANYD(w_mf[mfi]),
                          w_mf.nComp(),
                          BL_TO_FORTRAN_ANYD(Im[mfi]),Im.nComp(),
                          AMREX_INT_ANYD(domainBox.loVect()),
                          AMREX_INT_ANYD(domainBox.hiVect()),
                          1,bc_f,AMREX_SPACEDIM,3);
            } else {
#pragma gpu box(obx)
                ppm_3d(AMREX_INT_ANYD(obx.loVect()),
                       AMREX_INT_ANYD(obx.hiVect()),
                       BL_TO_FORTRAN_ANYD(utilde_mf[mfi]),
                       utilde_mf.nComp(),
                       BL_TO_FORTRAN_ANYD(u_mf[mfi]),
                       BL_TO_FORTRAN_ANYD(v_mf[mfi]),
                       BL_TO_FORTRAN_ANYD(w_mf[mfi]),
                       BL_TO_FORTRAN_ANYD(Ip[mfi]),
                       BL_TO_FORTRAN_ANYD(Im[mfi]),
                       AMREX_INT_ANYD(domainBox.loVect()),
                       AMREX_INT_ANYD(domainBox.hiVect()),
                       bc_f, AMREX_REAL_ANYD(dx), dt, false,
                       3,3);
            }

#pragma gpu box(zbx)
            mkutrans_3d(AMREX_INT_ANYD(zbx.loVect()),
                        AMREX_INT_ANYD(zbx.hiVect()),
                        lev, 3,
                        AMREX_INT_ANYD(domainBox.loVect()),
                        AMREX_INT_ANYD(domainBox.hiVect()),
                        BL_TO_FORTRAN_ANYD(utilde_mf[mfi]), utilde_mf.nComp(),
                        BL_TO_FORTRAN_ANYD(ufull_mf[mfi]), ufull_mf.nComp(),
                        BL_TO_FORTRAN_ANYD(utrans_mf[mfi]),
                        BL_TO_FORTRAN_ANYD(vtrans_mf[mfi]),
                        BL_TO_FORTRAN_ANYD(wtrans_mf[mfi]),
                        BL_TO_FORTRAN_ANYD(Ip[mfi]),
                        BL_TO_FORTRAN_ANYD(Im[mfi]),
                        BL_TO_FORTRAN_ANYD(w0macx_mf[mfi]),
                        BL_TO_FORTRAN_ANYD(w0macy_mf[mfi]),
                        BL_TO_FORTRAN_ANYD(w0macz_mf[mfi]),
                        w0.dataPtr(), AMREX_REAL_ANYD(dx), dt, bc_f, phys_bc.dataPtr());

        } // end MFIter loop

#ifdef AMREX_USE_CUDA
        clean_bc(bc_f);
#endif


#endif // end if AMREX_SPACEDIM
    } // end loop over levels

    if (finest_level == 0) {
        // fill periodic ghost cells
        for (int lev=0; lev<=finest_level; ++lev) {
            for (int d=0; d<AMREX_SPACEDIM; ++d) {
                utrans[lev][d].FillBoundary(geom[lev].periodicity());
            }
        }

        // fill ghost cells behind physical boundaries
        FillUmacGhost(utrans);
    } else {
        // edge_restriction
        AverageDownFaces(utrans);

        // fill ghost cells for all levels
        FillPatchUedge(utrans);
    }

#ifdef AMREX_USE_CUDA
    // turn off GPU
    if (not_launched) Gpu::setLaunchRegion(false);
#endif

}

void
Maestro::VelPred (const Vector<MultiFab>& utilde,
                  const Vector<MultiFab>& ufull,
                  const Vector<std::array< MultiFab, AMREX_SPACEDIM > >& utrans,
                  Vector<std::array< MultiFab, AMREX_SPACEDIM > >& umac,
                  const Vector<std::array< MultiFab, AMREX_SPACEDIM > >& w0mac,
                  const Vector<MultiFab>& force)
{
    // timer for profiling
    BL_PROFILE_VAR("Maestro::VelPred()", VelPred);

#ifdef AMREX_USE_CUDA
    auto not_launched = Gpu::notInLaunchRegion();
    // turn on GPU
    if (not_launched) Gpu::setLaunchRegion(true);
#endif

    for (int lev=0; lev<=finest_level; ++lev) {

        // Get the index space and grid spacing of the domain
        const Box& domainBox = geom[lev].Domain();
        const Real* dx = geom[lev].CellSize();

        // get references to the MultiFabs at level lev
        const MultiFab& utilde_mf  = utilde[lev];
        const MultiFab& ufull_mf   = ufull[lev];
        MultiFab& umac_mf    = umac[lev][0];
        const MultiFab& utrans_mf  = utrans[lev][0];
#if (AMREX_SPACEDIM >= 2)
        const MultiFab& vtrans_mf  = utrans[lev][1];
        MultiFab& vmac_mf    = umac[lev][1];
        MultiFab Ipu, Imu, Ipv, Imv;
        Ipu.define(grids[lev],dmap[lev],AMREX_SPACEDIM,1);
        Imu.define(grids[lev],dmap[lev],AMREX_SPACEDIM,1);
        Ipv.define(grids[lev],dmap[lev],AMREX_SPACEDIM,1);
        Imv.define(grids[lev],dmap[lev],AMREX_SPACEDIM,1);

        MultiFab Ipfx, Imfx, Ipfy, Imfy;
        Ipfx.define(grids[lev],dmap[lev],AMREX_SPACEDIM,1);
        Imfx.define(grids[lev],dmap[lev],AMREX_SPACEDIM,1);
        Ipfy.define(grids[lev],dmap[lev],AMREX_SPACEDIM,1);
        Imfy.define(grids[lev],dmap[lev],AMREX_SPACEDIM,1);

        MultiFab ulx, urx, uimhx, uly, ury, uimhy;
        ulx.define(grids[lev],dmap[lev],AMREX_SPACEDIM,1);
        urx.define(grids[lev],dmap[lev],AMREX_SPACEDIM,1);
        uimhx.define(grids[lev],dmap[lev],AMREX_SPACEDIM,1);
        uly.define(grids[lev],dmap[lev],AMREX_SPACEDIM,1);
        ury.define(grids[lev],dmap[lev],AMREX_SPACEDIM,1);
        uimhy.define(grids[lev],dmap[lev],AMREX_SPACEDIM,1);
#if (AMREX_SPACEDIM == 3)
        const MultiFab& wtrans_mf  = utrans[lev][2];
        MultiFab& wmac_mf    = umac[lev][2];
        const MultiFab& w0macx_mf  = w0mac[lev][0];
        const MultiFab& w0macy_mf  = w0mac[lev][1];
        const MultiFab& w0macz_mf  = w0mac[lev][2];

        MultiFab Ipw, Imw;
        Ipw.define(grids[lev],dmap[lev],AMREX_SPACEDIM,1);
        Imw.define(grids[lev],dmap[lev],AMREX_SPACEDIM,1);

        MultiFab Ipfz, Imfz;
        Ipfz.define(grids[lev],dmap[lev],AMREX_SPACEDIM,1);
        Imfz.define(grids[lev],dmap[lev],AMREX_SPACEDIM,1);

        MultiFab ulz, urz, uimhz;
        ulz.define(grids[lev],dmap[lev],AMREX_SPACEDIM,1);
        urz.define(grids[lev],dmap[lev],AMREX_SPACEDIM,1);
        uimhz.define(grids[lev],dmap[lev],AMREX_SPACEDIM,1);

        MultiFab uimhyz, uimhzy, vimhxz, vimhzx, wimhxy, wimhyx;
        uimhyz.define(grids[lev],dmap[lev],1,1);
        uimhzy.define(grids[lev],dmap[lev],1,1);
        vimhxz.define(grids[lev],dmap[lev],1,1);
        vimhzx.define(grids[lev],dmap[lev],1,1);
        wimhxy.define(grids[lev],dmap[lev],1,1);
        wimhyx.define(grids[lev],dmap[lev],1,1);
#endif
#endif
        const MultiFab& force_mf = force[lev];

        // loop over boxes (make sure mfi takes a cell-centered multifab as an argument)

#if (AMREX_SPACEDIM == 1)

        // NOTE: don't tile, but threaded in fortran subroutine
        for ( MFIter mfi(utilde_mf); mfi.isValid(); ++mfi ) {

            // Get the index space of the valid region
            const Box& tileBox = mfi.tilebox();

            // call fortran subroutine
            // use macros in AMReX_ArrayLim.H to pass in each FAB's data,
            // lo/hi coordinates (including ghost cells), and/or the # of components
            // We will also pass "validBox", which specifies the "valid" region.

            velpred_1d(&lev, ARLIM_3D(domainBox.loVect()), ARLIM_3D(domainBox.hiVect()),
                       ARLIM_3D(tileBox.loVect()), ARLIM_3D(tileBox.hiVect()),
                       BL_TO_FORTRAN_FAB(utilde_mf[mfi]), utilde_mf.nGrow(),
                       BL_TO_FORTRAN_FAB(ufull_mf[mfi]), ufull_mf.nGrow(),
                       BL_TO_FORTRAN_3D(utrans_mf[mfi]),
                       BL_TO_FORTRAN_3D(umac_mf[mfi]),
                       BL_TO_FORTRAN_FAB(force_mf[mfi]), force_mf.nGrow(),
                       w0.dataPtr(), dx, &dt, bcs_u[0].data(), phys_bc.dataPtr());
        } // end MFIter loop

#elif (AMREX_SPACEDIM == 2)

#ifdef AMREX_USE_CUDA
        int* bc_f = prepare_bc(bcs_u[0].data(), 1);
#else
        const int* bc_f = bcs_u[0].data();
#endif
        MultiFab u_mf, v_mf;

        if (ppm_type == 0) {
            u_mf.define(grids[lev],dmap[lev],1,utilde[lev].nGrow());
            v_mf.define(grids[lev],dmap[lev],1,utilde[lev].nGrow());

            MultiFab::Copy(u_mf, utilde[lev], 0, 0, 1, utilde[lev].nGrow());
            MultiFab::Copy(v_mf, utilde[lev], 1, 0, 1, utilde[lev].nGrow());

        } else if (ppm_type == 1 || ppm_type == 2) {

            u_mf.define(grids[lev],dmap[lev],1,ufull[lev].nGrow());
            v_mf.define(grids[lev],dmap[lev],1,ufull[lev].nGrow());

            MultiFab::Copy(u_mf, ufull[lev], 0, 0, 1, ufull[lev].nGrow());
            MultiFab::Copy(v_mf, ufull[lev], 1, 0, 1, ufull[lev].nGrow());
        }

        for ( MFIter mfi(utilde_mf, true); mfi.isValid(); ++mfi ) {

            // Get the index space of the valid region
            const Box& tileBox = mfi.tilebox();
            const Box& obx = amrex::grow(tileBox, 1);
            const Box& xbx = amrex::growHi(tileBox,0, 1);
            const Box& ybx = amrex::growHi(tileBox,1, 1);
            const Box& mxbx = amrex::growLo(obx,0, -1);
            const Box& mybx = amrex::growLo(obx,1, -1);

            if (ppm_type == 0) {
                // we're going to reuse Ip here as slopex as it has the
                // correct number of ghost zones
#pragma gpu box(obx)
                slopex_2d(AMREX_INT_ANYD(obx.loVect()),
                          AMREX_INT_ANYD(obx.hiVect()),
                          BL_TO_FORTRAN_ANYD(utilde_mf[mfi]),
                          utilde_mf.nComp(),
                          BL_TO_FORTRAN_ANYD(Ipu[mfi]),Ipu.nComp(),
                          AMREX_INT_ANYD(domainBox.loVect()),
                          AMREX_INT_ANYD(domainBox.hiVect()),
                          2,bc_f,AMREX_SPACEDIM,1);

            } else {

#pragma gpu box(obx)
                ppm_2d(AMREX_INT_ANYD(obx.loVect()),
                       AMREX_INT_ANYD(obx.hiVect()),
                       BL_TO_FORTRAN_ANYD(utilde_mf[mfi]),
                       utilde_mf.nComp(),
                       BL_TO_FORTRAN_ANYD(u_mf[mfi]),
                       BL_TO_FORTRAN_ANYD(v_mf[mfi]),
                       BL_TO_FORTRAN_ANYD(Ipu[mfi]),
                       BL_TO_FORTRAN_ANYD(Imu[mfi]),
                       AMREX_INT_ANYD(domainBox.loVect()),
                       AMREX_INT_ANYD(domainBox.hiVect()),
                       bc_f, AMREX_REAL_ANYD(dx), dt, false,
                       1,1);

                if (ppm_trace_forces == 1) {
#pragma gpu box(obx)
                    ppm_2d(AMREX_INT_ANYD(obx.loVect()),
                           AMREX_INT_ANYD(obx.hiVect()),
                           BL_TO_FORTRAN_ANYD(force_mf[mfi]),
                           force_mf.nComp(),
                           BL_TO_FORTRAN_ANYD(u_mf[mfi]),
                           BL_TO_FORTRAN_ANYD(v_mf[mfi]),
                           BL_TO_FORTRAN_ANYD(Ipfx[mfi]),
                           BL_TO_FORTRAN_ANYD(Imfx[mfi]),
                           AMREX_INT_ANYD(domainBox.loVect()),
                           AMREX_INT_ANYD(domainBox.hiVect()),
                           bc_f, AMREX_REAL_ANYD(dx), dt, false,
                           1,1);
                }
            }

            if (ppm_type == 0) {
                // we're going to reuse Im here as slopey as it has the
                // correct number of ghost zones
#pragma gpu box(obx)
                slopey_2d(AMREX_INT_ANYD(obx.loVect()),
                          AMREX_INT_ANYD(obx.hiVect()),
                          BL_TO_FORTRAN_ANYD(utilde_mf[mfi]),
                          utilde_mf.nComp(),
                          BL_TO_FORTRAN_ANYD(Imv[mfi]),Imv.nComp(),
                          AMREX_INT_ANYD(domainBox.loVect()),
                          AMREX_INT_ANYD(domainBox.hiVect()),
                          2,bc_f,AMREX_SPACEDIM,1);

            } else {

#pragma gpu box(obx)
                ppm_2d(AMREX_INT_ANYD(obx.loVect()),
                       AMREX_INT_ANYD(obx.hiVect()),
                       BL_TO_FORTRAN_ANYD(utilde_mf[mfi]),
                       utilde_mf.nComp(),
                       BL_TO_FORTRAN_ANYD(u_mf[mfi]),
                       BL_TO_FORTRAN_ANYD(v_mf[mfi]),
                       BL_TO_FORTRAN_ANYD(Ipv[mfi]),
                       BL_TO_FORTRAN_ANYD(Imv[mfi]),
                       AMREX_INT_ANYD(domainBox.loVect()),
                       AMREX_INT_ANYD(domainBox.hiVect()),
                       bc_f, AMREX_REAL_ANYD(dx), dt, false,
                       2,2);

                if (ppm_trace_forces == 1) {
#pragma gpu box(obx)
                    ppm_2d(AMREX_INT_ANYD(obx.loVect()),
                           AMREX_INT_ANYD(obx.hiVect()),
                           BL_TO_FORTRAN_ANYD(force_mf[mfi]),
                           force_mf.nComp(),
                           BL_TO_FORTRAN_ANYD(u_mf[mfi]),
                           BL_TO_FORTRAN_ANYD(v_mf[mfi]),
                           BL_TO_FORTRAN_ANYD(Ipfy[mfi]),
                           BL_TO_FORTRAN_ANYD(Imfy[mfi]),
                           AMREX_INT_ANYD(domainBox.loVect()),
                           AMREX_INT_ANYD(domainBox.hiVect()),
                           bc_f, AMREX_REAL_ANYD(dx), dt, false,
                           2,2);
                }
            }

            // call fortran subroutine
            // use macros in AMReX_ArrayLim.H to pass in each FAB's data,
            // lo/hi coordinates (including ghost cells), and/or the # of components
            // We will also pass "validBox", which specifies the "valid" region.

            // x-direction
#pragma gpu box(mxbx)
            velpred_interface_2d(AMREX_INT_ANYD(mxbx.loVect()), AMREX_INT_ANYD(mxbx.hiVect()),1,
                                 AMREX_INT_ANYD(domainBox.loVect()), AMREX_INT_ANYD(domainBox.hiVect()),
                                 BL_TO_FORTRAN_ANYD(utilde_mf[mfi]), utilde_mf.nComp(), utilde_mf.nGrow(),
                                 BL_TO_FORTRAN_ANYD(ufull_mf[mfi]), ufull_mf.nComp(), ufull_mf.nGrow(),
                                 BL_TO_FORTRAN_ANYD(utrans_mf[mfi]),
                                 BL_TO_FORTRAN_ANYD(Imu[mfi]),
                                 BL_TO_FORTRAN_ANYD(Ipu[mfi]),
                                 BL_TO_FORTRAN_ANYD(Imv[mfi]),
                                 BL_TO_FORTRAN_ANYD(Ipv[mfi]),
                                 BL_TO_FORTRAN_ANYD(ulx[mfi]),
                                 BL_TO_FORTRAN_ANYD(urx[mfi]),
                                 BL_TO_FORTRAN_ANYD(uimhx[mfi]),
                                 AMREX_REAL_ANYD(dx), dt, bc_f, phys_bc.dataPtr());

            // y-direction
#pragma gpu box(mybx)
            velpred_interface_2d(AMREX_INT_ANYD(mybx.loVect()), AMREX_INT_ANYD(mybx.hiVect()),2,
                                 AMREX_INT_ANYD(domainBox.loVect()), AMREX_INT_ANYD(domainBox.hiVect()),
                                 BL_TO_FORTRAN_ANYD(utilde_mf[mfi]), utilde_mf.nComp(), utilde_mf.nGrow(),
                                 BL_TO_FORTRAN_ANYD(ufull_mf[mfi]), ufull_mf.nComp(), ufull_mf.nGrow(),
                                 BL_TO_FORTRAN_ANYD(vtrans_mf[mfi]),
                                 BL_TO_FORTRAN_ANYD(Imu[mfi]),
                                 BL_TO_FORTRAN_ANYD(Ipu[mfi]),
                                 BL_TO_FORTRAN_ANYD(Imv[mfi]),
                                 BL_TO_FORTRAN_ANYD(Ipv[mfi]),
                                 BL_TO_FORTRAN_ANYD(uly[mfi]),
                                 BL_TO_FORTRAN_ANYD(ury[mfi]),
                                 BL_TO_FORTRAN_ANYD(uimhy[mfi]),
                                 AMREX_REAL_ANYD(dx), dt, bc_f, phys_bc.dataPtr());

            // x-direction
#pragma gpu box(xbx)
            velpred_2d(AMREX_INT_ANYD(xbx.loVect()), AMREX_INT_ANYD(xbx.hiVect()),lev,1,
                       AMREX_INT_ANYD(domainBox.loVect()), AMREX_INT_ANYD(domainBox.hiVect()),
                       BL_TO_FORTRAN_ANYD(utilde_mf[mfi]), utilde_mf.nComp(), utilde_mf.nGrow(),
                       BL_TO_FORTRAN_ANYD(utrans_mf[mfi]),
                       BL_TO_FORTRAN_ANYD(vtrans_mf[mfi]),
                       BL_TO_FORTRAN_ANYD(umac_mf[mfi]),
                       BL_TO_FORTRAN_ANYD(vmac_mf[mfi]),
                       BL_TO_FORTRAN_ANYD(Imfx[mfi]),
                       BL_TO_FORTRAN_ANYD(Ipfx[mfi]),
                       BL_TO_FORTRAN_ANYD(ulx[mfi]),
                       BL_TO_FORTRAN_ANYD(urx[mfi]),
                       BL_TO_FORTRAN_ANYD(uimhx[mfi]),
                       BL_TO_FORTRAN_ANYD(uly[mfi]),
                       BL_TO_FORTRAN_ANYD(ury[mfi]),
                       BL_TO_FORTRAN_ANYD(uimhy[mfi]),
                       BL_TO_FORTRAN_ANYD(force_mf[mfi]), force_mf.nComp(), force_mf.nGrow(),
                       w0.dataPtr(), AMREX_REAL_ANYD(dx), dt, bc_f, phys_bc.dataPtr());

            // y-direction
#pragma gpu box(ybx)
            velpred_2d(AMREX_INT_ANYD(ybx.loVect()), AMREX_INT_ANYD(ybx.hiVect()),lev,2,
                       AMREX_INT_ANYD(domainBox.loVect()), AMREX_INT_ANYD(domainBox.hiVect()),
                       BL_TO_FORTRAN_ANYD(utilde_mf[mfi]), utilde_mf.nComp(), utilde_mf.nGrow(),
                       BL_TO_FORTRAN_ANYD(utrans_mf[mfi]),
                       BL_TO_FORTRAN_ANYD(vtrans_mf[mfi]),
                       BL_TO_FORTRAN_ANYD(umac_mf[mfi]),
                       BL_TO_FORTRAN_ANYD(vmac_mf[mfi]),
                       BL_TO_FORTRAN_ANYD(Imfy[mfi]),
                       BL_TO_FORTRAN_ANYD(Ipfy[mfi]),
                       BL_TO_FORTRAN_ANYD(ulx[mfi]),
                       BL_TO_FORTRAN_ANYD(urx[mfi]),
                       BL_TO_FORTRAN_ANYD(uimhx[mfi]),
                       BL_TO_FORTRAN_ANYD(uly[mfi]),
                       BL_TO_FORTRAN_ANYD(ury[mfi]),
                       BL_TO_FORTRAN_ANYD(uimhy[mfi]),
                       BL_TO_FORTRAN_ANYD(force_mf[mfi]), force_mf.nComp(), force_mf.nGrow(),
                       w0.dataPtr(), AMREX_REAL_ANYD(dx), dt, bc_f, phys_bc.dataPtr());
        } // end MFIter loop

#ifdef AMREX_USE_CUDA
        clean_bc(bc_f);
#endif

#elif (AMREX_SPACEDIM == 3)

#ifdef AMREX_USE_CUDA
        int* bc_f = prepare_bc(bcs_u[0].data(), 1);
#else
        const int* bc_f = bcs_u[0].data();
#endif
        MultiFab u_mf, v_mf, w_mf;

        if (ppm_type == 0) {
            u_mf.define(grids[lev],dmap[lev],1,utilde[lev].nGrow());
            v_mf.define(grids[lev],dmap[lev],1,utilde[lev].nGrow());
            w_mf.define(grids[lev],dmap[lev],1,utilde[lev].nGrow());

            MultiFab::Copy(u_mf, utilde[lev], 0, 0, 1, utilde[lev].nGrow());
            MultiFab::Copy(v_mf, utilde[lev], 1, 0, 1, utilde[lev].nGrow());
            MultiFab::Copy(w_mf, utilde[lev], 2, 0, 1, utilde[lev].nGrow());

        } else if (ppm_type == 1 || ppm_type == 2) {

            u_mf.define(grids[lev],dmap[lev],1,ufull[lev].nGrow());
            v_mf.define(grids[lev],dmap[lev],1,ufull[lev].nGrow());
            w_mf.define(grids[lev],dmap[lev],1,ufull[lev].nGrow());

            MultiFab::Copy(u_mf, ufull[lev], 0, 0, 1, ufull[lev].nGrow());
            MultiFab::Copy(v_mf, ufull[lev], 1, 0, 1, ufull[lev].nGrow());
            MultiFab::Copy(w_mf, ufull[lev], 2, 0, 1, ufull[lev].nGrow());
        }

        for ( MFIter mfi(utilde_mf, true); mfi.isValid(); ++mfi ) {

            // Get the index space of the valid region
            const Box& tileBox = mfi.tilebox();
            const Box& obx = amrex::grow(tileBox, 1);
            const Box& xbx = amrex::growHi(tileBox,0, 1);
            const Box& ybx = amrex::growHi(tileBox,1, 1);
            const Box& zbx = amrex::growHi(tileBox,2, 1);
            const Box& mxbx = amrex::growLo(obx,0, -1);
            const Box& mybx = amrex::growLo(obx,1, -1);
            const Box& mzbx = amrex::growLo(obx,2, -1);

            // x-direction
            if (ppm_type == 0) {
                // we're going to reuse Ip here as slopex as it has the
                // correct number of ghost zones
#pragma gpu box(obx)
                slopex_2d(AMREX_INT_ANYD(obx.loVect()),
                          AMREX_INT_ANYD(obx.hiVect()),
                          BL_TO_FORTRAN_ANYD(utilde_mf[mfi]),
                          utilde_mf.nComp(),
                          BL_TO_FORTRAN_ANYD(Ipu[mfi]),Ipu.nComp(),
                          AMREX_INT_ANYD(domainBox.loVect()),
                          AMREX_INT_ANYD(domainBox.hiVect()),
                          3,bc_f,AMREX_SPACEDIM,1);

            } else {

#pragma gpu box(obx)
                ppm_3d(AMREX_INT_ANYD(obx.loVect()),
                       AMREX_INT_ANYD(obx.hiVect()),
                       BL_TO_FORTRAN_ANYD(utilde_mf[mfi]),
                       utilde_mf.nComp(),
                       BL_TO_FORTRAN_ANYD(u_mf[mfi]),
                       BL_TO_FORTRAN_ANYD(v_mf[mfi]),
                       BL_TO_FORTRAN_ANYD(w_mf[mfi]),
                       BL_TO_FORTRAN_ANYD(Ipu[mfi]),
                       BL_TO_FORTRAN_ANYD(Imu[mfi]),
                       AMREX_INT_ANYD(domainBox.loVect()),
                       AMREX_INT_ANYD(domainBox.hiVect()),
                       bc_f, AMREX_REAL_ANYD(dx), dt, false,
                       1,1);

                if (ppm_trace_forces == 1) {
#pragma gpu box(obx)
                    ppm_3d(AMREX_INT_ANYD(obx.loVect()),
                           AMREX_INT_ANYD(obx.hiVect()),
                           BL_TO_FORTRAN_ANYD(force_mf[mfi]),
                           force_mf.nComp(),
                           BL_TO_FORTRAN_ANYD(u_mf[mfi]),
                           BL_TO_FORTRAN_ANYD(v_mf[mfi]),
                           BL_TO_FORTRAN_ANYD(w_mf[mfi]),
                           BL_TO_FORTRAN_ANYD(Ipfx[mfi]),
                           BL_TO_FORTRAN_ANYD(Imfx[mfi]),
                           AMREX_INT_ANYD(domainBox.loVect()),
                           AMREX_INT_ANYD(domainBox.hiVect()),
                           bc_f, AMREX_REAL_ANYD(dx), dt, false,
                           1,1);
                }
            }

            // y-direction
            if (ppm_type == 0) {
                // we're going to reuse Im here as slopey as it has the
                // correct number of ghost zones
#pragma gpu box(obx)
                slopey_2d(AMREX_INT_ANYD(obx.loVect()),
                          AMREX_INT_ANYD(obx.hiVect()),
                          BL_TO_FORTRAN_ANYD(utilde_mf[mfi]),
                          utilde_mf.nComp(),
                          BL_TO_FORTRAN_ANYD(Imv[mfi]),Imv.nComp(),
                          AMREX_INT_ANYD(domainBox.loVect()),
                          AMREX_INT_ANYD(domainBox.hiVect()),
                          3,bc_f,AMREX_SPACEDIM,1);

            } else {

#pragma gpu box(obx)
                ppm_3d(AMREX_INT_ANYD(obx.loVect()),
                       AMREX_INT_ANYD(obx.hiVect()),
                       BL_TO_FORTRAN_ANYD(utilde_mf[mfi]),
                       utilde_mf.nComp(),
                       BL_TO_FORTRAN_ANYD(u_mf[mfi]),
                       BL_TO_FORTRAN_ANYD(v_mf[mfi]),
                       BL_TO_FORTRAN_ANYD(w_mf[mfi]),
                       BL_TO_FORTRAN_ANYD(Ipv[mfi]),
                       BL_TO_FORTRAN_ANYD(Imv[mfi]),
                       AMREX_INT_ANYD(domainBox.loVect()),
                       AMREX_INT_ANYD(domainBox.hiVect()),
                       bc_f, AMREX_REAL_ANYD(dx), dt, false,
                       2,2);

                if (ppm_trace_forces == 1) {
#pragma gpu box(obx)
                    ppm_3d(AMREX_INT_ANYD(obx.loVect()),
                           AMREX_INT_ANYD(obx.hiVect()),
                           BL_TO_FORTRAN_ANYD(force_mf[mfi]),
                           force_mf.nComp(),
                           BL_TO_FORTRAN_ANYD(u_mf[mfi]),
                           BL_TO_FORTRAN_ANYD(v_mf[mfi]),
                           BL_TO_FORTRAN_ANYD(w_mf[mfi]),
                           BL_TO_FORTRAN_ANYD(Ipfy[mfi]),
                           BL_TO_FORTRAN_ANYD(Imfy[mfi]),
                           AMREX_INT_ANYD(domainBox.loVect()),
                           AMREX_INT_ANYD(domainBox.hiVect()),
                           bc_f, AMREX_REAL_ANYD(dx), dt, false,
                           2,2);
                }
            }

            // z-direction
            if (ppm_type == 0) {
                // we're going to reuse Im here as slopey as it has the
                // correct number of ghost zones
#pragma gpu box(obx)
                slopez_3d(AMREX_INT_ANYD(obx.loVect()),
                          AMREX_INT_ANYD(obx.hiVect()),
                          BL_TO_FORTRAN_ANYD(utilde_mf[mfi]),
                          utilde_mf.nComp(),
                          BL_TO_FORTRAN_ANYD(Imw[mfi]),Imw.nComp(),
                          AMREX_INT_ANYD(domainBox.loVect()),
                          AMREX_INT_ANYD(domainBox.hiVect()),
                          3,bc_f,AMREX_SPACEDIM,1);

            } else {

#pragma gpu box(obx)
                ppm_3d(AMREX_INT_ANYD(obx.loVect()),
                       AMREX_INT_ANYD(obx.hiVect()),
                       BL_TO_FORTRAN_ANYD(utilde_mf[mfi]),
                       utilde_mf.nComp(),
                       BL_TO_FORTRAN_ANYD(u_mf[mfi]),
                       BL_TO_FORTRAN_ANYD(v_mf[mfi]),
                       BL_TO_FORTRAN_ANYD(w_mf[mfi]),
                       BL_TO_FORTRAN_ANYD(Ipw[mfi]),
                       BL_TO_FORTRAN_ANYD(Imw[mfi]),
                       AMREX_INT_ANYD(domainBox.loVect()),
                       AMREX_INT_ANYD(domainBox.hiVect()),
                       bc_f, AMREX_REAL_ANYD(dx), dt, false,
                       3,3);

                if (ppm_trace_forces == 1) {
#pragma gpu box(obx)
                    ppm_3d(AMREX_INT_ANYD(obx.loVect()),
                           AMREX_INT_ANYD(obx.hiVect()),
                           BL_TO_FORTRAN_ANYD(force_mf[mfi]),
                           force_mf.nComp(),
                           BL_TO_FORTRAN_ANYD(u_mf[mfi]),
                           BL_TO_FORTRAN_ANYD(v_mf[mfi]),
                           BL_TO_FORTRAN_ANYD(w_mf[mfi]),
                           BL_TO_FORTRAN_ANYD(Ipfz[mfi]),
                           BL_TO_FORTRAN_ANYD(Imfz[mfi]),
                           AMREX_INT_ANYD(domainBox.loVect()),
                           AMREX_INT_ANYD(domainBox.hiVect()),
                           bc_f, AMREX_REAL_ANYD(dx), dt, false,
                           3,3);
                }
            }

            // x-direction
#pragma gpu box(mxbx)
            velpred_interface_3d(AMREX_INT_ANYD(mxbx.loVect()), AMREX_INT_ANYD(mxbx.hiVect()),1,
                                 AMREX_INT_ANYD(domainBox.loVect()), AMREX_INT_ANYD(domainBox.hiVect()),
                                 BL_TO_FORTRAN_ANYD(utilde_mf[mfi]), utilde_mf.nComp(), utilde_mf.nGrow(),
                                 BL_TO_FORTRAN_ANYD(ufull_mf[mfi]), ufull_mf.nComp(), ufull_mf.nGrow(),
                                 BL_TO_FORTRAN_ANYD(utrans_mf[mfi]),
                                 BL_TO_FORTRAN_ANYD(Imu[mfi]),
                                 BL_TO_FORTRAN_ANYD(Ipu[mfi]),
                                 BL_TO_FORTRAN_ANYD(Imv[mfi]),
                                 BL_TO_FORTRAN_ANYD(Ipv[mfi]),
                                 BL_TO_FORTRAN_ANYD(Imw[mfi]),
                                 BL_TO_FORTRAN_ANYD(Ipw[mfi]),
                                 BL_TO_FORTRAN_ANYD(ulx[mfi]),
                                 BL_TO_FORTRAN_ANYD(urx[mfi]),
                                 BL_TO_FORTRAN_ANYD(uimhx[mfi]),
                                 AMREX_REAL_ANYD(dx), dt, phys_bc.dataPtr());

            // y-direction
#pragma gpu box(mybx)
            velpred_interface_3d(AMREX_INT_ANYD(mybx.loVect()), AMREX_INT_ANYD(mybx.hiVect()),2,
                                 AMREX_INT_ANYD(domainBox.loVect()), AMREX_INT_ANYD(domainBox.hiVect()),
                                 BL_TO_FORTRAN_ANYD(utilde_mf[mfi]), utilde_mf.nComp(), utilde_mf.nGrow(),
                                 BL_TO_FORTRAN_ANYD(ufull_mf[mfi]), ufull_mf.nComp(), ufull_mf.nGrow(),
                                 BL_TO_FORTRAN_ANYD(vtrans_mf[mfi]),
                                 BL_TO_FORTRAN_ANYD(Imu[mfi]),
                                 BL_TO_FORTRAN_ANYD(Ipu[mfi]),
                                 BL_TO_FORTRAN_ANYD(Imv[mfi]),
                                 BL_TO_FORTRAN_ANYD(Ipv[mfi]),
                                 BL_TO_FORTRAN_ANYD(Imw[mfi]),
                                 BL_TO_FORTRAN_ANYD(Ipw[mfi]),
                                 BL_TO_FORTRAN_ANYD(uly[mfi]),
                                 BL_TO_FORTRAN_ANYD(ury[mfi]),
                                 BL_TO_FORTRAN_ANYD(uimhy[mfi]),
                                 AMREX_REAL_ANYD(dx), dt, phys_bc.dataPtr());

            // z-direction
#pragma gpu box(mzbx)
            velpred_interface_3d(AMREX_INT_ANYD(mzbx.loVect()), AMREX_INT_ANYD(mzbx.hiVect()),3,
                                 AMREX_INT_ANYD(domainBox.loVect()), AMREX_INT_ANYD(domainBox.hiVect()),
                                 BL_TO_FORTRAN_ANYD(utilde_mf[mfi]), utilde_mf.nComp(), utilde_mf.nGrow(),
                                 BL_TO_FORTRAN_ANYD(ufull_mf[mfi]), ufull_mf.nComp(), ufull_mf.nGrow(),
                                 BL_TO_FORTRAN_ANYD(wtrans_mf[mfi]),
                                 BL_TO_FORTRAN_ANYD(Imu[mfi]),
                                 BL_TO_FORTRAN_ANYD(Ipu[mfi]),
                                 BL_TO_FORTRAN_ANYD(Imv[mfi]),
                                 BL_TO_FORTRAN_ANYD(Ipv[mfi]),
                                 BL_TO_FORTRAN_ANYD(Imw[mfi]),
                                 BL_TO_FORTRAN_ANYD(Ipw[mfi]),
                                 BL_TO_FORTRAN_ANYD(ulz[mfi]),
                                 BL_TO_FORTRAN_ANYD(urz[mfi]),
                                 BL_TO_FORTRAN_ANYD(uimhz[mfi]),
                                 AMREX_REAL_ANYD(dx), dt, phys_bc.dataPtr());

            // uimhyz, 1, 2
            Box imhbox = amrex::grow(mfi.tilebox(), 0, 1);
            imhbox = amrex::growHi(imhbox, 1, 1);
#pragma gpu box(imhbox)
            velpred_transverse_3d(AMREX_INT_ANYD(imhbox.loVect()), AMREX_INT_ANYD(imhbox.hiVect()),1,2,
                                  AMREX_INT_ANYD(domainBox.loVect()), AMREX_INT_ANYD(domainBox.hiVect()),
                                  BL_TO_FORTRAN_ANYD(utilde_mf[mfi]), utilde_mf.nComp(), utilde_mf.nGrow(),
                                  BL_TO_FORTRAN_ANYD(utrans_mf[mfi]),
                                  BL_TO_FORTRAN_ANYD(vtrans_mf[mfi]),
                                  BL_TO_FORTRAN_ANYD(wtrans_mf[mfi]),
                                  BL_TO_FORTRAN_ANYD(ulx[mfi]),
                                  BL_TO_FORTRAN_ANYD(urx[mfi]),
                                  BL_TO_FORTRAN_ANYD(uimhx[mfi]),
                                  BL_TO_FORTRAN_ANYD(uly[mfi]),
                                  BL_TO_FORTRAN_ANYD(ury[mfi]),
                                  BL_TO_FORTRAN_ANYD(uimhy[mfi]),
                                  BL_TO_FORTRAN_ANYD(ulz[mfi]),
                                  BL_TO_FORTRAN_ANYD(urz[mfi]),
                                  BL_TO_FORTRAN_ANYD(uimhz[mfi]),
                                  BL_TO_FORTRAN_ANYD(uimhyz[mfi]),
                                  AMREX_REAL_ANYD(dx), dt, phys_bc.dataPtr());

            // uimhzy, 1, 3
            imhbox = amrex::grow(mfi.tilebox(), 0, 1);
            imhbox = amrex::growHi(imhbox, 2, 1);
#pragma gpu box(imhbox)
            velpred_transverse_3d(AMREX_INT_ANYD(imhbox.loVect()), AMREX_INT_ANYD(imhbox.hiVect()),1,3,
                                  AMREX_INT_ANYD(domainBox.loVect()), AMREX_INT_ANYD(domainBox.hiVect()),
                                  BL_TO_FORTRAN_ANYD(utilde_mf[mfi]), utilde_mf.nComp(), utilde_mf.nGrow(),
                                  BL_TO_FORTRAN_ANYD(utrans_mf[mfi]),
                                  BL_TO_FORTRAN_ANYD(vtrans_mf[mfi]),
                                  BL_TO_FORTRAN_ANYD(wtrans_mf[mfi]),
                                  BL_TO_FORTRAN_ANYD(ulx[mfi]),
                                  BL_TO_FORTRAN_ANYD(urx[mfi]),
                                  BL_TO_FORTRAN_ANYD(uimhx[mfi]),
                                  BL_TO_FORTRAN_ANYD(uly[mfi]),
                                  BL_TO_FORTRAN_ANYD(ury[mfi]),
                                  BL_TO_FORTRAN_ANYD(uimhy[mfi]),
                                  BL_TO_FORTRAN_ANYD(ulz[mfi]),
                                  BL_TO_FORTRAN_ANYD(urz[mfi]),
                                  BL_TO_FORTRAN_ANYD(uimhz[mfi]),
                                  BL_TO_FORTRAN_ANYD(uimhzy[mfi]),
                                  AMREX_REAL_ANYD(dx), dt, phys_bc.dataPtr());

            // vimhxz, 2, 1
            imhbox = amrex::grow(mfi.tilebox(), 1, 1);
            imhbox = amrex::growHi(imhbox, 0, 1);
#pragma gpu box(imhbox)
            velpred_transverse_3d(AMREX_INT_ANYD(imhbox.loVect()), AMREX_INT_ANYD(imhbox.hiVect()),2,1,
                                  AMREX_INT_ANYD(domainBox.loVect()), AMREX_INT_ANYD(domainBox.hiVect()),
                                  BL_TO_FORTRAN_ANYD(utilde_mf[mfi]), utilde_mf.nComp(), utilde_mf.nGrow(),
                                  BL_TO_FORTRAN_ANYD(utrans_mf[mfi]),
                                  BL_TO_FORTRAN_ANYD(vtrans_mf[mfi]),
                                  BL_TO_FORTRAN_ANYD(wtrans_mf[mfi]),
                                  BL_TO_FORTRAN_ANYD(ulx[mfi]),
                                  BL_TO_FORTRAN_ANYD(urx[mfi]),
                                  BL_TO_FORTRAN_ANYD(uimhx[mfi]),
                                  BL_TO_FORTRAN_ANYD(uly[mfi]),
                                  BL_TO_FORTRAN_ANYD(ury[mfi]),
                                  BL_TO_FORTRAN_ANYD(uimhy[mfi]),
                                  BL_TO_FORTRAN_ANYD(ulz[mfi]),
                                  BL_TO_FORTRAN_ANYD(urz[mfi]),
                                  BL_TO_FORTRAN_ANYD(uimhz[mfi]),
                                  BL_TO_FORTRAN_ANYD(vimhxz[mfi]),
                                  AMREX_REAL_ANYD(dx), dt, phys_bc.dataPtr());

            // vimhxz, 2, 3
            imhbox = amrex::grow(mfi.tilebox(), 1, 1);
            imhbox = amrex::growHi(imhbox, 2, 1);
#pragma gpu box(imhbox)
            velpred_transverse_3d(AMREX_INT_ANYD(imhbox.loVect()), AMREX_INT_ANYD(imhbox.hiVect()),2,3,
                                  AMREX_INT_ANYD(domainBox.loVect()), AMREX_INT_ANYD(domainBox.hiVect()),
                                  BL_TO_FORTRAN_ANYD(utilde_mf[mfi]), utilde_mf.nComp(), utilde_mf.nGrow(),
                                  BL_TO_FORTRAN_ANYD(utrans_mf[mfi]),
                                  BL_TO_FORTRAN_ANYD(vtrans_mf[mfi]),
                                  BL_TO_FORTRAN_ANYD(wtrans_mf[mfi]),
                                  BL_TO_FORTRAN_ANYD(ulx[mfi]),
                                  BL_TO_FORTRAN_ANYD(urx[mfi]),
                                  BL_TO_FORTRAN_ANYD(uimhx[mfi]),
                                  BL_TO_FORTRAN_ANYD(uly[mfi]),
                                  BL_TO_FORTRAN_ANYD(ury[mfi]),
                                  BL_TO_FORTRAN_ANYD(uimhy[mfi]),
                                  BL_TO_FORTRAN_ANYD(ulz[mfi]),
                                  BL_TO_FORTRAN_ANYD(urz[mfi]),
                                  BL_TO_FORTRAN_ANYD(uimhz[mfi]),
                                  BL_TO_FORTRAN_ANYD(vimhzx[mfi]),
                                  AMREX_REAL_ANYD(dx), dt, phys_bc.dataPtr());

            // wimhxy, 3, 1
            imhbox = amrex::grow(mfi.tilebox(), 2, 1);
            imhbox = amrex::growHi(imhbox, 0, 1);
#pragma gpu box(imhbox)
            velpred_transverse_3d(AMREX_INT_ANYD(imhbox.loVect()), AMREX_INT_ANYD(imhbox.hiVect()),3,1,
                                  AMREX_INT_ANYD(domainBox.loVect()), AMREX_INT_ANYD(domainBox.hiVect()),
                                  BL_TO_FORTRAN_ANYD(utilde_mf[mfi]), utilde_mf.nComp(), utilde_mf.nGrow(),
                                  BL_TO_FORTRAN_ANYD(utrans_mf[mfi]),
                                  BL_TO_FORTRAN_ANYD(vtrans_mf[mfi]),
                                  BL_TO_FORTRAN_ANYD(wtrans_mf[mfi]),
                                  BL_TO_FORTRAN_ANYD(ulx[mfi]),
                                  BL_TO_FORTRAN_ANYD(urx[mfi]),
                                  BL_TO_FORTRAN_ANYD(uimhx[mfi]),
                                  BL_TO_FORTRAN_ANYD(uly[mfi]),
                                  BL_TO_FORTRAN_ANYD(ury[mfi]),
                                  BL_TO_FORTRAN_ANYD(uimhy[mfi]),
                                  BL_TO_FORTRAN_ANYD(ulz[mfi]),
                                  BL_TO_FORTRAN_ANYD(urz[mfi]),
                                  BL_TO_FORTRAN_ANYD(uimhz[mfi]),
                                  BL_TO_FORTRAN_ANYD(wimhxy[mfi]),
                                  AMREX_REAL_ANYD(dx), dt, phys_bc.dataPtr());

            // wimhyx, 3, 2
            imhbox = amrex::grow(mfi.tilebox(), 2, 1);
            imhbox = amrex::growHi(imhbox, 1, 1);
#pragma gpu box(imhbox)
            velpred_transverse_3d(AMREX_INT_ANYD(imhbox.loVect()), AMREX_INT_ANYD(imhbox.hiVect()),3,2,
                                  AMREX_INT_ANYD(domainBox.loVect()), AMREX_INT_ANYD(domainBox.hiVect()),
                                  BL_TO_FORTRAN_ANYD(utilde_mf[mfi]), utilde_mf.nComp(), utilde_mf.nGrow(),
                                  BL_TO_FORTRAN_ANYD(utrans_mf[mfi]),
                                  BL_TO_FORTRAN_ANYD(vtrans_mf[mfi]),
                                  BL_TO_FORTRAN_ANYD(wtrans_mf[mfi]),
                                  BL_TO_FORTRAN_ANYD(ulx[mfi]),
                                  BL_TO_FORTRAN_ANYD(urx[mfi]),
                                  BL_TO_FORTRAN_ANYD(uimhx[mfi]),
                                  BL_TO_FORTRAN_ANYD(uly[mfi]),
                                  BL_TO_FORTRAN_ANYD(ury[mfi]),
                                  BL_TO_FORTRAN_ANYD(uimhy[mfi]),
                                  BL_TO_FORTRAN_ANYD(ulz[mfi]),
                                  BL_TO_FORTRAN_ANYD(urz[mfi]),
                                  BL_TO_FORTRAN_ANYD(uimhz[mfi]),
                                  BL_TO_FORTRAN_ANYD(wimhyx[mfi]),
                                  AMREX_REAL_ANYD(dx), dt, phys_bc.dataPtr());

            // call fortran subroutine
            // use macros in AMReX_ArrayLim.H to pass in each FAB's data,
            // lo/hi coordinates (including ghost cells), and/or the # of components
            // We will also pass "validBox", which specifies the "valid" region.

            // x-direction
#pragma gpu box(xbx)
            velpred_3d(AMREX_INT_ANYD(xbx.loVect()),
                       AMREX_INT_ANYD(xbx.hiVect()),
                       lev, 1, AMREX_INT_ANYD(domainBox.loVect()),
                       AMREX_INT_ANYD(domainBox.hiVect()),
                       BL_TO_FORTRAN_ANYD(utilde_mf[mfi]),
                       utilde_mf.nComp(), utilde_mf.nGrow(),
                       BL_TO_FORTRAN_ANYD(utrans_mf[mfi]),
                       BL_TO_FORTRAN_ANYD(vtrans_mf[mfi]),
                       BL_TO_FORTRAN_ANYD(wtrans_mf[mfi]),
                       BL_TO_FORTRAN_ANYD(umac_mf[mfi]),
                       BL_TO_FORTRAN_ANYD(vmac_mf[mfi]),
                       BL_TO_FORTRAN_ANYD(wmac_mf[mfi]),
                       BL_TO_FORTRAN_ANYD(w0macx_mf[mfi]),
                       BL_TO_FORTRAN_ANYD(w0macy_mf[mfi]),
                       BL_TO_FORTRAN_ANYD(w0macz_mf[mfi]),
                       BL_TO_FORTRAN_ANYD(Imfx[mfi]),
                       BL_TO_FORTRAN_ANYD(Ipfx[mfi]),
                       BL_TO_FORTRAN_ANYD(ulx[mfi]),
                       BL_TO_FORTRAN_ANYD(urx[mfi]),
                       BL_TO_FORTRAN_ANYD(uimhyz[mfi]),
                       BL_TO_FORTRAN_ANYD(uimhzy[mfi]),
                       BL_TO_FORTRAN_ANYD(vimhxz[mfi]),
                       BL_TO_FORTRAN_ANYD(vimhzx[mfi]),
                       BL_TO_FORTRAN_ANYD(wimhxy[mfi]),
                       BL_TO_FORTRAN_ANYD(wimhyx[mfi]),
                       BL_TO_FORTRAN_ANYD(force_mf[mfi]), force_mf.nComp(), force_mf.nGrow(),
                       w0.dataPtr(), AMREX_REAL_ANYD(dx), dt, phys_bc.dataPtr());

            // y-direction
#pragma gpu box(ybx)
            velpred_3d(AMREX_INT_ANYD(ybx.loVect()),
                       AMREX_INT_ANYD(ybx.hiVect()),
                       lev, 2, AMREX_INT_ANYD(domainBox.loVect()),
                       AMREX_INT_ANYD(domainBox.hiVect()),
                       BL_TO_FORTRAN_ANYD(utilde_mf[mfi]),
                       utilde_mf.nComp(), utilde_mf.nGrow(),
                       BL_TO_FORTRAN_ANYD(utrans_mf[mfi]),
                       BL_TO_FORTRAN_ANYD(vtrans_mf[mfi]),
                       BL_TO_FORTRAN_ANYD(wtrans_mf[mfi]),
                       BL_TO_FORTRAN_ANYD(umac_mf[mfi]),
                       BL_TO_FORTRAN_ANYD(vmac_mf[mfi]),
                       BL_TO_FORTRAN_ANYD(wmac_mf[mfi]),
                       BL_TO_FORTRAN_ANYD(w0macx_mf[mfi]),
                       BL_TO_FORTRAN_ANYD(w0macy_mf[mfi]),
                       BL_TO_FORTRAN_ANYD(w0macz_mf[mfi]),
                       BL_TO_FORTRAN_ANYD(Imfy[mfi]),
                       BL_TO_FORTRAN_ANYD(Ipfy[mfi]),
                       BL_TO_FORTRAN_ANYD(uly[mfi]),
                       BL_TO_FORTRAN_ANYD(ury[mfi]),
                       BL_TO_FORTRAN_ANYD(uimhyz[mfi]),
                       BL_TO_FORTRAN_ANYD(uimhzy[mfi]),
                       BL_TO_FORTRAN_ANYD(vimhxz[mfi]),
                       BL_TO_FORTRAN_ANYD(vimhzx[mfi]),
                       BL_TO_FORTRAN_ANYD(wimhxy[mfi]),
                       BL_TO_FORTRAN_ANYD(wimhyx[mfi]),
                       BL_TO_FORTRAN_ANYD(force_mf[mfi]), force_mf.nComp(), force_mf.nGrow(),
                       w0.dataPtr(), AMREX_REAL_ANYD(dx), dt, phys_bc.dataPtr());

            // z-direction
#pragma gpu box(zbx)
            velpred_3d(AMREX_INT_ANYD(zbx.loVect()),
                       AMREX_INT_ANYD(zbx.hiVect()),
                       lev, 3, AMREX_INT_ANYD(domainBox.loVect()),
                       AMREX_INT_ANYD(domainBox.hiVect()),
                       BL_TO_FORTRAN_ANYD(utilde_mf[mfi]),
                       utilde_mf.nComp(), utilde_mf.nGrow(),
                       BL_TO_FORTRAN_ANYD(utrans_mf[mfi]),
                       BL_TO_FORTRAN_ANYD(vtrans_mf[mfi]),
                       BL_TO_FORTRAN_ANYD(wtrans_mf[mfi]),
                       BL_TO_FORTRAN_ANYD(umac_mf[mfi]),
                       BL_TO_FORTRAN_ANYD(vmac_mf[mfi]),
                       BL_TO_FORTRAN_ANYD(wmac_mf[mfi]),
                       BL_TO_FORTRAN_ANYD(w0macx_mf[mfi]),
                       BL_TO_FORTRAN_ANYD(w0macy_mf[mfi]),
                       BL_TO_FORTRAN_ANYD(w0macz_mf[mfi]),
                       BL_TO_FORTRAN_ANYD(Imfz[mfi]),
                       BL_TO_FORTRAN_ANYD(Ipfz[mfi]),
                       BL_TO_FORTRAN_ANYD(ulz[mfi]),
                       BL_TO_FORTRAN_ANYD(urz[mfi]),
                       BL_TO_FORTRAN_ANYD(uimhyz[mfi]),
                       BL_TO_FORTRAN_ANYD(uimhzy[mfi]),
                       BL_TO_FORTRAN_ANYD(vimhxz[mfi]),
                       BL_TO_FORTRAN_ANYD(vimhzx[mfi]),
                       BL_TO_FORTRAN_ANYD(wimhxy[mfi]),
                       BL_TO_FORTRAN_ANYD(wimhyx[mfi]),
                       BL_TO_FORTRAN_ANYD(force_mf[mfi]), force_mf.nComp(), force_mf.nGrow(),
                       w0.dataPtr(), AMREX_REAL_ANYD(dx), dt, phys_bc.dataPtr());
        } // end MFIter loop

#ifdef AMREX_USE_CUDA
        clean_bc(bc_f);
#endif

#endif // AMREX_SPACEDIM
    } // end loop over levels

    // edge_restriction
    AverageDownFaces(umac);

#ifdef AMREX_USE_CUDA
    // turn off GPU
    if (not_launched) Gpu::setLaunchRegion(false);
#endif

}

void
Maestro::MakeEdgeScal (const Vector<MultiFab>& state,
                       Vector<std::array< MultiFab, AMREX_SPACEDIM > >& sedge,
                       const Vector<std::array< MultiFab, AMREX_SPACEDIM > >& umac,
                       const Vector<MultiFab>& force,
                       int is_vel, const Vector<BCRec>& bcs, int nbccomp,
                       int start_scomp, int start_bccomp, int num_comp, int is_conservative)
{
    // timer for profiling
    BL_PROFILE_VAR("Maestro::MakeEdgeScal()", MakeEdgeScal);

#ifdef AMREX_USE_CUDA
    auto not_launched = Gpu::notInLaunchRegion();
    // turn on GPU
    if (not_launched) Gpu::setLaunchRegion(true);
#endif

    for (int lev=0; lev<=finest_level; ++lev) {

        // Get the index space and grid spacing of the domain
        const Box& domainBox = geom[lev].Domain();
        const Real* dx = geom[lev].CellSize();

        // get references to the MultiFabs at level lev
        const MultiFab& scal_mf   = state[lev];
        MultiFab& sedgex_mf = sedge[lev][0];
        const MultiFab& umac_mf   = umac[lev][0];
#if (AMREX_SPACEDIM >= 2)
        MultiFab& sedgey_mf = sedge[lev][1];
        const MultiFab& vmac_mf   = umac[lev][1];

        MultiFab Ip, Im, Ipf, Imf;
        Ip.define(grids[lev],dmap[lev],AMREX_SPACEDIM,1);
        Im.define(grids[lev],dmap[lev],AMREX_SPACEDIM,1);
        Ipf.define(grids[lev],dmap[lev],AMREX_SPACEDIM,1);
        Imf.define(grids[lev],dmap[lev],AMREX_SPACEDIM,1);

        MultiFab slx, srx, simhx;
        slx.define(grids[lev],dmap[lev],1,1);
        srx.define(grids[lev],dmap[lev],1,1);
        simhx.define(grids[lev],dmap[lev],1,1);

        MultiFab sly, sry, simhy;
        sly.define(grids[lev],dmap[lev],1,1);
        sry.define(grids[lev],dmap[lev],1,1);
        simhy.define(grids[lev],dmap[lev],1,1);

        slx.setVal(0.);
        srx.setVal(0.);
        simhx.setVal(0.);
        sly.setVal(0.);
        sry.setVal(0.);
        simhy.setVal(0.);

#if (AMREX_SPACEDIM == 3)
        MultiFab& sedgez_mf = sedge[lev][2];
        const MultiFab& wmac_mf   = umac[lev][2];

        MultiFab slopez, divu;
        slopez.define(grids[lev],dmap[lev],1,1);
        divu.define(grids[lev],dmap[lev],1,1);

        MultiFab slz, srz, simhz;
        slz.define(grids[lev],dmap[lev],1,1);
        srz.define(grids[lev],dmap[lev],1,1);
        simhz.define(grids[lev],dmap[lev],1,1);

        MultiFab simhxy, simhxz, simhyx, simhyz, simhzx, simhzy;
        simhxy.define(grids[lev],dmap[lev],1,1);
        simhxz.define(grids[lev],dmap[lev],1,1);
        simhyx.define(grids[lev],dmap[lev],1,1);
        simhyz.define(grids[lev],dmap[lev],1,1);
        simhzx.define(grids[lev],dmap[lev],1,1);
        simhzy.define(grids[lev],dmap[lev],1,1);

#endif
#endif
        const MultiFab& force_mf = force[lev];

        // loop over boxes (make sure mfi takes a cell-centered multifab as an argument)
#if (AMREX_SPACEDIM == 1)
        // NOTE: don't tile, but threaded in fortran subroutine
        for ( MFIter mfi(scal_mf); mfi.isValid(); ++mfi ) {

            // Get the index space of the valid region
            const Box& tileBox = mfi.tilebox();

            // Be careful to pass in comp+1 for fortran indexing
            for (int scomp = start_scomp+1; scomp <= start_scomp + num_comp; ++scomp) {

                int bccomp = start_bccomp + scomp - start_scomp;

                // call fortran subroutine
                // use macros in AMReX_ArrayLim.H to pass in each FAB's data,
                // lo/hi coordinates (including ghost cells), and/or the # of components
                // We will also pass "validBox", which specifies the "valid" region.
                make_edge_scal_1d(
                    AMREX_ARLIM_3D(domainBox.loVect()), AMREX_ARLIM_3D(domainBox.hiVect()),
                    AMREX_ARLIM_3D(tileBox.loVect()), AMREX_ARLIM_3D(tileBox.hiVect()),
                    BL_TO_FORTRAN_3D(scal_mf[mfi]), scal_mf.nComp(), scal_mf.nGrow(),
                    BL_TO_FORTRAN_3D(sedgex_mf[mfi]), sedgex_mf.nComp(),
                    BL_TO_FORTRAN_3D(umac_mf[mfi]),
                    umac_mf.nGrow(),
                    BL_TO_FORTRAN_3D(force_mf[mfi]), force_mf.nComp(),
                    dx, dt, is_vel, bcs[0].data(),
                    nbccomp, scomp, bccomp, is_conservative);
            } // end loop over components
        } // end MFIter loop

#elif (AMREX_SPACEDIM == 2)

#ifdef AMREX_USE_CUDA
        int* bc_f = prepare_bc(bcs_u[0].data(), 1);
#else
        const int* bc_f = bcs[0].data();
#endif
        Vector<MultiFab> vec_scal_mf(num_comp);
        for (int comp=0; comp < num_comp; ++comp) {
            vec_scal_mf[comp].define(grids[lev],dmap[lev],1,scal_mf.nGrow());

            MultiFab::Copy(vec_scal_mf[comp], scal_mf, start_scomp+comp, 0, 1, scal_mf.nGrow());
        }

        for ( MFIter mfi(scal_mf, true); mfi.isValid(); ++mfi ) {

            // Get the index space of the valid region
            const Box& tileBox = mfi.tilebox();
            const Box& obx = amrex::grow(tileBox, 1);
            const Box& xbx = amrex::growHi(tileBox,0, 1);
            const Box& ybx = amrex::growHi(tileBox,1, 1);
            const Box& mxbx = amrex::growLo(obx,0, -1);
            const Box& mybx = amrex::growLo(obx,1, -1);

            // Be careful to pass in comp+1 for fortran indexing
            for (int scomp = start_scomp+1; scomp <= start_scomp + num_comp; ++scomp) {

                int vcomp = scomp - start_scomp - 1;

                int bccomp = start_bccomp + scomp - start_scomp;

                // x-direction
                if (ppm_type == 0) {
                    // we're going to reuse Ip here as slopex and Im as slopey
                    // as they have the correct number of ghost zones

                    // x-direction
#pragma gpu box(obx)
                    slopex_2d(AMREX_INT_ANYD(obx.loVect()),
                              AMREX_INT_ANYD(obx.hiVect()),
                              BL_TO_FORTRAN_ANYD(vec_scal_mf[vcomp][mfi]),
                              vec_scal_mf[vcomp].nComp(),
                              BL_TO_FORTRAN_ANYD(Ip[mfi]),Ip.nComp(),
                              AMREX_INT_ANYD(domainBox.loVect()),
                              AMREX_INT_ANYD(domainBox.hiVect()),
                              1,bc_f,nbccomp,bccomp);

                    // y-direction
#pragma gpu box(obx)
                    slopey_2d(AMREX_INT_ANYD(obx.loVect()),
                              AMREX_INT_ANYD(obx.hiVect()),
                              BL_TO_FORTRAN_ANYD(vec_scal_mf[vcomp][mfi]),
                              vec_scal_mf[vcomp].nComp(),
                              BL_TO_FORTRAN_ANYD(Im[mfi]),Im.nComp(),
                              AMREX_INT_ANYD(domainBox.loVect()),
                              AMREX_INT_ANYD(domainBox.hiVect()),
                              1,bc_f,nbccomp,bccomp);


                } else {
#pragma gpu box(obx)
                    ppm_2d(AMREX_INT_ANYD(obx.loVect()),
                           AMREX_INT_ANYD(obx.hiVect()),
                           BL_TO_FORTRAN_ANYD(scal_mf[mfi]),
                           scal_mf.nComp(),
                           BL_TO_FORTRAN_ANYD(umac_mf[mfi]),
                           BL_TO_FORTRAN_ANYD(vmac_mf[mfi]),
                           BL_TO_FORTRAN_ANYD(Ip[mfi]),
                           BL_TO_FORTRAN_ANYD(Im[mfi]),
                           AMREX_INT_ANYD(domainBox.loVect()),
                           AMREX_INT_ANYD(domainBox.hiVect()),
                           bc_f, AMREX_REAL_ANYD(dx), dt, true,
                           scomp, bccomp);

                    if (ppm_trace_forces == 1) {
#pragma gpu box(obx)
                        ppm_2d(AMREX_INT_ANYD(obx.loVect()),
                               AMREX_INT_ANYD(obx.hiVect()),
                               BL_TO_FORTRAN_ANYD(force_mf[mfi]),
                               force_mf.nComp(),
                               BL_TO_FORTRAN_ANYD(umac_mf[mfi]),
                               BL_TO_FORTRAN_ANYD(vmac_mf[mfi]),
                               BL_TO_FORTRAN_ANYD(Ipf[mfi]),
                               BL_TO_FORTRAN_ANYD(Imf[mfi]),
                               AMREX_INT_ANYD(domainBox.loVect()),
                               AMREX_INT_ANYD(domainBox.hiVect()),
                               bc_f, AMREX_REAL_ANYD(dx), dt, true,
                               scomp, bccomp);

                    }
                }

                // call fortran subroutine
                // use macros in AMReX_ArrayLim.H to pass in each FAB's data,
                // lo/hi coordinates (including ghost cells), and/or the # of components
                // We will also pass "validBox", which specifies the "valid" region.

                // x-direction
#pragma gpu box(mxbx)
                make_edge_scal_predictor_2d(
                    AMREX_INT_ANYD(mxbx.loVect()), AMREX_INT_ANYD(mxbx.hiVect()), 1,
                    AMREX_INT_ANYD(domainBox.loVect()), AMREX_INT_ANYD(domainBox.hiVect()),
                    BL_TO_FORTRAN_ANYD(scal_mf[mfi]), scal_mf.nComp(), scal_mf.nGrow(),
                    BL_TO_FORTRAN_ANYD(umac_mf[mfi]),
                    BL_TO_FORTRAN_ANYD(vmac_mf[mfi]),
                    BL_TO_FORTRAN_ANYD(Ip[mfi]),
                    BL_TO_FORTRAN_ANYD(Im[mfi]),
                    BL_TO_FORTRAN_ANYD(slx[mfi]),
                    BL_TO_FORTRAN_ANYD(srx[mfi]),
                    BL_TO_FORTRAN_ANYD(simhx[mfi]),
                    AMREX_REAL_ANYD(dx), dt, is_vel, bc_f,
                    nbccomp, scomp, bccomp);

                // y-direction
#pragma gpu box(mybx)
                make_edge_scal_predictor_2d(
                    AMREX_INT_ANYD(mybx.loVect()), AMREX_INT_ANYD(mybx.hiVect()), 2,
                    AMREX_INT_ANYD(domainBox.loVect()), AMREX_INT_ANYD(domainBox.hiVect()),
                    BL_TO_FORTRAN_ANYD(scal_mf[mfi]), scal_mf.nComp(), scal_mf.nGrow(),
                    BL_TO_FORTRAN_ANYD(umac_mf[mfi]),
                    BL_TO_FORTRAN_ANYD(vmac_mf[mfi]),
                    BL_TO_FORTRAN_ANYD(Ip[mfi]),
                    BL_TO_FORTRAN_ANYD(Im[mfi]),
                    BL_TO_FORTRAN_ANYD(sly[mfi]),
                    BL_TO_FORTRAN_ANYD(sry[mfi]),
                    BL_TO_FORTRAN_ANYD(simhy[mfi]),
                    AMREX_REAL_ANYD(dx), dt, is_vel, bc_f,
                    nbccomp, scomp, bccomp);

                // x-direction
#pragma gpu box(xbx)
                make_edge_scal_2d(
                    AMREX_INT_ANYD(xbx.loVect()), AMREX_INT_ANYD(xbx.hiVect()),1,
                    AMREX_INT_ANYD(domainBox.loVect()), AMREX_INT_ANYD(domainBox.hiVect()),
                    BL_TO_FORTRAN_ANYD(scal_mf[mfi]), scal_mf.nComp(), scal_mf.nGrow(),
                    BL_TO_FORTRAN_ANYD(sedgex_mf[mfi]), sedgex_mf.nComp(),
                    BL_TO_FORTRAN_ANYD(umac_mf[mfi]),
                    BL_TO_FORTRAN_ANYD(vmac_mf[mfi]),
                    BL_TO_FORTRAN_ANYD(Ipf[mfi]),
                    BL_TO_FORTRAN_ANYD(Imf[mfi]),
                    BL_TO_FORTRAN_ANYD(slx[mfi]),
                    BL_TO_FORTRAN_ANYD(srx[mfi]),
                    BL_TO_FORTRAN_ANYD(simhy[mfi]),
                    BL_TO_FORTRAN_ANYD(force_mf[mfi]), force_mf.nComp(),
                    AMREX_REAL_ANYD(dx), dt, is_vel, bc_f,
                    nbccomp, scomp, bccomp, is_conservative);

                // y-direction
#pragma gpu box(ybx)
                make_edge_scal_2d(
                    AMREX_INT_ANYD(ybx.loVect()), AMREX_INT_ANYD(ybx.hiVect()),2,
                    AMREX_INT_ANYD(domainBox.loVect()), AMREX_INT_ANYD(domainBox.hiVect()),
                    BL_TO_FORTRAN_ANYD(scal_mf[mfi]), scal_mf.nComp(), scal_mf.nGrow(),
                    BL_TO_FORTRAN_ANYD(sedgey_mf[mfi]), sedgey_mf.nComp(),
                    BL_TO_FORTRAN_ANYD(umac_mf[mfi]),
                    BL_TO_FORTRAN_ANYD(vmac_mf[mfi]),
                    BL_TO_FORTRAN_ANYD(Ipf[mfi]),
                    BL_TO_FORTRAN_ANYD(Imf[mfi]),
                    BL_TO_FORTRAN_ANYD(sly[mfi]),
                    BL_TO_FORTRAN_ANYD(sry[mfi]),
                    BL_TO_FORTRAN_ANYD(simhx[mfi]),
                    BL_TO_FORTRAN_ANYD(force_mf[mfi]), force_mf.nComp(),
                    AMREX_REAL_ANYD(dx), dt, is_vel, bc_f,
                    nbccomp, scomp, bccomp, is_conservative);
            } // end loop over components
        } // end MFIter loop

#ifdef AMREX_USE_CUDA
        clean_bc(bc_f);
#endif

#elif (AMREX_SPACEDIM == 3)

#ifdef AMREX_USE_CUDA
        int* bc_f = prepare_bc(bcs_u[0].data(), 1);
#else
        const int* bc_f = bcs[0].data();
#endif
        Vector<MultiFab> vec_scal_mf(num_comp);
        for (int comp=0; comp < num_comp; ++comp) {
            vec_scal_mf[comp].define(grids[lev],dmap[lev],1,scal_mf.nGrow());

            MultiFab::Copy(vec_scal_mf[comp], scal_mf, start_scomp+comp, 0, 1, scal_mf.nGrow());
        }

        for ( MFIter mfi(scal_mf, true); mfi.isValid(); ++mfi ) {

            // Get the index space of the valid region
            const Box& tileBox = mfi.tilebox();
            const Box& obx = amrex::grow(tileBox, 1);
            const Box& xbx = amrex::growHi(tileBox,0, 1);
            const Box& ybx = amrex::growHi(tileBox,1, 1);
            const Box& zbx = amrex::growHi(tileBox,2, 1);
            const Box& mxbx = amrex::growLo(obx,0, -1);
            const Box& mybx = amrex::growLo(obx,1, -1);
            const Box& mzbx = amrex::growLo(obx,2, -1);

            // Be careful to pass in comp+1 for fortran indexing
            for (int scomp = start_scomp+1; scomp <= start_scomp + num_comp; ++scomp) {

                int vcomp = scomp - start_scomp - 1;

                int bccomp = start_bccomp + scomp - start_scomp;

                // x-direction
                if (ppm_type == 0) {
                    // we're going to reuse Ip here as slopex and Im as slopey
                    // as they have the correct number of ghost zones

                    // x-direction
#pragma gpu box(obx)
                    slopex_2d(AMREX_INT_ANYD(obx.loVect()),
                              AMREX_INT_ANYD(obx.hiVect()),
                              BL_TO_FORTRAN_ANYD(vec_scal_mf[vcomp][mfi]),
                              vec_scal_mf[vcomp].nComp(),
                              BL_TO_FORTRAN_ANYD(Ip[mfi]),Ip.nComp(),
                              AMREX_INT_ANYD(domainBox.loVect()),
                              AMREX_INT_ANYD(domainBox.hiVect()),
                              1,bc_f,nbccomp,bccomp);

                    // y-direction
#pragma gpu box(obx)
                    slopey_2d(AMREX_INT_ANYD(obx.loVect()),
                              AMREX_INT_ANYD(obx.hiVect()),
                              BL_TO_FORTRAN_ANYD(vec_scal_mf[vcomp][mfi]),
                              vec_scal_mf[vcomp].nComp(),
                              BL_TO_FORTRAN_ANYD(Im[mfi]),Im.nComp(),
                              AMREX_INT_ANYD(domainBox.loVect()),
                              AMREX_INT_ANYD(domainBox.hiVect()),
                              1,bc_f,nbccomp,bccomp);

                    // z-direction
#pragma gpu box(obx)
                    slopez_3d(AMREX_INT_ANYD(obx.loVect()),
                              AMREX_INT_ANYD(obx.hiVect()),
                              BL_TO_FORTRAN_ANYD(vec_scal_mf[vcomp][mfi]),
                              vec_scal_mf[vcomp].nComp(),
                              BL_TO_FORTRAN_ANYD(slopez[mfi]),slopez.nComp(),
                              AMREX_INT_ANYD(domainBox.loVect()),
                              AMREX_INT_ANYD(domainBox.hiVect()),
                              1,bc_f,nbccomp,bccomp);


                } else {
#pragma gpu box(obx)
                    ppm_3d(AMREX_INT_ANYD(obx.loVect()),
                           AMREX_INT_ANYD(obx.hiVect()),
                           BL_TO_FORTRAN_ANYD(scal_mf[mfi]),
                           scal_mf.nComp(),
                           BL_TO_FORTRAN_ANYD(umac_mf[mfi]),
                           BL_TO_FORTRAN_ANYD(vmac_mf[mfi]),
                           BL_TO_FORTRAN_ANYD(wmac_mf[mfi]),
                           BL_TO_FORTRAN_ANYD(Ip[mfi]),
                           BL_TO_FORTRAN_ANYD(Im[mfi]),
                           AMREX_INT_ANYD(domainBox.loVect()),
                           AMREX_INT_ANYD(domainBox.hiVect()),
                           bc_f, AMREX_REAL_ANYD(dx), dt, true,
                           scomp, bccomp);

                    if (ppm_trace_forces == 1) {
#pragma gpu box(obx)
                        ppm_3d(AMREX_INT_ANYD(obx.loVect()),
                               AMREX_INT_ANYD(obx.hiVect()),
                               BL_TO_FORTRAN_ANYD(force_mf[mfi]),
                               force_mf.nComp(),
                               BL_TO_FORTRAN_ANYD(umac_mf[mfi]),
                               BL_TO_FORTRAN_ANYD(vmac_mf[mfi]),
                               BL_TO_FORTRAN_ANYD(wmac_mf[mfi]),
                               BL_TO_FORTRAN_ANYD(Ipf[mfi]),
                               BL_TO_FORTRAN_ANYD(Imf[mfi]),
                               AMREX_INT_ANYD(domainBox.loVect()),
                               AMREX_INT_ANYD(domainBox.hiVect()),
                               bc_f, AMREX_REAL_ANYD(dx), dt, true,
                               scomp, bccomp);

                    }
                }

#pragma gpu box(obx)
                make_divu(AMREX_INT_ANYD(obx.loVect()),
                          AMREX_INT_ANYD(obx.hiVect()),
                          BL_TO_FORTRAN_ANYD(divu[mfi]),
                          BL_TO_FORTRAN_ANYD(umac_mf[mfi]),
                          BL_TO_FORTRAN_ANYD(vmac_mf[mfi]),
                          BL_TO_FORTRAN_ANYD(wmac_mf[mfi]),
                          AMREX_REAL_ANYD(dx), is_conservative);

                // x-direction
#pragma gpu box(mxbx)
                make_edge_scal_predictor_3d(
                    AMREX_INT_ANYD(mxbx.loVect()), AMREX_INT_ANYD(mxbx.hiVect()), 1,
                    AMREX_INT_ANYD(domainBox.loVect()), AMREX_INT_ANYD(domainBox.hiVect()),
                    BL_TO_FORTRAN_ANYD(scal_mf[mfi]), scal_mf.nComp(),
                    BL_TO_FORTRAN_ANYD(umac_mf[mfi]),
                    BL_TO_FORTRAN_ANYD(vmac_mf[mfi]),
                    BL_TO_FORTRAN_ANYD(wmac_mf[mfi]),
                    BL_TO_FORTRAN_ANYD(Ip[mfi]),
                    BL_TO_FORTRAN_ANYD(Im[mfi]),
                    BL_TO_FORTRAN_ANYD(slopez[mfi]),
                    BL_TO_FORTRAN_ANYD(slx[mfi]),
                    BL_TO_FORTRAN_ANYD(srx[mfi]),
                    BL_TO_FORTRAN_ANYD(simhx[mfi]),
                    AMREX_REAL_ANYD(dx), dt, is_vel, bc_f,
                    nbccomp, scomp, bccomp);

                // y-direction
#pragma gpu box(mybx)
                make_edge_scal_predictor_3d(
                    AMREX_INT_ANYD(mybx.loVect()), AMREX_INT_ANYD(mybx.hiVect()), 2,
                    AMREX_INT_ANYD(domainBox.loVect()), AMREX_INT_ANYD(domainBox.hiVect()),
                    BL_TO_FORTRAN_ANYD(scal_mf[mfi]), scal_mf.nComp(),
                    BL_TO_FORTRAN_ANYD(umac_mf[mfi]),
                    BL_TO_FORTRAN_ANYD(vmac_mf[mfi]),
                    BL_TO_FORTRAN_ANYD(wmac_mf[mfi]),
                    BL_TO_FORTRAN_ANYD(Ip[mfi]),
                    BL_TO_FORTRAN_ANYD(Im[mfi]),
                    BL_TO_FORTRAN_ANYD(slopez[mfi]),
                    BL_TO_FORTRAN_ANYD(sly[mfi]),
                    BL_TO_FORTRAN_ANYD(sry[mfi]),
                    BL_TO_FORTRAN_ANYD(simhy[mfi]),
                    AMREX_REAL_ANYD(dx), dt, is_vel, bc_f,
                    nbccomp, scomp, bccomp);

                // z-direction
#pragma gpu box(mzbx)
                make_edge_scal_predictor_3d(
                    AMREX_INT_ANYD(mzbx.loVect()), AMREX_INT_ANYD(mzbx.hiVect()), 3,
                    AMREX_INT_ANYD(domainBox.loVect()), AMREX_INT_ANYD(domainBox.hiVect()),
                    BL_TO_FORTRAN_ANYD(scal_mf[mfi]), scal_mf.nComp(),
                    BL_TO_FORTRAN_ANYD(umac_mf[mfi]),
                    BL_TO_FORTRAN_ANYD(vmac_mf[mfi]),
                    BL_TO_FORTRAN_ANYD(wmac_mf[mfi]),
                    BL_TO_FORTRAN_ANYD(Ip[mfi]),
                    BL_TO_FORTRAN_ANYD(Im[mfi]),
                    BL_TO_FORTRAN_ANYD(slopez[mfi]),
                    BL_TO_FORTRAN_ANYD(slz[mfi]),
                    BL_TO_FORTRAN_ANYD(srz[mfi]),
                    BL_TO_FORTRAN_ANYD(simhz[mfi]),
                    AMREX_REAL_ANYD(dx), dt, is_vel, bc_f,
                    nbccomp, scomp, bccomp);

                // simhxy
                Box imhbox = amrex::grow(mfi.tilebox(), 2, 1);
                imhbox = amrex::growHi(imhbox, 0, 1);
#pragma gpu box(imhbox)
                make_edge_scal_transverse_3d(
                    AMREX_INT_ANYD(imhbox.loVect()), AMREX_INT_ANYD(imhbox.hiVect()),1,2,
                    AMREX_INT_ANYD(domainBox.loVect()), AMREX_INT_ANYD(domainBox.hiVect()),
                    BL_TO_FORTRAN_ANYD(scal_mf[mfi]), scal_mf.nComp(),
                    BL_TO_FORTRAN_ANYD(umac_mf[mfi]),
                    BL_TO_FORTRAN_ANYD(vmac_mf[mfi]),
                    BL_TO_FORTRAN_ANYD(wmac_mf[mfi]),
                    BL_TO_FORTRAN_ANYD(divu[mfi]),
                    BL_TO_FORTRAN_ANYD(slx[mfi]),
                    BL_TO_FORTRAN_ANYD(srx[mfi]),
                    BL_TO_FORTRAN_ANYD(simhx[mfi]),
                    BL_TO_FORTRAN_ANYD(sly[mfi]),
                    BL_TO_FORTRAN_ANYD(sry[mfi]),
                    BL_TO_FORTRAN_ANYD(simhy[mfi]),
                    BL_TO_FORTRAN_ANYD(slz[mfi]),
                    BL_TO_FORTRAN_ANYD(srz[mfi]),
                    BL_TO_FORTRAN_ANYD(simhz[mfi]),
                    BL_TO_FORTRAN_ANYD(simhxy[mfi]),
                    AMREX_REAL_ANYD(dx), dt, is_vel, bc_f,
                    nbccomp, scomp, bccomp, is_conservative);

                // simhxz
                imhbox = amrex::grow(mfi.tilebox(), 1, 1);
                imhbox = amrex::growHi(imhbox, 0, 1);
#pragma gpu box(imhbox)
                make_edge_scal_transverse_3d(
                    AMREX_INT_ANYD(imhbox.loVect()), AMREX_INT_ANYD(imhbox.hiVect()),1,3,
                    AMREX_INT_ANYD(domainBox.loVect()), AMREX_INT_ANYD(domainBox.hiVect()),
                    BL_TO_FORTRAN_ANYD(scal_mf[mfi]), scal_mf.nComp(),
                    BL_TO_FORTRAN_ANYD(umac_mf[mfi]),
                    BL_TO_FORTRAN_ANYD(vmac_mf[mfi]),
                    BL_TO_FORTRAN_ANYD(wmac_mf[mfi]),
                    BL_TO_FORTRAN_ANYD(divu[mfi]),
                    BL_TO_FORTRAN_ANYD(slx[mfi]),
                    BL_TO_FORTRAN_ANYD(srx[mfi]),
                    BL_TO_FORTRAN_ANYD(simhx[mfi]),
                    BL_TO_FORTRAN_ANYD(sly[mfi]),
                    BL_TO_FORTRAN_ANYD(sry[mfi]),
                    BL_TO_FORTRAN_ANYD(simhy[mfi]),
                    BL_TO_FORTRAN_ANYD(slz[mfi]),
                    BL_TO_FORTRAN_ANYD(srz[mfi]),
                    BL_TO_FORTRAN_ANYD(simhz[mfi]),
                    BL_TO_FORTRAN_ANYD(simhxz[mfi]),
                    AMREX_REAL_ANYD(dx), dt, is_vel, bc_f,
                    nbccomp, scomp, bccomp, is_conservative);

                // simhyx
                imhbox = amrex::grow(mfi.tilebox(), 2, 1);
                imhbox = amrex::growHi(imhbox, 1, 1);
#pragma gpu box(imhbox)
                make_edge_scal_transverse_3d(
                    AMREX_INT_ANYD(imhbox.loVect()), AMREX_INT_ANYD(imhbox.hiVect()),2,1,
                    AMREX_INT_ANYD(domainBox.loVect()), AMREX_INT_ANYD(domainBox.hiVect()),
                    BL_TO_FORTRAN_ANYD(scal_mf[mfi]), scal_mf.nComp(),
                    BL_TO_FORTRAN_ANYD(umac_mf[mfi]),
                    BL_TO_FORTRAN_ANYD(vmac_mf[mfi]),
                    BL_TO_FORTRAN_ANYD(wmac_mf[mfi]),
                    BL_TO_FORTRAN_ANYD(divu[mfi]),
                    BL_TO_FORTRAN_ANYD(slx[mfi]),
                    BL_TO_FORTRAN_ANYD(srx[mfi]),
                    BL_TO_FORTRAN_ANYD(simhx[mfi]),
                    BL_TO_FORTRAN_ANYD(sly[mfi]),
                    BL_TO_FORTRAN_ANYD(sry[mfi]),
                    BL_TO_FORTRAN_ANYD(simhy[mfi]),
                    BL_TO_FORTRAN_ANYD(slz[mfi]),
                    BL_TO_FORTRAN_ANYD(srz[mfi]),
                    BL_TO_FORTRAN_ANYD(simhz[mfi]),
                    BL_TO_FORTRAN_ANYD(simhyx[mfi]),
                    AMREX_REAL_ANYD(dx), dt, is_vel, bc_f,
                    nbccomp, scomp, bccomp, is_conservative);

                // simhyz
                imhbox = amrex::grow(mfi.tilebox(), 0, 1);
                imhbox = amrex::growHi(imhbox, 1, 1);
#pragma gpu box(imhbox)
                make_edge_scal_transverse_3d(
                    AMREX_INT_ANYD(imhbox.loVect()), AMREX_INT_ANYD(imhbox.hiVect()),2,3,
                    AMREX_INT_ANYD(domainBox.loVect()), AMREX_INT_ANYD(domainBox.hiVect()),
                    BL_TO_FORTRAN_ANYD(scal_mf[mfi]), scal_mf.nComp(),
                    BL_TO_FORTRAN_ANYD(umac_mf[mfi]),
                    BL_TO_FORTRAN_ANYD(vmac_mf[mfi]),
                    BL_TO_FORTRAN_ANYD(wmac_mf[mfi]),
                    BL_TO_FORTRAN_ANYD(divu[mfi]),
                    BL_TO_FORTRAN_ANYD(slx[mfi]),
                    BL_TO_FORTRAN_ANYD(srx[mfi]),
                    BL_TO_FORTRAN_ANYD(simhx[mfi]),
                    BL_TO_FORTRAN_ANYD(sly[mfi]),
                    BL_TO_FORTRAN_ANYD(sry[mfi]),
                    BL_TO_FORTRAN_ANYD(simhy[mfi]),
                    BL_TO_FORTRAN_ANYD(slz[mfi]),
                    BL_TO_FORTRAN_ANYD(srz[mfi]),
                    BL_TO_FORTRAN_ANYD(simhz[mfi]),
                    BL_TO_FORTRAN_ANYD(simhyz[mfi]),
                    AMREX_REAL_ANYD(dx), dt, is_vel, bc_f,
                    nbccomp, scomp, bccomp, is_conservative);

                // simhzx
                imhbox = amrex::grow(mfi.tilebox(), 1, 1);
                imhbox = amrex::growHi(imhbox, 2, 1);
#pragma gpu box(imhbox)
                make_edge_scal_transverse_3d(
                    AMREX_INT_ANYD(imhbox.loVect()), AMREX_INT_ANYD(imhbox.hiVect()),3,1,
                    AMREX_INT_ANYD(domainBox.loVect()), AMREX_INT_ANYD(domainBox.hiVect()),
                    BL_TO_FORTRAN_ANYD(scal_mf[mfi]), scal_mf.nComp(),
                    BL_TO_FORTRAN_ANYD(umac_mf[mfi]),
                    BL_TO_FORTRAN_ANYD(vmac_mf[mfi]),
                    BL_TO_FORTRAN_ANYD(wmac_mf[mfi]),
                    BL_TO_FORTRAN_ANYD(divu[mfi]),
                    BL_TO_FORTRAN_ANYD(slx[mfi]),
                    BL_TO_FORTRAN_ANYD(srx[mfi]),
                    BL_TO_FORTRAN_ANYD(simhx[mfi]),
                    BL_TO_FORTRAN_ANYD(sly[mfi]),
                    BL_TO_FORTRAN_ANYD(sry[mfi]),
                    BL_TO_FORTRAN_ANYD(simhy[mfi]),
                    BL_TO_FORTRAN_ANYD(slz[mfi]),
                    BL_TO_FORTRAN_ANYD(srz[mfi]),
                    BL_TO_FORTRAN_ANYD(simhz[mfi]),
                    BL_TO_FORTRAN_ANYD(simhzx[mfi]),
                    AMREX_REAL_ANYD(dx), dt, is_vel, bc_f,
                    nbccomp, scomp, bccomp, is_conservative);

                // simhzy
                imhbox = amrex::grow(mfi.tilebox(), 0, 1);
                imhbox = amrex::growHi(imhbox, 2, 1);
#pragma gpu box(imhbox)
                make_edge_scal_transverse_3d(
                    AMREX_INT_ANYD(imhbox.loVect()), AMREX_INT_ANYD(imhbox.hiVect()),3,2,
                    AMREX_INT_ANYD(domainBox.loVect()), AMREX_INT_ANYD(domainBox.hiVect()),
                    BL_TO_FORTRAN_ANYD(scal_mf[mfi]), scal_mf.nComp(),
                    BL_TO_FORTRAN_ANYD(umac_mf[mfi]),
                    BL_TO_FORTRAN_ANYD(vmac_mf[mfi]),
                    BL_TO_FORTRAN_ANYD(wmac_mf[mfi]),
                    BL_TO_FORTRAN_ANYD(divu[mfi]),
                    BL_TO_FORTRAN_ANYD(slx[mfi]),
                    BL_TO_FORTRAN_ANYD(srx[mfi]),
                    BL_TO_FORTRAN_ANYD(simhx[mfi]),
                    BL_TO_FORTRAN_ANYD(sly[mfi]),
                    BL_TO_FORTRAN_ANYD(sry[mfi]),
                    BL_TO_FORTRAN_ANYD(simhy[mfi]),
                    BL_TO_FORTRAN_ANYD(slz[mfi]),
                    BL_TO_FORTRAN_ANYD(srz[mfi]),
                    BL_TO_FORTRAN_ANYD(simhz[mfi]),
                    BL_TO_FORTRAN_ANYD(simhzy[mfi]),
                    AMREX_REAL_ANYD(dx), dt, is_vel, bc_f,
                    nbccomp, scomp, bccomp, is_conservative);

                // x-direction
#pragma gpu box(xbx)
                make_edge_scal_3d(AMREX_INT_ANYD(xbx.loVect()),
                                  AMREX_INT_ANYD(xbx.hiVect()),1,
                                  AMREX_INT_ANYD(domainBox.loVect()),
                                  AMREX_INT_ANYD(domainBox.hiVect()),
                                  BL_TO_FORTRAN_ANYD(scal_mf[mfi]), scal_mf.nComp(),
                                  BL_TO_FORTRAN_ANYD(sedgex_mf[mfi]), sedgex_mf.nComp(),
                                  BL_TO_FORTRAN_ANYD(umac_mf[mfi]),
                                  BL_TO_FORTRAN_ANYD(vmac_mf[mfi]),
                                  BL_TO_FORTRAN_ANYD(wmac_mf[mfi]),
                                  BL_TO_FORTRAN_ANYD(Ipf[mfi]),
                                  BL_TO_FORTRAN_ANYD(Imf[mfi]),
                                  BL_TO_FORTRAN_ANYD(slx[mfi]),
                                  BL_TO_FORTRAN_ANYD(srx[mfi]),
                                  BL_TO_FORTRAN_ANYD(simhxy[mfi]),
                                  BL_TO_FORTRAN_ANYD(simhxz[mfi]),
                                  BL_TO_FORTRAN_ANYD(simhyx[mfi]),
                                  BL_TO_FORTRAN_ANYD(simhyz[mfi]),
                                  BL_TO_FORTRAN_ANYD(simhzx[mfi]),
                                  BL_TO_FORTRAN_ANYD(simhzy[mfi]),
                                  BL_TO_FORTRAN_ANYD(force_mf[mfi]), force_mf.nComp(),
                                  AMREX_REAL_ANYD(dx), dt, is_vel, bc_f,
                                  nbccomp, scomp, bccomp, is_conservative);

                // y-direction
#pragma gpu box(ybx)
                make_edge_scal_3d(AMREX_INT_ANYD(ybx.loVect()),
                                  AMREX_INT_ANYD(ybx.hiVect()),2,
                                  AMREX_INT_ANYD(domainBox.loVect()),
                                  AMREX_INT_ANYD(domainBox.hiVect()),
                                  BL_TO_FORTRAN_ANYD(scal_mf[mfi]), scal_mf.nComp(),
                                  BL_TO_FORTRAN_ANYD(sedgey_mf[mfi]), sedgey_mf.nComp(),
                                  BL_TO_FORTRAN_ANYD(umac_mf[mfi]),
                                  BL_TO_FORTRAN_ANYD(vmac_mf[mfi]),
                                  BL_TO_FORTRAN_ANYD(wmac_mf[mfi]),
                                  BL_TO_FORTRAN_ANYD(Ipf[mfi]),
                                  BL_TO_FORTRAN_ANYD(Imf[mfi]),
                                  BL_TO_FORTRAN_ANYD(sly[mfi]),
                                  BL_TO_FORTRAN_ANYD(sry[mfi]),
                                  BL_TO_FORTRAN_ANYD(simhxy[mfi]),
                                  BL_TO_FORTRAN_ANYD(simhxz[mfi]),
                                  BL_TO_FORTRAN_ANYD(simhyx[mfi]),
                                  BL_TO_FORTRAN_ANYD(simhyz[mfi]),
                                  BL_TO_FORTRAN_ANYD(simhzx[mfi]),
                                  BL_TO_FORTRAN_ANYD(simhzy[mfi]),
                                  BL_TO_FORTRAN_ANYD(force_mf[mfi]), force_mf.nComp(),
                                  AMREX_REAL_ANYD(dx), dt, is_vel, bc_f,
                                  nbccomp, scomp, bccomp, is_conservative);

                // z-direction
#pragma gpu box(zbx)
                make_edge_scal_3d(AMREX_INT_ANYD(zbx.loVect()),
                                  AMREX_INT_ANYD(zbx.hiVect()),3,
                                  AMREX_INT_ANYD(domainBox.loVect()),
                                  AMREX_INT_ANYD(domainBox.hiVect()),
                                  BL_TO_FORTRAN_ANYD(scal_mf[mfi]), scal_mf.nComp(),
                                  BL_TO_FORTRAN_ANYD(sedgez_mf[mfi]), sedgez_mf.nComp(),
                                  BL_TO_FORTRAN_ANYD(umac_mf[mfi]),
                                  BL_TO_FORTRAN_ANYD(vmac_mf[mfi]),
                                  BL_TO_FORTRAN_ANYD(wmac_mf[mfi]),
                                  BL_TO_FORTRAN_ANYD(Ipf[mfi]),
                                  BL_TO_FORTRAN_ANYD(Imf[mfi]),
                                  BL_TO_FORTRAN_ANYD(slz[mfi]),
                                  BL_TO_FORTRAN_ANYD(srz[mfi]),
                                  BL_TO_FORTRAN_ANYD(simhxy[mfi]),
                                  BL_TO_FORTRAN_ANYD(simhxz[mfi]),
                                  BL_TO_FORTRAN_ANYD(simhyx[mfi]),
                                  BL_TO_FORTRAN_ANYD(simhyz[mfi]),
                                  BL_TO_FORTRAN_ANYD(simhzx[mfi]),
                                  BL_TO_FORTRAN_ANYD(simhzy[mfi]),
                                  BL_TO_FORTRAN_ANYD(force_mf[mfi]), force_mf.nComp(),
                                  AMREX_REAL_ANYD(dx), dt, is_vel, bc_f,
                                  nbccomp, scomp, bccomp, is_conservative);

            } // end loop over components
        } // end MFIter loop

#ifdef AMREX_USE_CUDA
        clean_bc(bc_f);
#endif

#endif
    } // end loop over levels

    // We use edge_restriction for the output velocity if is_vel == 1
    // we do not use edge_restriction for scalars because instead we will use
    // reflux on the fluxes in make_flux.
    if (is_vel == 1) {
        if (reflux_type == 1 || reflux_type == 2) {
            AverageDownFaces(sedge);
        }
    }

#ifdef AMREX_USE_CUDA
    // turn off GPU
    if (not_launched) Gpu::setLaunchRegion(false);
#endif
}


void
Maestro::MakeRhoXFlux (const Vector<MultiFab>& state,
                       Vector<std::array< MultiFab, AMREX_SPACEDIM > >& sflux,
                       Vector<MultiFab>& etarhoflux,
                       Vector<std::array< MultiFab, AMREX_SPACEDIM > >& sedge,
                       const Vector<std::array< MultiFab, AMREX_SPACEDIM > >& umac,
                       const Vector<std::array< MultiFab, AMREX_SPACEDIM > >& w0mac,
                       const RealVector& r0_old,
                       const RealVector& r0_edge_old,
                       const Vector<std::array< MultiFab, AMREX_SPACEDIM > >& r0mac_old,
                       const RealVector& r0_new,
                       const RealVector& r0_edge_new,
                       const Vector<std::array< MultiFab, AMREX_SPACEDIM > >& r0mac_new,
                       const RealVector& r0_predicted_edge,
                       int start_comp, int num_comp)
{
    // timer for profiling
    BL_PROFILE_VAR("Maestro::MakeRhoXFlux()", MakeRhoXFlux);

#ifdef AMREX_USE_CUDA
    auto not_launched = Gpu::notInLaunchRegion();
    // turn on GPU
    if (not_launched) Gpu::setLaunchRegion(true);
#endif

    // Make sure to pass in comp+1 for fortran indexing
    const int startcomp = start_comp + 1;
    const int endcomp = startcomp + num_comp - 1;

    for (int lev=0; lev<=finest_level; ++lev) {

// get references to the MultiFabs at level lev
        const MultiFab& scal_mf  = state[lev];
        MultiFab& sedgex_mf      = sedge[lev][0];
        MultiFab& sfluxx_mf      = sflux[lev][0];
        MultiFab& etarhoflux_mf  = etarhoflux[lev];
        const MultiFab& umac_mf  = umac[lev][0];
#if (AMREX_SPACEDIM >= 2)
        MultiFab& sedgey_mf      = sedge[lev][1];
        MultiFab& sfluxy_mf      = sflux[lev][1];
        const MultiFab& vmac_mf  = umac[lev][1];

#if (AMREX_SPACEDIM == 3)
        MultiFab& sedgez_mf      = sedge[lev][2];
        MultiFab& sfluxz_mf      = sflux[lev][2];
        const MultiFab& wmac_mf  = umac[lev][2];

    	// if spherical == 1
    	const MultiFab& w0macx_mf = w0mac[lev][0];
    	const MultiFab& w0macy_mf = w0mac[lev][1];
    	const MultiFab& w0macz_mf = w0mac[lev][2];
    	MultiFab rho0mac_edgex, rho0mac_edgey, rho0mac_edgez;
    	rho0mac_edgex.define(convert(grids[lev],nodal_flag_x), dmap[lev], 1, 1);
    	rho0mac_edgey.define(convert(grids[lev],nodal_flag_y), dmap[lev], 1, 1);
    	rho0mac_edgez.define(convert(grids[lev],nodal_flag_z), dmap[lev], 1, 1);

    	if (spherical == 1) {
    	    MultiFab::LinComb(rho0mac_edgex,0.5,r0mac_old[lev][0],0,0.5,r0mac_new[lev][0],0,0,1,1);
    	    MultiFab::LinComb(rho0mac_edgey,0.5,r0mac_old[lev][1],0,0.5,r0mac_new[lev][1],0,0,1,1);
    	    MultiFab::LinComb(rho0mac_edgez,0.5,r0mac_old[lev][2],0,0.5,r0mac_new[lev][2],0,0,1,1);
    	}
#endif
#endif

        // loop over boxes (make sure mfi takes a cell-centered multifab as an argument)
#ifdef _OPENMP
#pragma omp parallel
#endif
        for ( MFIter mfi(scal_mf, true); mfi.isValid(); ++mfi ) {

            // Get the index space of the valid region
            const Box& tileBox = mfi.tilebox();
#if (AMREX_SPACEDIM >= 2)
            const Box& xbx = amrex::growHi(tileBox,0, 1);
            const Box& ybx = amrex::growHi(tileBox,1, 1);
#if (AMREX_SPACEDIM == 3)
            const Box& zbx = amrex::growHi(tileBox,2, 1);
#endif
#endif

#if (AMREX_SPACEDIM == 1)
        		make_rhoX_flux_1d(
				  &lev, tileBox.loVect(), tileBox.hiVect(),
				  BL_TO_FORTRAN_FAB(sfluxx_mf[mfi]),
				  BL_TO_FORTRAN_3D(etarhoflux_mf[mfi]),
				  BL_TO_FORTRAN_FAB(sedgex_mf[mfi]),
				  BL_TO_FORTRAN_3D(umac_mf[mfi]),
				  r0_old.dataPtr(), r0_edge_old.dataPtr(),
				  r0_new.dataPtr(), r0_edge_new.dataPtr(),
				  r0_predicted_edge.dataPtr(),
				  w0.dataPtr(),
				  &startcomp, &endcomp);
#elif (AMREX_SPACEDIM == 2)
                // x-direction
#pragma gpu box(xbx)
                make_rhoX_flux_2d(AMREX_INT_ANYD(xbx.loVect()),
                    AMREX_INT_ANYD(xbx.hiVect()), lev, 1,
                    BL_TO_FORTRAN_ANYD(sfluxx_mf[mfi]), sfluxx_mf.nComp(),
                    BL_TO_FORTRAN_ANYD(sfluxy_mf[mfi]), sfluxy_mf.nComp(),
                    BL_TO_FORTRAN_ANYD(etarhoflux_mf[mfi]),
                    BL_TO_FORTRAN_ANYD(sedgex_mf[mfi]), sedgex_mf.nComp(),
                    BL_TO_FORTRAN_ANYD(sedgey_mf[mfi]), sedgey_mf.nComp(),
                    BL_TO_FORTRAN_ANYD(umac_mf[mfi]),
                    BL_TO_FORTRAN_ANYD(vmac_mf[mfi]),
                    r0_old.dataPtr(), r0_edge_old.dataPtr(),
                    r0_new.dataPtr(), r0_edge_new.dataPtr(),
                    r0_predicted_edge.dataPtr(),
                    w0.dataPtr(),
                    startcomp, endcomp);

                // y-direction
#pragma gpu box(ybx)
                make_rhoX_flux_2d(AMREX_INT_ANYD(ybx.loVect()),
                    AMREX_INT_ANYD(ybx.hiVect()), lev, 2,
                    BL_TO_FORTRAN_ANYD(sfluxx_mf[mfi]), sfluxx_mf.nComp(),
                    BL_TO_FORTRAN_ANYD(sfluxy_mf[mfi]), sfluxy_mf.nComp(),
                    BL_TO_FORTRAN_ANYD(etarhoflux_mf[mfi]),
                    BL_TO_FORTRAN_ANYD(sedgex_mf[mfi]), sedgex_mf.nComp(),
                    BL_TO_FORTRAN_ANYD(sedgey_mf[mfi]), sedgey_mf.nComp(),
                    BL_TO_FORTRAN_ANYD(umac_mf[mfi]),
                    BL_TO_FORTRAN_ANYD(vmac_mf[mfi]),
                    r0_old.dataPtr(), r0_edge_old.dataPtr(),
                    r0_new.dataPtr(), r0_edge_new.dataPtr(),
                    r0_predicted_edge.dataPtr(),
                    w0.dataPtr(),
                    startcomp, endcomp);

#elif (AMREX_SPACEDIM == 3)

    	    // call fortran subroutine
    	    // use macros in AMReX_ArrayLim.H to pass in each FAB's data,
    	    // lo/hi coordinates (including ghost cells), and/or the # of components
    	    // We will also pass "validBox", which specifies the "valid" region.
    	    if (spherical == 0) {
                // x-direction
#pragma gpu box(xbx)
                make_rhoX_flux_3d(AMREX_INT_ANYD(xbx.loVect()),
                                AMREX_INT_ANYD(xbx.hiVect()),
                                lev, 1,
                                BL_TO_FORTRAN_ANYD(sfluxx_mf[mfi]), sfluxx_mf.nComp(),
                                BL_TO_FORTRAN_ANYD(etarhoflux_mf[mfi]),
                                BL_TO_FORTRAN_ANYD(sedgex_mf[mfi]), sedgex_mf.nComp(),
                                BL_TO_FORTRAN_ANYD(sedgey_mf[mfi]), sedgey_mf.nComp(),
                                BL_TO_FORTRAN_ANYD(sedgez_mf[mfi]), sedgez_mf.nComp(),
                                BL_TO_FORTRAN_ANYD(umac_mf[mfi]),
                                BL_TO_FORTRAN_ANYD(vmac_mf[mfi]),
                                BL_TO_FORTRAN_ANYD(wmac_mf[mfi]),
                                r0_old.dataPtr(), r0_edge_old.dataPtr(),
                                r0_new.dataPtr(), r0_edge_new.dataPtr(),
                                r0_predicted_edge.dataPtr(),
                                w0.dataPtr(),
                                startcomp, endcomp);
                // y-direction
#pragma gpu box(ybx)
                make_rhoX_flux_3d(AMREX_INT_ANYD(ybx.loVect()),
                                AMREX_INT_ANYD(ybx.hiVect()),
                                lev, 2,
                                BL_TO_FORTRAN_ANYD(sfluxy_mf[mfi]), sfluxy_mf.nComp(),
                                BL_TO_FORTRAN_ANYD(etarhoflux_mf[mfi]),
                                BL_TO_FORTRAN_ANYD(sedgex_mf[mfi]), sedgex_mf.nComp(),
                                BL_TO_FORTRAN_ANYD(sedgey_mf[mfi]), sedgey_mf.nComp(),
                                BL_TO_FORTRAN_ANYD(sedgez_mf[mfi]), sedgez_mf.nComp(),
                                BL_TO_FORTRAN_ANYD(umac_mf[mfi]),
                                BL_TO_FORTRAN_ANYD(vmac_mf[mfi]),
                                BL_TO_FORTRAN_ANYD(wmac_mf[mfi]),
                                r0_old.dataPtr(), r0_edge_old.dataPtr(),
                                r0_new.dataPtr(), r0_edge_new.dataPtr(),
                                r0_predicted_edge.dataPtr(),
                                w0.dataPtr(),
                                startcomp, endcomp);
                // z-direction
#pragma gpu box(zbx)
                make_rhoX_flux_3d(AMREX_INT_ANYD(zbx.loVect()),
                                AMREX_INT_ANYD(zbx.hiVect()),
                                lev, 3,
                                BL_TO_FORTRAN_ANYD(sfluxz_mf[mfi]), sfluxz_mf.nComp(),
                                BL_TO_FORTRAN_ANYD(etarhoflux_mf[mfi]),
                                BL_TO_FORTRAN_ANYD(sedgex_mf[mfi]), sedgex_mf.nComp(),
                                BL_TO_FORTRAN_ANYD(sedgey_mf[mfi]), sedgey_mf.nComp(),
                                BL_TO_FORTRAN_ANYD(sedgez_mf[mfi]), sedgez_mf.nComp(),
                                BL_TO_FORTRAN_ANYD(umac_mf[mfi]),
                                BL_TO_FORTRAN_ANYD(vmac_mf[mfi]),
                                BL_TO_FORTRAN_ANYD(wmac_mf[mfi]),
                                r0_old.dataPtr(), r0_edge_old.dataPtr(),
                                r0_new.dataPtr(), r0_edge_new.dataPtr(),
                                r0_predicted_edge.dataPtr(),
                                w0.dataPtr(),
                                startcomp, endcomp);
    	    } else {
    		    make_rhoX_flux_3d_sphr(tileBox.loVect(), tileBox.hiVect(),
                                    BL_TO_FORTRAN_FAB(sfluxx_mf[mfi]),
                                    BL_TO_FORTRAN_FAB(sfluxy_mf[mfi]),
                                    BL_TO_FORTRAN_FAB(sfluxz_mf[mfi]),
                                    BL_TO_FORTRAN_FAB(sedgex_mf[mfi]),
                                    BL_TO_FORTRAN_FAB(sedgey_mf[mfi]),
                                    BL_TO_FORTRAN_FAB(sedgez_mf[mfi]),
                                    BL_TO_FORTRAN_3D(umac_mf[mfi]),
                                    BL_TO_FORTRAN_3D(vmac_mf[mfi]),
                                    BL_TO_FORTRAN_3D(wmac_mf[mfi]),
                                    BL_TO_FORTRAN_3D(w0macx_mf[mfi]),
                                    BL_TO_FORTRAN_3D(w0macy_mf[mfi]),
                                    BL_TO_FORTRAN_3D(w0macz_mf[mfi]),
                                    BL_TO_FORTRAN_3D(rho0mac_edgex[mfi]),
                                    BL_TO_FORTRAN_3D(rho0mac_edgey[mfi]),
                                    BL_TO_FORTRAN_3D(rho0mac_edgez[mfi]),
                                    &startcomp, &endcomp);
	    } // end spherical
#endif
	} // end MFIter loop

	// increment or decrement the flux registers by area and time-weighted fluxes
        // Note that the fluxes need to be scaled by dt and area
        // In this example we are solving s_t = -div(+F)
        // The fluxes contain, e.g., F_{i+1/2,j} = (s*u)_{i+1/2,j}
        // Keep this in mind when considering the different sign convention for updating
        // the flux registers from the coarse or fine grid perspective
        // NOTE: the flux register associated with flux_reg_s[lev] is associated
        // with the lev/lev-1 interface (and has grid spacing associated with lev-1)
        if (reflux_type == 2) {

    	    // Get the grid size
    	    const Real* dx = geom[lev].CellSize();
    	    // NOTE: areas are different in DIM=2 and DIM=3
#if (AMREX_SPACEDIM == 3)
    	    const Real area[3] = {dx[1]*dx[2], dx[0]*dx[2], dx[0]*dx[1]};
#else
    	    const Real area[2] = {dx[1], dx[0]};
#endif

    	    if (flux_reg_s[lev+1])
            {
                for (int i = 0; i < AMREX_SPACEDIM; ++i) {
                    // update the lev+1/lev flux register (index lev+1)
                    flux_reg_s[lev+1]->CrseInit(sflux[lev][i],i,start_comp,start_comp,num_comp, -1.0*dt*area[i]);
		    // also include density flux
                    flux_reg_s[lev+1]->CrseInit(sflux[lev][i],i,Rho,Rho,1, -1.0*dt*area[i]);
                }
            }
    	    if (flux_reg_s[lev])
            {
                for (int i = 0; i < AMREX_SPACEDIM; ++i) {
                    // update the lev/lev-1 flux register (index lev)
                    flux_reg_s[lev]->FineAdd(sflux[lev][i],i,start_comp,start_comp,num_comp, 1.0*dt*area[i]);
		    // also include density flux
                    flux_reg_s[lev]->FineAdd(sflux[lev][i],i,Rho,Rho,1, 1.0*dt*area[i]);
                }
            }

    	    if (spherical == 0) {
    		// need edge_restrict for etarhoflux
    	    }
        }

    } // end loop over levels

    // average down fluxes
    if (reflux_type == 1) {
    	AverageDownFaces(sflux);
    }

    // Something analogous to edge_restriction is done in UpdateScal()

#ifdef AMREX_USE_CUDA
    // turn off GPU
    if (not_launched) Gpu::setLaunchRegion(false);
#endif
}