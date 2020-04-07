#include <Maestro.H>
#include <Maestro_F.H>

using namespace amrex;

void 
Maestro::MakePsiPlanar()
{
    // timer for profiling
    BL_PROFILE_VAR("Maestro::MakePsiPlanar()", MakePsiPlanar);

    const int max_lev = max_radial_level + 1;

    psi.setVal(0.0);

    for (auto n = 0; n <= finest_radial_level; ++n) {
        for (auto i = 1; i <= numdisjointchunks(n); ++i){
            for (auto r = r_start_coord(n,i); 
                 r<= r_end_coord(n,i); ++r) {
                if (r < base_cutoff_density_coord(n)) {
                    psi(n,r) = etarho_cc(n,r) * fabs(grav_const);
                }
            }
        }
    }
    RestrictBase(psi, true);
    FillGhostBase(psi, true);
}

void 
Maestro::MakePsiSphr(const BaseState<Real>& gamma1bar, 
                     const BaseState<Real>& p0_avg,
                     const BaseState<Real>& Sbar_in) 
{
    // timer for profiling
    BL_PROFILE_VAR("Maestro::MakePsiSphr()", MakePsiSphr);

    const int max_lev = max_radial_level + 1;

    psi.setVal(0.0);

    Real dr0 = dr(0);

    const auto& r_cc_loc_p = r_cc_loc_b;
    const auto& r_edge_loc_p = r_edge_loc_b;
    const Real * AMREX_RESTRICT w0_p = w0.dataPtr();
    auto &psi_p = psi;

    const auto npts = base_cutoff_density_coord(0);
    AMREX_PARALLEL_FOR_1D(npts, r, {
        Real div_w0_sph = 1.0 / (r_cc_loc_p(0,r)*r_cc_loc_p(0,r)) * 
            (r_edge_loc_p(0,r+1)*r_edge_loc_p(0,r+1) *
             w0_p[max_lev*(r+1)] - 
             r_edge_loc_p(0,r)*r_edge_loc_p(0,r) * 
             w0_p[max_lev*r]) / dr0;
        psi_p(0,r) = gamma1bar(0,r) * p0_avg(0,r) * 
            (Sbar_in(0,r) - div_w0_sph);
    });
}

void 
Maestro::MakePsiIrreg(const BaseState<Real>& grav_cell) 
{
    // timer for profiling
    BL_PROFILE_VAR("Maestro::MakePsiIrreg()", MakePsiIrreg);

    const int max_lev = max_radial_level+1;

    psi.setVal(0.0);

    const auto& etarho_cc_p = etarho_cc;
    auto& psi_p = psi;

    const auto npts = base_cutoff_density_coord(0);
    AMREX_PARALLEL_FOR_1D(npts, r, {
        psi_p(0,r) = etarho_cc_p(0,r) * grav_cell(0,r);
    });

    for (auto r = base_cutoff_density_coord(0)+1; r < nr_fine; ++r) {
        psi(0,r) = psi(0,r-1);
    }

    RestrictBase(psi, true);
    FillGhostBase(psi, true);
}
