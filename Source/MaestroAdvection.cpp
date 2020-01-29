
#include <Maestro.H>
#include <Maestro_F.H>
#include <MaestroBCThreads.H>

using namespace amrex;

// compute unprojected mac velocities
void
Maestro::AdvancePremac (Vector<std::array< MultiFab, AMREX_SPACEDIM > >& umac,
                        const Vector<std::array< MultiFab,AMREX_SPACEDIM > >& w0mac,
                        const RealVector& w0_force,
                        const Vector<MultiFab>& w0_force_cart)
{
	// timer for profiling
	BL_PROFILE_VAR("Maestro::AdvancePremac()",AdvancePremac);

	// create a uold with filled ghost cells
	Vector<MultiFab> utilde(finest_level+1);
	for (int lev=0; lev<=finest_level; ++lev) {
		utilde[lev].define(grids[lev], dmap[lev], AMREX_SPACEDIM, ng_adv);
		utilde[lev].setVal(0.);
	}

	FillPatch(t_new, utilde, uold, uold, 0, 0, AMREX_SPACEDIM, 0, bcs_u, 1);

	// create a MultiFab to hold uold + w0
	Vector<MultiFab> ufull(finest_level+1);
	for (int lev=0; lev<=finest_level; ++lev) {
		ufull[lev].define(grids[lev], dmap[lev], AMREX_SPACEDIM, ng_adv);
        // needed to avoid NaNs in filling corner ghost cells with 2 physical boundaries
        ufull[lev].setVal(0.);
	}

	// create ufull = uold + w0
    for (int lev=0; lev<=finest_level; ++lev) {
        MultiFab::Copy(ufull[lev], w0_cart[lev], 0, 0, AMREX_SPACEDIM, 0);
    }
    // fill ufull ghost cells
    FillPatch(t_old, ufull, ufull, ufull, 0, 0, AMREX_SPACEDIM, 0, bcs_u, 1);
	for (int lev=0; lev<=finest_level; ++lev) {
            MultiFab::Add(ufull[lev],utilde[lev],0,0,AMREX_SPACEDIM,ng_adv);
	}
        
	// create a face-centered MultiFab to hold utrans
	Vector<std::array< MultiFab, AMREX_SPACEDIM > > utrans(finest_level+1);
	for (int lev=0; lev<=finest_level; ++lev) {
		utrans[lev][0].define(convert(grids[lev],nodal_flag_x), dmap[lev], 1, 1);
		utrans[lev][1].define(convert(grids[lev],nodal_flag_y), dmap[lev], 1, 1);
#if (AMREX_SPACEDIM == 3)
		utrans[lev][2].define(convert(grids[lev],nodal_flag_z), dmap[lev], 1, 1);
#endif
		for (int j=0; j < AMREX_SPACEDIM; j++)
			utrans[lev][j].setVal(0.);
	}

	// create utrans
	MakeUtrans(utilde,ufull,utrans,w0mac);

	// create a MultiFab to hold the velocity forcing
	Vector<MultiFab> vel_force(finest_level+1);
	for (int lev=0; lev<=finest_level; ++lev) {
		if (ppm_trace_forces == 0) {
			vel_force[lev].define(grids[lev], dmap[lev], AMREX_SPACEDIM, 1);
		} else {
			// tracing needs more ghost cells
			vel_force[lev].define(grids[lev], dmap[lev], AMREX_SPACEDIM, ng_s);
		}
		vel_force[lev].setVal(0.);

	}

	int do_add_utilde_force = 1;
	MakeVelForce(vel_force,utrans,sold,rho0_old,grav_cell_old,
	             w0_force_cart,do_add_utilde_force);

	// add w0 to trans velocities
	Addw0 (utrans,w0mac,1.);

	VelPred(utilde,ufull,utrans,umac,w0mac,vel_force);
}


void
<<<<<<< HEAD
=======
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

        // loop over boxes (make sure mfi takes a cell-centered multifab as an argument)
#ifdef _OPENMP
#pragma omp parallel
#endif
        for ( MFIter mfi(scal_mf, TilingIfNotGPU()); mfi.isValid(); ++mfi ) {

            // Get the index space of the valid region
            const Box& tileBox = mfi.tilebox();
            const Box& xbx = amrex::growHi(tileBox,0, 1);
            const Box& ybx = amrex::growHi(tileBox,1, 1);
            // const Box& xbx = mfi.nodaltilebox(0);
            // const Box& ybx = mfi.nodaltilebox(1);
#if (AMREX_SPACEDIM == 3)
            const Box& zbx = amrex::growHi(tileBox,2, 1);
            // const Box& zbx = mfi.nodaltilebox(2);
#endif

#if (AMREX_SPACEDIM == 2)
                // x-direction
#pragma gpu box(xbx)
                make_rhoX_flux_2d(AMREX_INT_ANYD(xbx.loVect()),
                    AMREX_INT_ANYD(xbx.hiVect()), lev, 1,
                    BL_TO_FORTRAN_ANYD(sfluxx_mf[mfi]), sfluxx_mf.nComp(),
                    BL_TO_FORTRAN_ANYD(etarhoflux_mf[mfi]),
                    BL_TO_FORTRAN_ANYD(sedgex_mf[mfi]), sedgex_mf.nComp(),
                    BL_TO_FORTRAN_ANYD(umac_mf[mfi]),
                    r0_old.dataPtr(), r0_edge_old.dataPtr(),
                    r0_new.dataPtr(), r0_edge_new.dataPtr(),
                    r0_predicted_edge.dataPtr(),
                    w0.dataPtr(),
                    startcomp, endcomp);

                // y-direction
#pragma gpu box(ybx)
                make_rhoX_flux_2d(AMREX_INT_ANYD(ybx.loVect()),
                    AMREX_INT_ANYD(ybx.hiVect()), lev, 2,
                    BL_TO_FORTRAN_ANYD(sfluxy_mf[mfi]), sfluxy_mf.nComp(),
                    BL_TO_FORTRAN_ANYD(etarhoflux_mf[mfi]),
                    BL_TO_FORTRAN_ANYD(sedgey_mf[mfi]), sedgey_mf.nComp(),
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
                                BL_TO_FORTRAN_ANYD(umac_mf[mfi]),
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
                                BL_TO_FORTRAN_ANYD(sedgey_mf[mfi]), sedgey_mf.nComp(),
                                BL_TO_FORTRAN_ANYD(vmac_mf[mfi]),
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
                                BL_TO_FORTRAN_ANYD(sedgez_mf[mfi]), sedgez_mf.nComp(),
                                BL_TO_FORTRAN_ANYD(wmac_mf[mfi]),
                                r0_old.dataPtr(), r0_edge_old.dataPtr(),
                                r0_new.dataPtr(), r0_edge_new.dataPtr(),
                                r0_predicted_edge.dataPtr(),
                                w0.dataPtr(),
                                startcomp, endcomp);
    	    } else {
                // x-direction
#pragma gpu box(xbx)
    		    make_rhoX_flux_3d_sphr(AMREX_INT_ANYD(xbx.loVect()),
                                    AMREX_INT_ANYD(xbx.hiVect()),
                                    BL_TO_FORTRAN_ANYD(sfluxx_mf[mfi]), sfluxx_mf.nComp(),
                                    BL_TO_FORTRAN_ANYD(sedgex_mf[mfi]), sedgex_mf.nComp(),
                                    BL_TO_FORTRAN_ANYD(umac_mf[mfi]),
                                    BL_TO_FORTRAN_ANYD(w0macx_mf[mfi]),
                                    BL_TO_FORTRAN_ANYD(rho0mac_edgex[mfi]),
                                    startcomp, endcomp);
                // y-direction
#pragma gpu box(ybx)
    		    make_rhoX_flux_3d_sphr(AMREX_INT_ANYD(ybx.loVect()),
                                    AMREX_INT_ANYD(ybx.hiVect()),
                                    BL_TO_FORTRAN_ANYD(sfluxy_mf[mfi]), sfluxy_mf.nComp(),
                                    BL_TO_FORTRAN_ANYD(sedgey_mf[mfi]), sedgey_mf.nComp(),
                                    BL_TO_FORTRAN_ANYD(vmac_mf[mfi]),
                                    BL_TO_FORTRAN_ANYD(w0macy_mf[mfi]),
                                    BL_TO_FORTRAN_ANYD(rho0mac_edgey[mfi]),
                                    startcomp, endcomp);
                // z-direction
#pragma gpu box(zbx)
    		    make_rhoX_flux_3d_sphr(AMREX_INT_ANYD(zbx.loVect()),
                                    AMREX_INT_ANYD(zbx.hiVect()),
                                    BL_TO_FORTRAN_ANYD(sfluxz_mf[mfi]), sfluxz_mf.nComp(),
                                    BL_TO_FORTRAN_ANYD(sedgez_mf[mfi]), sedgez_mf.nComp(),
                                    BL_TO_FORTRAN_ANYD(wmac_mf[mfi]),
                                    BL_TO_FORTRAN_ANYD(w0macz_mf[mfi]),
                                    BL_TO_FORTRAN_ANYD(rho0mac_edgez[mfi]),
                                    startcomp, endcomp);

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
}


void
Maestro::MakeRhoHFlux (const Vector<MultiFab>& state,
                       Vector<std::array< MultiFab, AMREX_SPACEDIM > >& sflux,
                       Vector<std::array< MultiFab, AMREX_SPACEDIM > >& sedge,
                       const Vector<std::array< MultiFab, AMREX_SPACEDIM > >& umac,
                       const Vector<std::array< MultiFab, AMREX_SPACEDIM > >& w0mac,
                       const RealVector& r0_old,
                       const RealVector& r0_edge_old,
                       const Vector<std::array< MultiFab, AMREX_SPACEDIM > >& r0mac_old,
                       const RealVector& r0_new,
                       const RealVector& r0_edge_new,
                       const Vector<std::array< MultiFab, AMREX_SPACEDIM > >& r0mac_new,
                       const RealVector& rh0_old,
                       const RealVector& rh0_edge_old,
                       const Vector<std::array< MultiFab, AMREX_SPACEDIM > >& rh0mac_old,
                       const RealVector& rh0_new,
                       const RealVector& rh0_edge_new,
                       const Vector<std::array< MultiFab, AMREX_SPACEDIM > >& rh0mac_new,
                       const Vector<std::array< MultiFab, AMREX_SPACEDIM > >& h0mac_old,
                       const Vector<std::array< MultiFab, AMREX_SPACEDIM > >& h0mac_new)
{
    // timer for profiling
    BL_PROFILE_VAR("Maestro::MakeRhoHFlux()", MakeRhoHFlux);

    for (int lev=0; lev<=finest_level; ++lev) {

        // get references to the MultiFabs at level lev
        const MultiFab& scal_mf   = state[lev];
        MultiFab& sedgex_mf = sedge[lev][0];
        MultiFab& sfluxx_mf = sflux[lev][0];
        const MultiFab& umac_mf   = umac[lev][0];
        MultiFab& sedgey_mf = sedge[lev][1];
        MultiFab& sfluxy_mf = sflux[lev][1];
        const MultiFab& vmac_mf   = umac[lev][1];

#if (AMREX_SPACEDIM == 3)
        MultiFab& sedgez_mf = sedge[lev][2];
        MultiFab& sfluxz_mf = sflux[lev][2];
        const MultiFab& wmac_mf   = umac[lev][2];

        // if spherical == 1
        const MultiFab& w0macx_mf = w0mac[lev][0];
        const MultiFab& w0macy_mf = w0mac[lev][1];
        const MultiFab& w0macz_mf = w0mac[lev][2];
        MultiFab rho0mac_edgex, rho0mac_edgey, rho0mac_edgez;
        MultiFab h0mac_edgex, h0mac_edgey, h0mac_edgez;
        MultiFab rhoh0mac_edgex, rhoh0mac_edgey, rhoh0mac_edgez;
        
        rho0mac_edgex.define(convert(grids[lev],nodal_flag_x), dmap[lev], 1, 0);
        rho0mac_edgey.define(convert(grids[lev],nodal_flag_y), dmap[lev], 1, 0);
        rho0mac_edgez.define(convert(grids[lev],nodal_flag_z), dmap[lev], 1, 0);
        h0mac_edgex.define(convert(grids[lev],nodal_flag_x), dmap[lev], 1, 0);
        h0mac_edgey.define(convert(grids[lev],nodal_flag_y), dmap[lev], 1, 0);
        h0mac_edgez.define(convert(grids[lev],nodal_flag_z), dmap[lev], 1, 0);
        rhoh0mac_edgex.define(convert(grids[lev],nodal_flag_x), dmap[lev], 1, 0);
        rhoh0mac_edgey.define(convert(grids[lev],nodal_flag_y), dmap[lev], 1, 0);
        rhoh0mac_edgez.define(convert(grids[lev],nodal_flag_z), dmap[lev], 1, 0);

        rho0mac_edgex.setVal(0.);
        rho0mac_edgey.setVal(0.);
        rho0mac_edgez.setVal(0.);

        h0mac_edgex.setVal(0.);
        h0mac_edgey.setVal(0.);
        h0mac_edgez.setVal(0.);

        rhoh0mac_edgex.setVal(0.);
        rhoh0mac_edgey.setVal(0.);
        rhoh0mac_edgez.setVal(0.);

        if (spherical == 1) {
            if (use_exact_base_state) {
                MultiFab::LinComb(rhoh0mac_edgex,0.5,rh0mac_old[lev][0],0,0.5,rh0mac_new[lev][0],0,0,1,0);
                MultiFab::LinComb(rhoh0mac_edgey,0.5,rh0mac_old[lev][1],0,0.5,rh0mac_new[lev][1],0,0,1,0);
                MultiFab::LinComb(rhoh0mac_edgez,0.5,rh0mac_old[lev][2],0,0.5,rh0mac_new[lev][2],0,0,1,0);
            } else {
                MultiFab::LinComb(rho0mac_edgex,0.5,r0mac_old[lev][0],0,0.5,r0mac_new[lev][0],0,0,1,0);
                MultiFab::LinComb(rho0mac_edgey,0.5,r0mac_old[lev][1],0,0.5,r0mac_new[lev][1],0,0,1,0);
                MultiFab::LinComb(rho0mac_edgez,0.5,r0mac_old[lev][2],0,0.5,r0mac_new[lev][2],0,0,1,0);
                MultiFab::LinComb(h0mac_edgex,0.5,h0mac_old[lev][0],0,0.5,h0mac_new[lev][0],0,0,1,0);
                MultiFab::LinComb(h0mac_edgey,0.5,h0mac_old[lev][1],0,0.5,h0mac_new[lev][1],0,0,1,0);
                MultiFab::LinComb(h0mac_edgez,0.5,h0mac_old[lev][2],0,0.5,h0mac_new[lev][2],0,0,1,0);
            }
        }
#endif

        // loop over boxes (make sure mfi takes a cell-centered multifab as an argument)
#ifdef _OPENMP
#pragma omp parallel
#endif
        for ( MFIter mfi(scal_mf, TilingIfNotGPU()); mfi.isValid(); ++mfi ) {

            // Get the index space of the valid region
            const Box& tileBox = mfi.tilebox();
            const Box& xbx = mfi.nodaltilebox(0);
            const Box& ybx = mfi.nodaltilebox(1);
#if (AMREX_SPACEDIM == 3)
            const Box& zbx = mfi.nodaltilebox(2);
#endif

            // call fortran subroutine
            // use macros in AMReX_ArrayLim.H to pass in each FAB's data,
            // lo/hi coordinates (including ghost cells), and/or the # of components
            // We will also pass "validBox", which specifies the "valid" region.
#if (AMREX_SPACEDIM == 2)
            // x-direction
#pragma gpu box(xbx)
            make_rhoh_flux_2d(AMREX_INT_ANYD(xbx.loVect()),
                              AMREX_INT_ANYD(xbx.hiVect()),
                              lev, 1,
                              BL_TO_FORTRAN_ANYD(sfluxx_mf[mfi]), sfluxx_mf.nComp(),
                              BL_TO_FORTRAN_ANYD(sedgex_mf[mfi]), sedgex_mf.nComp(),
                              BL_TO_FORTRAN_ANYD(umac_mf[mfi]),
                              r0_old.dataPtr(), r0_edge_old.dataPtr(),
                              r0_new.dataPtr(), r0_edge_new.dataPtr(),
                              rh0_old.dataPtr(), rh0_edge_old.dataPtr(),
                              rh0_new.dataPtr(), rh0_edge_new.dataPtr(),
                              w0.dataPtr());

            // y-direction
#pragma gpu box(ybx)
            make_rhoh_flux_2d(AMREX_INT_ANYD(ybx.loVect()),
                              AMREX_INT_ANYD(ybx.hiVect()),
                              lev, 2,
                              BL_TO_FORTRAN_ANYD(sfluxy_mf[mfi]), sfluxy_mf.nComp(),
                              BL_TO_FORTRAN_ANYD(sedgey_mf[mfi]), sedgey_mf.nComp(),
                              BL_TO_FORTRAN_ANYD(vmac_mf[mfi]),
                              r0_old.dataPtr(), r0_edge_old.dataPtr(),
                              r0_new.dataPtr(), r0_edge_new.dataPtr(),
                              rh0_old.dataPtr(), rh0_edge_old.dataPtr(),
                              rh0_new.dataPtr(), rh0_edge_new.dataPtr(),
                              w0.dataPtr());

#elif (AMREX_SPACEDIM == 3)

            if (spherical == 0) {
                // x-direction
#pragma gpu box(xbx)
                make_rhoh_flux_3d(AMREX_INT_ANYD(xbx.loVect()),
                    AMREX_INT_ANYD(xbx.hiVect()),
                    lev, 1,
                    BL_TO_FORTRAN_ANYD(sfluxx_mf[mfi]), sfluxx_mf.nComp(),
                    BL_TO_FORTRAN_ANYD(sedgex_mf[mfi]), sedgex_mf.nComp(),
                    BL_TO_FORTRAN_ANYD(umac_mf[mfi]),
                    r0_old.dataPtr(), r0_edge_old.dataPtr(),
                    r0_new.dataPtr(), r0_edge_new.dataPtr(),
                    rh0_old.dataPtr(), rh0_edge_old.dataPtr(),
                    rh0_new.dataPtr(), rh0_edge_new.dataPtr(),
                    w0.dataPtr());

                // y-direction
#pragma gpu box(ybx)
                make_rhoh_flux_3d(AMREX_INT_ANYD(ybx.loVect()),
                    AMREX_INT_ANYD(ybx.hiVect()),
                    lev, 2,
                    BL_TO_FORTRAN_ANYD(sfluxy_mf[mfi]), sfluxy_mf.nComp(),
                    BL_TO_FORTRAN_ANYD(sedgey_mf[mfi]), sedgey_mf.nComp(),
                    BL_TO_FORTRAN_ANYD(vmac_mf[mfi]),
                    r0_old.dataPtr(), r0_edge_old.dataPtr(),
                    r0_new.dataPtr(), r0_edge_new.dataPtr(),
                    rh0_old.dataPtr(), rh0_edge_old.dataPtr(),
                    rh0_new.dataPtr(), rh0_edge_new.dataPtr(),
                    w0.dataPtr());

                // z-direction
#pragma gpu box(zbx)
                make_rhoh_flux_3d(AMREX_INT_ANYD(zbx.loVect()),
                    AMREX_INT_ANYD(zbx.hiVect()),
                    lev, 3,
                    BL_TO_FORTRAN_ANYD(sfluxz_mf[mfi]), sfluxz_mf.nComp(),
                    BL_TO_FORTRAN_ANYD(sedgez_mf[mfi]), sedgez_mf.nComp(),
                    BL_TO_FORTRAN_ANYD(wmac_mf[mfi]),
                    r0_old.dataPtr(), r0_edge_old.dataPtr(),
                    r0_new.dataPtr(), r0_edge_new.dataPtr(),
                    rh0_old.dataPtr(), rh0_edge_old.dataPtr(),
                    rh0_new.dataPtr(), rh0_edge_new.dataPtr(),
                    w0.dataPtr());
            } else {

                if (use_exact_base_state) {
                    // x-direction
#pragma gpu box(xbx)
                    make_rhoh_flux_3d_sphr_irreg(AMREX_INT_ANYD(xbx.loVect()),
                                                 AMREX_INT_ANYD(xbx.hiVect()),
                                                 BL_TO_FORTRAN_ANYD(sfluxx_mf[mfi]), sfluxx_mf.nComp(),
                                                 BL_TO_FORTRAN_ANYD(sedgex_mf[mfi]), sedgex_mf.nComp(),
                                                 BL_TO_FORTRAN_ANYD(umac_mf[mfi]),
                                                 BL_TO_FORTRAN_ANYD(w0macx_mf[mfi]),
                                                 BL_TO_FORTRAN_ANYD(rhoh0mac_edgex[mfi]));
                     // y-direction
#pragma gpu box(ybx)
                     make_rhoh_flux_3d_sphr_irreg(AMREX_INT_ANYD(ybx.loVect()),
                                                  AMREX_INT_ANYD(ybx.hiVect()),
                                                  BL_TO_FORTRAN_ANYD(sfluxy_mf[mfi]), sfluxy_mf.nComp(),
                                                  BL_TO_FORTRAN_ANYD(sedgey_mf[mfi]), sedgey_mf.nComp(),
                                                  BL_TO_FORTRAN_ANYD(vmac_mf[mfi]),
                                                  BL_TO_FORTRAN_ANYD(w0macy_mf[mfi]),
                                                  BL_TO_FORTRAN_ANYD(rhoh0mac_edgey[mfi]));
                  // z-direction
#pragma gpu box(zbx)
                  make_rhoh_flux_3d_sphr_irreg(AMREX_INT_ANYD(zbx.loVect()),
                                               AMREX_INT_ANYD(zbx.hiVect()),
                                               BL_TO_FORTRAN_ANYD(sfluxz_mf[mfi]), sfluxz_mf.nComp(),
                                               BL_TO_FORTRAN_ANYD(sedgez_mf[mfi]), sedgez_mf.nComp(),
                                               BL_TO_FORTRAN_ANYD(wmac_mf[mfi]),
                                               BL_TO_FORTRAN_ANYD(w0macz_mf[mfi]),
                                               BL_TO_FORTRAN_ANYD(rhoh0mac_edgez[mfi]));
                }
                else
                {
                    // x-direction
#pragma gpu box(xbx)
                    make_rhoh_flux_3d_sphr(AMREX_INT_ANYD(xbx.loVect()),
                                           AMREX_INT_ANYD(xbx.hiVect()),
                                           BL_TO_FORTRAN_ANYD(sfluxx_mf[mfi]), sfluxx_mf.nComp(),
                                           BL_TO_FORTRAN_ANYD(sedgex_mf[mfi]), sedgex_mf.nComp(),
                                           BL_TO_FORTRAN_ANYD(umac_mf[mfi]),
                                           BL_TO_FORTRAN_ANYD(w0macx_mf[mfi]),
                                           BL_TO_FORTRAN_ANYD(rho0mac_edgex[mfi]),
                                           BL_TO_FORTRAN_ANYD(h0mac_edgex[mfi]));
                   // y-direction
#pragma gpu box(ybx)
                   make_rhoh_flux_3d_sphr(AMREX_INT_ANYD(ybx.loVect()),
                                          AMREX_INT_ANYD(ybx.hiVect()),
                                          BL_TO_FORTRAN_ANYD(sfluxy_mf[mfi]), sfluxy_mf.nComp(),
                                          BL_TO_FORTRAN_ANYD(sedgey_mf[mfi]), sedgey_mf.nComp(),
                                          BL_TO_FORTRAN_ANYD(vmac_mf[mfi]),
                                          BL_TO_FORTRAN_ANYD(w0macy_mf[mfi]),
                                          BL_TO_FORTRAN_ANYD(rho0mac_edgey[mfi]),
                                          BL_TO_FORTRAN_ANYD(h0mac_edgey[mfi]));
                  // z-direction
#pragma gpu box(zbx)
                  make_rhoh_flux_3d_sphr(AMREX_INT_ANYD(zbx.loVect()),
                                         AMREX_INT_ANYD(zbx.hiVect()),
                                         BL_TO_FORTRAN_ANYD(sfluxz_mf[mfi]), sfluxz_mf.nComp(),
                                         BL_TO_FORTRAN_ANYD(sedgez_mf[mfi]), sedgez_mf.nComp(),
                                         BL_TO_FORTRAN_ANYD(wmac_mf[mfi]),
                                         BL_TO_FORTRAN_ANYD(w0macz_mf[mfi]),
                                         BL_TO_FORTRAN_ANYD(rho0mac_edgez[mfi]),
                                         BL_TO_FORTRAN_ANYD(h0mac_edgez[mfi]));
                }
            }
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
                    flux_reg_s[lev+1]->CrseInit(sflux[lev][i],i,RhoH,RhoH,1, -1.0*dt*area[i]);
                }
            }
            if (flux_reg_s[lev])
            {
                for (int i = 0; i < AMREX_SPACEDIM; ++i) {
                    // update the lev/lev-1 flux register (index lev)
                    flux_reg_s[lev]->FineAdd(sflux[lev][i],i,RhoH,RhoH,1, 1.0*dt*area[i]);
                }
            }
        }
    } // end loop over levels

    if (reflux_type == 1) {
        AverageDownFaces(sflux);
    }

    // Something analogous to edge_restriction is done in UpdateScal()

}

void
>>>>>>> development
Maestro::UpdateScal(const Vector<MultiFab>& stateold,
                    Vector<MultiFab>& statenew,
                    const Vector<std::array< MultiFab, AMREX_SPACEDIM > >& sflux,
                    const Vector<MultiFab>& force,
                    int start_comp, int num_comp,
                    const Vector<MultiFab>& p0_cart)
{
    // timer for profiling
    BL_PROFILE_VAR("Maestro::UpdateScal()", UpdateScal);

    const int rho_comp = Rho;
    const int rhoh_comp = RhoH;
    const int spec_comp = FirstSpec;
    const int nspec = NumSpec;
    const Real dt_loc = dt;
    const Real base_cutoff_dens_loc = base_cutoff_density;
    const bool do_eos_h_above_cutoff_loc = do_eos_h_above_cutoff;

    for (int lev=0; lev<=finest_level; ++lev) {

        // get references to the MultiFabs at level lev
        const MultiFab& scalold_mf = stateold[lev];
        MultiFab& scalnew_mf = statenew[lev];
        const MultiFab& sfluxx_mf = sflux[lev][0];
        const MultiFab& sfluxy_mf = sflux[lev][1];
#if (AMREX_SPACEDIM == 3)
        const MultiFab& sfluxz_mf = sflux[lev][2];
#endif
    	const MultiFab& p0cart_mf = p0_cart[lev];
        const MultiFab& force_mf = force[lev];

        const auto dx = geom[lev].CellSizeArray();
        const Real* d_x = geom[lev].CellSize();

        // loop over boxes (make sure mfi takes a cell-centered multifab as an argument)
#ifdef _OPENMP
#pragma omp parallel
#endif
        for ( MFIter mfi(scalold_mf, TilingIfNotGPU()); mfi.isValid(); ++mfi ) {

            // Get the index space of the valid region
            const Box& tileBox = mfi.tilebox();

            const Array4<const Real> sold_arr = stateold[lev].array(mfi);
            const Array4<Real> snew_arr = statenew[lev].array(mfi);
            const Array4<const Real> sfluxx = sflux[lev][0].array(mfi);
            const Array4<const Real> sfluxy = sflux[lev][1].array(mfi);
#if (AMREX_SPACEDIM == 3)
            const Array4<const Real> sfluxz = sflux[lev][2].array(mfi);
#endif
            const Array4<const Real> force_arr = force[lev].array(mfi);
            
            if (start_comp == RhoH) {   
                // Enthalpy update
                const Array4<const Real> p0_arr = p0_cart[lev].array(mfi);

                AMREX_PARALLEL_FOR_3D(tileBox, i, j, k, {
                    Real divterm = ((sfluxx(i+1,j,k,rhoh_comp) - sfluxx(i,j,k,rhoh_comp))/dx[0]
                        + (sfluxy(i,j+1,k,rhoh_comp) - sfluxy(i,j,k,rhoh_comp))/dx[1]
#if (AMREX_SPACEDIM == 3)
                        + (sfluxz(i,j,k+1,rhoh_comp) - sfluxz(i,j,k,rhoh_comp))/dx[2]
#endif
                        );
             
                    snew_arr(i,j,k,rhoh_comp) = sold_arr(i,j,k,rhoh_comp) 
                        + dt_loc * (-divterm + force_arr(i,j,k,rhoh_comp));

                    if (do_eos_h_above_cutoff_loc && snew_arr(i,j,k,rho_comp) <= base_cutoff_dens_loc) {
                        // need Microphysics C++
                    }
                });

                // this just does the EoS stuff now
                if (do_eos_h_above_cutoff) {
#pragma gpu box(tileBox)
                    update_rhoh(AMREX_INT_ANYD(tileBox.loVect()), 
                        AMREX_INT_ANYD(tileBox.hiVect()),
                        BL_TO_FORTRAN_ANYD(scalold_mf[mfi]),
                        BL_TO_FORTRAN_ANYD(scalnew_mf[mfi]),
                        BL_TO_FORTRAN_ANYD(sfluxx_mf[mfi]),
                        BL_TO_FORTRAN_ANYD(sfluxy_mf[mfi]),
#if (AMREX_SPACEDIM == 3)
                        BL_TO_FORTRAN_ANYD(sfluxz_mf[mfi]),
#endif
                        BL_TO_FORTRAN_ANYD(force_mf[mfi]),
                        BL_TO_FORTRAN_ANYD(p0cart_mf[mfi]),
                        AMREX_REAL_ANYD(d_x), dt,
                        NumSpec);
                }

            } else if (start_comp == FirstSpec) {   
                // RhoX update

                AMREX_PARALLEL_FOR_4D(tileBox, NumSpec, i, j, k, n, {
                    int comp = spec_comp + n;

                    Real divterm = (sfluxx(i+1,j,k,comp) - sfluxx(i,j,k,comp))/dx[0];
                    divterm += (sfluxy(i,j+1,k,comp) - sfluxy(i,j,k,comp))/dx[1];
    #if (AMREX_SPACEDIM == 3)
                    divterm += (sfluxz(i,j,k+1,comp) - sfluxz(i,j,k,comp))/dx[2];
    #endif
                    snew_arr(i,j,k,comp) = sold_arr(i,j,k,comp) 
                        + dt_loc * (-divterm + force_arr(i,j,k,comp));
                });

                AMREX_PARALLEL_FOR_3D(tileBox, i, j, k, {
                    // update density
                    snew_arr(i,j,k,rho_comp) = sold_arr(i,j,k,rho_comp);

                    bool has_negative_species = false;

                    // define the update to rho as the sum of the updates to (rho X)_i
                    for (int comp=start_comp; comp<start_comp+nspec; ++comp) {
                        snew_arr(i,j,k,rho_comp) += snew_arr(i,j,k,comp)-sold_arr(i,j,k,comp);
                        if (snew_arr(i,j,k,comp) < 0.0) 
                            has_negative_species = true;
                    }

                    // enforce a density floor
                    if (snew_arr(i,j,k,rho_comp) < 0.5*base_cutoff_dens_loc) {
                        for (int comp=start_comp; comp<start_comp+nspec; ++comp) {
                            snew_arr(i,j,k,comp) *= 0.5*base_cutoff_dens_loc/snew_arr(i,j,k,rho_comp);
                        }
                        snew_arr(i,j,k,rho_comp) = 0.5*base_cutoff_dens_loc;
                    }

                    // do not allow the species to leave here negative.
                    if (has_negative_species) {
                        for (int comp=start_comp; comp<start_comp+nspec; ++comp) {
                            if (snew_arr(i,j,k,comp) < 0.0) {
                                Real delta = -snew_arr(i,j,k,comp);
                                Real sumX = 0.0;
                                for (int comp2=start_comp; comp2<start_comp+nspec; ++comp2) {
                                    if (comp2 != comp && snew_arr(i,j,k,comp2) >= 0.0) {
                                        sumX += snew_arr(i,j,k,comp2);
                                    }
                                }
                                for (int comp2 = start_comp; comp2 < start_comp+nspec; ++comp2) {
                                    if (comp2 != comp && snew_arr(i,j,k,comp2) >= 0.0) {
                                        Real frac = snew_arr(i,j,k,comp2) / sumX;
                                        snew_arr(i,j,k,comp2) -= frac * delta;
                                    }
                                }
                                snew_arr(i,j,k,comp) = 0.0;
                            }
                        }
                    }
                });
            } else {
                Abort("Invalid scalar in UpdateScal().");
            } // }
        } // end MFIter loop
    } // end loop over levels

    // synchronize by refluxing and averaging down, starting from the finest_level-1/finest_level pair
    if (reflux_type == 2) {
        for (int lev=finest_level-1; lev>=0; --lev) {
            // update lev based on coarse-fine flux mismatch
            flux_reg_s[lev+1]->Reflux(statenew[lev], 1.0, start_comp, start_comp, num_comp, geom[lev]);
            if (start_comp == FirstSpec) {
                // do the same for density if we updated the species
                flux_reg_s[lev+1]->Reflux(statenew[lev], 1.0, Rho, Rho, 1, geom[lev]);
            }
        }
    }

    // average fine data onto coarser cells
    // fill ghost cells
    AverageDown(statenew,start_comp,num_comp);
    FillPatch(t_old, statenew, statenew, statenew, start_comp, start_comp, 
        num_comp, start_comp, bcs_s);

    // do the same for density if we updated the species
    if (start_comp == FirstSpec) {
        AverageDown(statenew,Rho,1);
        FillPatch(t_old, statenew, statenew, statenew, Rho, Rho, 1, Rho, bcs_s);
    }
}

void
Maestro::UpdateVel (const Vector<std::array< MultiFab, AMREX_SPACEDIM > >& umac,
                    const Vector<std::array< MultiFab, AMREX_SPACEDIM > >& uedge,
                    const Vector<MultiFab>& force,
                    const Vector<MultiFab>& sponge,
                    const Vector<std::array< MultiFab, AMREX_SPACEDIM > >& w0mac)
{
    // timer for profiling
    BL_PROFILE_VAR("Maestro::UpdateVel()",UpdateVel);

    // 1) Subtract (Utilde dot grad) Utilde term from old Utilde
    // 2) Add forcing term to new Utilde

    const Real dt_loc = dt;
    const bool do_sponge_loc = do_sponge;

    for (int lev=0; lev<=finest_level; ++lev) {

        const MultiFab& force_mf = force[lev];

        const auto dx = geom[lev].CellSizeArray();

        // loop over boxes (make sure mfi takes a cell-centered multifab as an argument)
#ifdef _OPENMP
#pragma omp parallel
#endif
        for ( MFIter mfi(force_mf, TilingIfNotGPU()); mfi.isValid(); ++mfi ) {

            // Get the index space of the valid region
            const Box& tileBox = mfi.tilebox();

            const Array4<const Real> uold_arr = uold[lev].array(mfi);
            const Array4<Real> unew_arr = unew[lev].array(mfi);
            const Array4<const Real> umacx = umac[lev][0].array(mfi);
            const Array4<const Real> uedgex = uedge[lev][0].array(mfi);
            const Array4<const Real> vmac = umac[lev][1].array(mfi);
            const Array4<const Real> uedgey = uedge[lev][1].array(mfi);
#if (AMREX_SPACEDIM == 3)
            const Array4<const Real> wmac = umac[lev][2].array(mfi);
            const Array4<const Real> uedgez = uedge[lev][2].array(mfi);
#endif
            const Array4<const Real> force_arr = force[lev].array(mfi);
            const Array4<const Real> sponge_arr = sponge[lev].array(mfi);
            const Array4<const Real> w0_arr = w0_cart[lev].array(mfi);

            if (spherical == 0) {

                AMREX_PARALLEL_FOR_3D(tileBox, i, j, k, {
                    // create cell-centered Utilde
                    Real ubar = 0.5*(umacx(i,j,k) + umacx(i+1,j,k));
                    Real vbar = 0.5*(vmac(i,j,k) + vmac(i,j+1,k));
#if (AMREX_SPACEDIM == 3)
                    Real wbar = 0.5*(wmac(i,j,k) + wmac(i,j,k+1));
#endif

                    // create (Utilde dot grad) Utilde
                    Real ugradu = (ubar*(uedgex(i+1,j,k,0) - uedgex(i,j,k,0))/dx[0] 
                        + vbar*(uedgey(i,j+1,k,0) - uedgey(i,j,k,0))/dx[1]
#if (AMREX_SPACEDIM == 3)
                        + wbar*(uedgez(i,j,k+1,0) - uedgez(i,j,k,0))/dx[2]
#endif
                        );

                    Real ugradv = (ubar*(uedgex(i+1,j,k,1) - uedgex(i,j,k,1))/dx[0] 
                        + vbar*(uedgey(i,j+1,k,1) - uedgey(i,j,k,1))/dx[1]
#if (AMREX_SPACEDIM == 3)
                        + wbar*(uedgez(i,j,k+1,1) - uedgez(i,j,k,1))/dx[2]
#endif
                        );

#if (AMREX_SPACEDIM == 3)
                    Real ugradw = ubar*(uedgex(i+1,j,k,2) - uedgex(i,j,k,2))/dx[0]
                        + vbar*(uedgey(i,j+1,k,2) - uedgey(i,j,k,2))/dx[1]
                        + wbar*(uedgez(i,j,k+1,2) - uedgez(i,j,k,2))/dx[2];
#endif

                    // update with (Utilde dot grad) Utilde and force
                    unew_arr(i,j,k,0) = uold_arr(i,j,k,0) - dt_loc * ugradu + dt_loc * force_arr(i,j,k,0);
                    unew_arr(i,j,k,1) = uold_arr(i,j,k,1) - dt_loc * ugradv + dt_loc * force_arr(i,j,k,1);
#if (AMREX_SPACEDIM == 3)
                    unew_arr(i,j,k,2) = uold_arr(i,j,k,2) - dt_loc * ugradw + dt_loc * force_arr(i,j,k,2);
#endif

                    // subtract (w0 dot grad) Utilde term
#if (AMREX_SPACEDIM == 2)
                    Real w0bar = 0.5*(w0_arr(i,j,k,AMREX_SPACEDIM-1) + w0_arr(i,j+1,k,AMREX_SPACEDIM-1));
#else 
                    Real w0bar = 0.5*(w0_arr(i,j,k,AMREX_SPACEDIM-1) + w0_arr(i,j,k+1,AMREX_SPACEDIM-1));
#endif

                    for (int n = 0; n < AMREX_SPACEDIM; ++n) {
                        unew_arr(i,j,k,n) -= dt_loc * w0bar * 
#if (AMREX_SPACEDIM == 2)
                        (uedgey(i,j+1,k,n) - uedgey(i,j,k,n))/dx[1];
#else
                        (uedgez(i,j,k+1,n) - uedgez(i,j,k,n))/dx[2];
#endif
                        // Add the sponge
                        if (do_sponge_loc) unew_arr(i,j,k,n) *= sponge_arr(i,j,k);
                    }                    
                });
            } else {
#if (AMREX_SPACEDIM == 3)
                const Array4<const Real> w0macx = w0mac[lev][0].array(mfi);
                const Array4<const Real> w0macy = w0mac[lev][1].array(mfi);
                const Array4<const Real> w0macz = w0mac[lev][2].array(mfi);

                AMREX_PARALLEL_FOR_3D(tileBox, i, j, k, {
                    // create cell-centered Utilde
                    Real ubar = 0.5*(umacx(i,j,k) + umacx(i+1,j,k));
                    Real vbar = 0.5*(vmac(i,j,k) + vmac(i,j+1,k));
                    Real wbar = 0.5*(wmac(i,j,k) + wmac(i,j,k+1));

                    // create (Utilde dot grad) Utilde
                    Real ugradu = ubar*(uedgex(i+1,j,k,0) - uedgex(i,j,k,0))/dx[0] + 
                        vbar*(uedgey(i,j+1,k,0) - uedgey(i,j,k,0))/dx[1] + 
                        wbar*(uedgez(i,j,k+1,0) - uedgez(i,j,k,0))/dx[2];

                    Real ugradv = ubar*(uedgex(i+1,j,k,1) - uedgex(i,j,k,1))/dx[0] +
                        vbar*(uedgey(i,j+1,k,1) - uedgey(i,j,k,1))/dx[1] + 
                        wbar*(uedgez(i,j,k+1,1) - uedgez(i,j,k,1))/dx[2];

                    Real ugradw = ubar*(uedgex(i+1,j,k,2) - uedgex(i,j,k,2))/dx[0] + 
                        vbar*(uedgey(i,j+1,k,2) - uedgey(i,j,k,2))/dx[1] + 
                        wbar*(uedgez(i,j,k+1,2) - uedgez(i,j,k,2))/dx[2];

                    // update with (Utilde dot grad) Utilde and force
                    unew_arr(i,j,k,0) = uold_arr(i,j,k,0) - dt_loc * ugradu + dt_loc * force_arr(i,j,k,0);
                    unew_arr(i,j,k,1) = uold_arr(i,j,k,1) - dt_loc * ugradv + dt_loc * force_arr(i,j,k,1);
                    unew_arr(i,j,k,2) = uold_arr(i,j,k,2) - dt_loc * ugradw + dt_loc * force_arr(i,j,k,2);

                    // Subtract (w0 dot grad) Utilde term from new Utilde
                    Real gradux = (uedgex(i+1,j,k,0) - uedgex(i,j,k,0))/dx[0];
                    Real gradvx = (uedgex(i+1,j,k,1) - uedgex(i,j,k,1))/dx[0];
                    Real gradwx = (uedgex(i+1,j,k,2) - uedgex(i,j,k,2))/dx[0];

                    Real graduy = (uedgey(i,j+1,k,0) - uedgey(i,j,k,0))/dx[1];
                    Real gradvy = (uedgey(i,j+1,k,1) - uedgey(i,j,k,1))/dx[1];
                    Real gradwy = (uedgey(i,j+1,k,2) - uedgey(i,j,k,2))/dx[1];

                    Real graduz = (uedgez(i,j,k+1,0) - uedgez(i,j,k,0))/dx[2];
                    Real gradvz = (uedgez(i,j,k+1,1) - uedgez(i,j,k,1))/dx[2];
                    Real gradwz = (uedgez(i,j,k+1,2) - uedgez(i,j,k,2))/dx[2];

                    Real w0_gradur = gradux * 0.5*(w0macx(i,j,k)+w0macx(i+1,j,k)) 
                        + graduy * 0.5*(w0macy(i,j,k)+w0macy(i,j+1,k)) 
                        + graduz * 0.5*(w0macz(i,j,k)+w0macz(i,j,k+1));

                    Real w0_gradvr = gradvx * 0.5*(w0macx(i,j,k)+w0macx(i+1,j,k)) 
                        + gradvy * 0.5*(w0macy(i,j,k)+w0macy(i,j+1,k)) 
                        + gradvz * 0.5*(w0macz(i,j,k)+w0macz(i,j,k+1));

                    Real w0_gradwr = gradwx * 0.5*(w0macx(i,j,k)+w0macx(i+1,j,k)) 
                        + gradwy * 0.5*(w0macy(i,j,k)+w0macy(i,j+1,k)) 
                        + gradwz * 0.5*(w0macz(i,j,k)+w0macz(i,j,k+1));

                    unew_arr(i,j,k,0) -= dt_loc * w0_gradur;
                    unew_arr(i,j,k,1) -= dt_loc * w0_gradvr;
                    unew_arr(i,j,k,2) -= dt_loc * w0_gradwr;

                    // Add the sponge
                    if (do_sponge_loc) {
                        for (int n = 0; n < AMREX_SPACEDIM; ++n) {
                            unew_arr(i,j,k,n) *= sponge_arr(i,j,k);
                        }
                    }
                });
#else
            Abort("UpdateVel: Spherical is not valid for DIM < 3");
#endif
            }
        } // end MFIter loop
    } // end loop over levels

    // average fine data onto coarser cells
    AverageDown(unew,0,AMREX_SPACEDIM);

    // fill ghost cells
    FillPatch(t_old, unew, unew, unew, 0, 0, AMREX_SPACEDIM, 0, bcs_u, 1);
}
