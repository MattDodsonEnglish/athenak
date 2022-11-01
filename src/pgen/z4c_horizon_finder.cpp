//========================================================================================
// AthenaXXX astrophysical plasma code
// Copyright(C) 2020 James M. Stone <jmstone@ias.edu> and the Athena code team
// Licensed under the 3-clause BSD License (the "LICENSE")
//========================================================================================
//! \file z4c_one_puncture.cpp
//  \brief Problem generator for finding the horizon for a single puncture placed at the origin of the domain
//

#include <algorithm>
#include <cmath>
#include <sstream>
#include <fstream>

#include "parameter_input.hpp"
#include "athena.hpp"
#include "athena_tensor.hpp"
#include "mesh/mesh.hpp"
#include "z4c/z4c.hpp"
#include "adm/adm.hpp"
#include "geodesic-grid/gauss_legendre.hpp"
#include "coordinates/cell_locations.hpp"
#include "utils/finite_diff.hpp"

//----------------------------------------------------------------------------------------
//! \fn ProblemGenerator::UserProblem_()
//! \brief Problem Generator for Testing Horizon Finder

int SYMM2_Ind(int v1, int v2) {
  if (v1==0) {
    return v2;
  } else if (v1==1) {
    if (v2==0) {
      return 1;
    } else {
      return v2+2;
    }
  } else {
    if (v2==0) {
      return 2;
    } else {
      return v2+3;
    }
  }
}

//----------------------------------------------------------------------------------------
// \!fn void DualArray6D<Real> metric_partial(MeshBlockPack *pmbp)
// \brief Compute derivative of g_ij
//
// This sets the d_g_kij everywhere in the MeshBlock
//----------------------------------------------------------------------------------------

template <int NGHOST>
DualArray6D<Real> metric_partial(MeshBlockPack *pmbp) {
  // capture variables for the kernel
  auto &indcs = pmbp->pmesh->mb_indcs;
  auto &size = pmbp->pmb->mb_size;
  int &is = indcs.is; int &ie = indcs.ie;
  int &js = indcs.js; int &je = indcs.je;
  int &ks = indcs.ks; int &ke = indcs.ke;
  int &nghost = indcs.ng;

  //For GLOOPS
  int nmb = pmbp->nmb_thispack;

  // Initialize dg_ddd container
  int ncells1 = indcs.nx1 + 2*nghost;
  int ncells2 = indcs.nx2 + 2*nghost;
  int ncells3 = indcs.nx3 + 2*nghost;
  DualArray6D<Real> dg_ddd_full;
  Kokkos::realloc(dg_ddd_full,nmb,3,6,ncells3,ncells2,ncells1);

  auto &adm = pmbp->padm->adm;
  int scr_level = 1;
  // 2 1D scratch array and 1 2D scratch array
  size_t scr_size = ScrArray2D<Real>::shmem_size(0,0); // 3D tensor with symm
  par_for_outer("ADM constraints loop",DevExeSpace(),scr_size,scr_level,0,nmb-1,ks,ke,js,je,
  KOKKOS_LAMBDA(TeamMember_t member, const int m, const int k, const int j) {

    Real idx[] = {size.d_view(m).idx1, size.d_view(m).idx2, size.d_view(m).idx3};
    // -----------------------------------------------------------------------------------
    // derivatives
    //
    // first derivatives of g
    for(int c = 0; c < 3; ++c)
    for(int a = 0; a < 3; ++a)
    for(int b = a; b < 3; ++b) {
      par_for_inner(member, is, ie, [&](const int i) {
        dg_ddd_full.d_view(m,c,SYMM2_Ind(a,b),k,j,i) = Dx<NGHOST>(c, idx, adm.g_dd, m,a,b,k,j,i);
      });
    }
  });

  // sync to host
  dg_ddd_full.template modify<DevExeSpace>();
  dg_ddd_full.template sync<HostMemSpace>();
  return dg_ddd_full;
}

template DualArray6D<Real> metric_partial<2>(MeshBlockPack *pmbp);
template DualArray6D<Real> metric_partial<3>(MeshBlockPack *pmbp);
template DualArray6D<Real> metric_partial<4>(MeshBlockPack *pmbp);


//----------------------------------------------------------------------------------------
// \!fn void AthenaSurfaceTensor<Real,TensorSymm::NONE,3,0>  SurfaceNullExpansion(MeshBlockPack *pmbp, 
// GaussLegendreGrid *S, DualArray6D<Real> dg_ddd)
// \brief Compute \rho H for horizon finder
//
// This evaluate the surface null expansion multiplied by a weighting function \rho.
// 
//----------------------------------------------------------------------------------------

AthenaSurfaceTensor<Real,TensorSymm::NONE,3,0> SurfaceNullExpansion(MeshBlockPack *pmbp, GaussLegendreGrid *S, DualArray6D<Real> dg_ddd) {
  // Load adm variables
  auto &adm = pmbp->padm->adm;
  auto &g_dd = adm.g_dd;
  auto &K_dd = adm.K_dd;

  int nangles = S->nangles;
  auto surface_jacobian = S->surface_jacobian;
  auto d_surface_jacobian = S->d_surface_jacobian;

  // *****************  Step 4 of Schnetter 2002  *******************

  // Interpolate g_dd, K_dd, and dg_ddd onto the surface

  auto g_dd_surf =  S->InterpolateToSphere(g_dd);
  auto K_dd_surf =  S->InterpolateToSphere(K_dd);
  auto dg_ddd_surf = S->InterpolateToSphere(dg_ddd);

  // Calculating g_uu on the sphere 
  // all tensors on surface ends with "_surf"

  AthenaSurfaceTensor<Real,TensorSymm::SYM2,3,2> g_uu_surf;
  g_uu_surf.NewAthenaSurfaceTensor(nangles);

  for(int n=0; n<nangles; ++n) {
    Real detg = SpatialDet(g_dd_surf(0,0,n), g_dd_surf(0,1,n), g_dd_surf(0,2,n),
                           g_dd_surf(1,1,n), g_dd_surf(1,2,n), g_dd_surf(2,2,n));
    SpatialInv(1.0/detg,
            g_dd_surf(0,0,n), g_dd_surf(0,1,n), g_dd_surf(0,2,n),
            g_dd_surf(1,1,n), g_dd_surf(1,2,n), g_dd_surf(2,2,n),
            &g_uu_surf(0,0,n), &g_uu_surf(0,1,n), &g_uu_surf(0,2,n),
            &g_uu_surf(1,1,n), &g_uu_surf(1,2,n), &g_uu_surf(2,2,n));
  }

  // some stuffs for debug
  // std::ofstream spherical_grid_output2;
  /*
  spherical_grid_output2.open ("/home/hzhu/Desktop/research/gr/athenak_versions/athenak/build/cart_pos.out", std::ios_base::app);
  for (int i=0;i<S->nangles;++i) {
    spherical_grid_output2 << S->cart_pos.h_view(i,0) << "\t" << S->cart_pos.h_view(i,1) << "\t" << S->cart_pos.h_view(i,2) << "\n";// << ones_dphi.h_view(i) <<"\n";
  }
  spherical_grid_output2.close();

  spherical_grid_output2.open ("/home/hzhu/Desktop/research/gr/athenak_versions/athenak/build/polar_pos.out", std::ios_base::app);
  for (int i=0;i<S->nangles;++i) {
    spherical_grid_output2 << S->polar_pos.h_view(i,0) << "\t" << S->polar_pos.h_view(i,1) << "\n";// << ones_dphi.h_view(i) <<"\n";
  }
  spherical_grid_output2.close();

  spherical_grid_output2.open ("/home/hzhu/Desktop/research/gr/athenak_versions/athenak/build/metric_on_sphere.out", std::ios_base::app);
  for (int i=0;i<S->nangles;++i) {
    spherical_grid_output2 << g_dd_surf(0,0,i) << "\t" << g_dd_surf(0,1,i) << "\t" << g_dd_surf(0,2,i) << "\t" << g_dd_surf(1,1,i) << "\t" << g_dd_surf(1,2,i)<< "\t" << g_dd_surf(2,2,i) << "\n";// << ones_dphi.h_view(i) <<"\n";
  }
  spherical_grid_output2.close();

  spherical_grid_output2.open ("/home/hzhu/Desktop/research/gr/athenak_versions/athenak/build/metric_uu_on_sphere.out", std::ios_base::app);
  for (int i=0;i<S->nangles;++i) {
    spherical_grid_output2 << g_uu_surf(0,0,i) << "\t" << g_uu_surf(0,1,i) << "\t" << g_uu_surf(0,2,i) << "\t" << g_uu_surf(1,1,i) << "\t" << g_uu_surf(1,2,i)<< "\t" << g_uu_surf(2,2,i) << "\n";// << ones_dphi.h_view(i) <<"\n";
  }
  spherical_grid_output2.close();

  spherical_grid_output2.open ("/home/hzhu/Desktop/research/gr/athenak_versions/athenak/build/interp_indcs.out", std::ios_base::app);
  for (int i=0;i<S->nangles;++i) {
    spherical_grid_output2 << S->interp_indcs.h_view(i,0) << "\t" << S->interp_indcs.h_view(i,1) << "\t" << S->interp_indcs.h_view(i,2) << "\t" << S->interp_indcs.h_view(i,3) << "\n";// << ones_dphi.h_view(i) <<"\n";
  }
  spherical_grid_output2.close();


  spherical_grid_output2.open ("/home/hzhu/Desktop/research/gr/athenak_versions/athenak/build/dg_on_sphere.out", std::ios_base::app);
  for (int i=0;i<S->nangles;++i) {
    spherical_grid_output2 << dg_ddd_surf(0,0,0,i) << "\t" << dg_ddd_surf(0,0,1,i) << "\t" << dg_ddd_surf(0,0,2,i) << "\t" << dg_ddd_surf(0,1,1,i) << "\t" << dg_ddd_surf(0,1,2,i)<< "\t" << dg_ddd_surf(0,2,2,i) << "\n";// << ones_dphi.h_view(i) <<"\n";
  }
  spherical_grid_output2.close();
  */

  // Christoffel symbols of the second kind on the surface, saved as rank3 tensor

  AthenaSurfaceTensor<Real,TensorSymm::SYM2,3,3> Gamma_udd_surf;
  Gamma_udd_surf.NewAthenaSurfaceTensor(nangles);

  for(int n=0; n<nangles; ++n) {
    for(int i=0; i<3; ++i)
    for(int j=0; j<3; ++j)
    for(int k=j; k<3; ++k) {
      Gamma_udd_surf(i,j,k,n) = 0;
      for(int s=0; s<3; ++s) {
        Gamma_udd_surf(i,j,k,n) += 0.5*g_uu_surf(i,s,n) * (dg_ddd_surf(j,k,s,n)
                                  +dg_ddd_surf(k,s,j,n)-dg_ddd_surf(s,j,k,n));
      }
    }
  }

  // *****************  Step 6 of Schnetter 2002  *******************
  // Evaluate Derivatives of F = r - h(theta,phi)
  // First in spherical components "_sb" stands for spherical basis

  AthenaSurfaceTensor<Real,TensorSymm::NONE,3,1> dF_d_surf_sb;
  dF_d_surf_sb.NewAthenaSurfaceTensor(nangles);

  DualArray1D<Real> place_holder_for_partial_theta;
  DualArray1D<Real> place_holder_for_partial_phi;
  Kokkos::realloc(place_holder_for_partial_theta,nangles);
  Kokkos::realloc(place_holder_for_partial_phi,nangles);

  place_holder_for_partial_theta = S->ThetaDerivative(S->pointwise_radius); // these should vanish!
  place_holder_for_partial_phi = S->PhiDerivative(S->pointwise_radius);

  for(int n=0; n<nangles; ++n) {
    // radial derivatives
    dF_d_surf_sb(0,n) = 1;
    // theta and phi derivatives
    dF_d_surf_sb(1,n) = place_holder_for_partial_theta.h_view(n);
    dF_d_surf_sb(2,n) = place_holder_for_partial_phi.h_view(n);
  }

  // Evaluate Second Derivatives of F in spherical components
  AthenaSurfaceTensor<Real,TensorSymm::SYM2,3,2> ddF_dd_surf_sb;
  ddF_dd_surf_sb.NewAthenaSurfaceTensor(nangles);

  DualArray1D<Real> place_holder_for_second_partials;
  Kokkos::realloc(place_holder_for_second_partials,nangles);

  // all second derivatives w.r.t. r vanishes as dr_F = 1
  for(int n=0; n<nangles; ++n) {
    ddF_dd_surf_sb(0,0,n) = 0;
    ddF_dd_surf_sb(0,1,n) = 0;
    ddF_dd_surf_sb(0,2,n) = 0;
  }
  // tt
  place_holder_for_second_partials = S->ThetaDerivative(place_holder_for_partial_theta);
  for(int n=0; n<nangles; ++n) {
    ddF_dd_surf_sb(1,1,n) = place_holder_for_second_partials.h_view(n);
  }
  // tp
  place_holder_for_second_partials = S->PhiDerivative(place_holder_for_partial_theta);
  for(int n=0; n<nangles; ++n) {
    ddF_dd_surf_sb(1,2,n) = place_holder_for_second_partials.h_view(n);
  }
  // pp
  place_holder_for_second_partials = S->PhiDerivative(place_holder_for_partial_phi);
  for(int n=0; n<nangles; ++n) {
    ddF_dd_surf_sb(2,2,n) = place_holder_for_second_partials.h_view(n);
  }

  // Convert to derivatives of F to cartesian basis
  AthenaSurfaceTensor<Real,TensorSymm::NONE,3,1> dF_d_surf;
  dF_d_surf.NewAthenaSurfaceTensor(nangles);

  // check the Surface Jacobian
  for(int n=0; n<nangles; ++n) {
    for(int i=0; i<3;++i) {
      dF_d_surf(i,n) = 0;
      for(int u=0; u<3;++u) {
        dF_d_surf(i,n) += surface_jacobian.h_view(n,u,i)*dF_d_surf_sb(u,n);
      }
    }
  }

  // Second Covariant derivatives of F in cartesian basis
  AthenaSurfaceTensor<Real,TensorSymm::SYM2,3,2> ddF_dd_surf;
  ddF_dd_surf.NewAthenaSurfaceTensor(nangles);
  for(int n=0; n<nangles; ++n) {
    for(int i=0; i<3;++i) {
      for(int j=0; j<3;++j) {
        ddF_dd_surf(i,j,n) = 0;
        for(int v=0; v<3;++v) {
          ddF_dd_surf(i,j,n) += d_surface_jacobian.h_view(n,i,v,j)*dF_d_surf_sb(v,n);
          ddF_dd_surf(i,j,n) += -Gamma_udd_surf(v,i,j,n)*dF_d_surf(v,n);
          for(int u=0; u<3;++u) {
            ddF_dd_surf(i,j,n) += surface_jacobian.h_view(n,v,j)*surface_jacobian.h_view(n,u,i)
                                                  *ddF_dd_surf_sb(u,v,n);
          }
        }
      }
    }
  }

  // auxiliary variable delta_F_abs, Gundlach 1997 eqn 8

  AthenaSurfaceTensor<Real,TensorSymm::NONE,3,0> delta_F_abs;
  delta_F_abs.NewAthenaSurfaceTensor(nangles);

  for(int n=0; n<nangles; ++n) {
    Real delta_F_sqr = 0;
    for(int i=0; i<3;++i) {
      for(int j=0; j<3;++j) {
        delta_F_sqr += g_uu_surf(i,j,n)*dF_d_surf(i,n)
                                *dF_d_surf(j,n);
      }
    }
    delta_F_abs(n) = sqrt(delta_F_sqr);
  }

  // contravariant form of delta_F

  AthenaSurfaceTensor<Real,TensorSymm::NONE,3,1> dF_u_surf;
  dF_u_surf.NewAthenaSurfaceTensor(nangles);

  for(int n=0; n<nangles; ++n) {
    for(int i=0; i<3;++i) {
      dF_u_surf(i,n) = 0;
      for(int j=0; j<3;++j) {
        dF_u_surf(i,n) += g_uu_surf(i,j,n)*dF_d_surf(j,n);
      }
    }
  }

  // Surface unit normal (in cartesian coordinate), Gundlach 1997 eqn 8
  AthenaSurfaceTensor<Real,TensorSymm::NONE,3,1> s;
  s.NewAthenaSurfaceTensor(nangles);

  for(int n=0; n<nangles; ++n) {
    for(int i=0; i<3;++i) {
      s(i,n) = 0;
      for(int j=0; j<3;++j) {
        s(i,n) += g_uu_surf(i,j,n)*dF_d_surf(j,n)/delta_F_abs(n);
      }
    }
  }

  // projection op (in cartesian coordinate), Gundlach 1997 eqn 28
  AthenaSurfaceTensor<Real,TensorSymm::SYM2,3,2> p1_uu_surf;
  p1_uu_surf.NewAthenaSurfaceTensor(nangles);

  for(int n=0; n<nangles; ++n) {
    for(int i=0; i<3;++i) {
      for(int j=0; j<3;++j) {
        p1_uu_surf(i,j,n) = g_uu_surf(i,j,n) - s(i,n)*s(j,n);
      }
    }
  }

  // radius and its flat derivatives
  S->pointwise_radius.h_view;
  // Convert to derivatives of F to cartesian basis
  AthenaSurfaceTensor<Real,TensorSymm::NONE,3,1> dr_d_surf;
  dr_d_surf.NewAthenaSurfaceTensor(nangles);

  for(int n=0; n<nangles; ++n) {
    for(int i=0; i<3;++i) {
      dr_d_surf(i,n) = surface_jacobian.h_view(n,0,i);
    }
  }

  // Flat projection operator, Gundlach 1997 eqn 25
  AthenaSurfaceTensor<Real,TensorSymm::SYM2,3,2> p2_dd_surf;
  p2_dd_surf.NewAthenaSurfaceTensor(nangles);

  double gbar_uu;
  for(int n=0; n<nangles; ++n) {
    for(int i=0; i<3;++i) {
      for(int j=0; j<3;++j) {
        if (i==j) {
          gbar_uu = 1.;
        } else {
          gbar_uu = 0.;
        }
        p2_dd_surf(i,j,n) = gbar_uu-dr_d_surf(i,n)*dr_d_surf(j,n);
      }
    }
  }

  // contraction between p1 and p2
  AthenaSurfaceTensor<Real,TensorSymm::NONE,3,0> p1p2;
  p1p2.NewAthenaSurfaceTensor(nangles);
  for(int n=0; n<nangles; ++n) {
    for(int i=0; i<3;++i) {
      for(int j=0; j<3;++j) {
        p1p2(n) += p1_uu_surf(i,j,n)*p2_dd_surf(i,j,n);
      }
    }
  }

  // Weighting function for N-flow, Gundlach 1997 eqn 28
  AthenaSurfaceTensor<Real,TensorSymm::NONE,3,0> rho;
  rho.NewAthenaSurfaceTensor(nangles);
  for(int n=0; n<nangles; ++n) {
    rho(n) = 2*pow(S->pointwise_radius.h_view(n),2)*delta_F_abs(n)/p1p2(n);
  }

  // Surface inverse metric (in cartesian coordinate), Gundlach 1997 eqn 9
  AthenaSurfaceTensor<Real,TensorSymm::SYM2,3,2> m_uu_surf;
  m_uu_surf.NewAthenaSurfaceTensor(nangles);

  for(int n=0; n<nangles; ++n) {
    for(int i=0; i<3;++i) {
      for(int j=0; j<3;++j) {
        m_uu_surf(i,j,n) = g_uu_surf(i,j,n)
                - dF_u_surf(i,n)*dF_u_surf(j,n)
                /delta_F_abs(n)/delta_F_abs(n);
      }
    }
  }

  // Surface Null Expansion, Gundlach 1997 eqn 9
  AthenaSurfaceTensor<Real,TensorSymm::NONE,3,0> H;
  H.NewAthenaSurfaceTensor(nangles);

  for(int n=0; n<nangles; ++n) {
    H(n) = 0;
    for(int i=0; i<3;++i) {
      for(int j=0; j<3;++j) {
        H(n) += m_uu_surf(i,j,n)*(ddF_dd_surf(i,j,n)
                        /delta_F_abs(n)-K_dd_surf(i,j,n))*delta_F_abs(n);//*rho(n); // last term added to give mean curvature flow
      }
    }
  }

  return H;
}

// Analytical surface null expansion for Schwarzschild in isotropic coordinate, testing only
AthenaSurfaceTensor<Real,TensorSymm::NONE,3,0> AnalyticSurfaceNullExpansion(GaussLegendreGrid *S) {
  int nangles = S->nangles;
  AthenaSurfaceTensor<Real,TensorSymm::NONE,3,0> H;
  H.NewAthenaSurfaceTensor(nangles);

  auto r = S->pointwise_radius;
  for(int n=0; n<nangles; ++n) {
    Real denominator = 2*r.h_view(n)+1;
    H(n) = 8*r.h_view(n)*(2*r.h_view(n)-1)/(denominator*denominator*denominator);
  }
  return H;
}

void ProblemGenerator::UserProblem(ParameterInput *pin, const bool restart) {
  MeshBlockPack *pmbp = pmy_mesh_->pmb_pack;
  auto &indcs = pmy_mesh_->mb_indcs;
  auto &size = pmbp->pmb->mb_size;

  if (pmbp->pz4c == nullptr) {
    std::cout << "### FATAL ERROR in " << __FILE__ << " at line " << __LINE__ << std::endl
              << "One Puncture test can only be run in Z4c, but no <z4c> block "
              << "in input file" << std::endl;
    exit(EXIT_FAILURE);
  }
  // std::cout << "here" << std::endl;

  // One Puncture nitial data 
  pmbp->pz4c->ADMOnePuncture(pmbp, pin);
  // std::cout << "here2" << std::endl;

  pmbp->pz4c->GaugePreCollapsedLapse(pmbp, pin);
  switch (indcs.ng) {
    case 2: pmbp->pz4c->ADMToZ4c<2>(pmbp, pin);
            break;
    case 3: pmbp->pz4c->ADMToZ4c<3>(pmbp, pin);
            break;
    case 4: pmbp->pz4c->ADMToZ4c<4>(pmbp, pin);
            break;
  }
  // std::cout << "here" << std::endl;

  std::cout<<"OnePuncture initialized; Starting Horizon Finder"<<std::endl;

  // load in adm variables
  auto &adm = pmbp->padm->adm;
  auto &z4c = pmbp->pz4c->z4c;
  auto &g_dd = adm.g_dd;
  auto &K_dd = adm.K_dd;
  // Evaluate partial derivatives of the metric over the entire domain
  // 6 dimensional array, nmb, 3, 6, ncells3, ncells2, ncells1
  //DualArray6D<Real> *dg_ddd = nullptr;
  DualArray6D<Real> dg_ddd;

  switch (indcs.ng) {
    case 2: dg_ddd = metric_partial<2>(pmbp);
            break;
    case 3: dg_ddd = metric_partial<3>(pmbp);
            break;
    case 4: dg_ddd = metric_partial<4>(pmbp);
            break;
  }

  // Initialize a surface with radius of 2 centered at the origin

  int nlev = 10;
  int nfilt = 16;
  bool rotate_sphere = true;
  bool fluxes = true;
  Real radius = .8;
  int maxit = 100;
  GaussLegendreGrid *S = nullptr;
  S = new GaussLegendreGrid(pmbp, nlev, radius,nfilt);
  Real ctr[3] = {0.,0.,0.};
  
  // Surface Null Expansion, Gundlach 1997 eqn 9
  AthenaSurfaceTensor<Real,TensorSymm::NONE,3,0> H = SurfaceNullExpansion(pmbp, S, dg_ddd); // AnalyticSurfaceNullExpansion(S); // 

  // Template this integration function over DualArray1D and Tensor of rank 0
  Real H_integrated = S->Integrate(H);
  std::cout << "Initial Norm of H: " << H_integrated << std::endl;
  std::cout << "Initial Radius: " << S->pointwise_radius.h_view(0) << std::endl;

  std::ofstream spherical_grid_output;
  // alpha and beta parametrization
  Real alpha = 1.;
  Real beta = 0.5;

  // H-flow Jacobi loop, take A = 1; B = 0; rho = 1
  Real A = alpha/(nfilt*(nfilt+1))+beta;
  Real B = beta/alpha;

  for (int itr=0; itr<maxit; ++itr) {
    // auto pointwise_radius = S->pointwise_radius;
    auto H_spectral = S->SpatialToSpectral(H);

    auto r_spectral = S->SpatialToSpectral(S->pointwise_radius);

    DualArray1D<Real> r_spectral_np1;
    Kokkos::realloc(r_spectral_np1,nfilt);

    for (int i=0; i<nfilt; ++i) {
      int l = (int) sqrt(i);
      // std::cout << r_spectral.h_view(i) << std::endl;
      r_spectral_np1.h_view(i) =  r_spectral.h_view(i) - A/(1+B*l*(l+1))*H_spectral.h_view(i);
    }

    auto r_np1 = S->SpectralToSpatial(r_spectral_np1);

    // reset radius
    S->SetPointwiseRadius(r_np1,ctr);

    spherical_grid_output.open ("/home/hzhu/Desktop/research/gr/athenak_versions/athenak/build/radius.out", std::ios_base::app);
    for (int i=0;i<S->nangles;++i) {
      spherical_grid_output << S->pointwise_radius.h_view(i) <<  "\n";// << ones_dphi.h_view(i) <<"\n";
    }
    spherical_grid_output.close();

    // reevaluate H
    H = SurfaceNullExpansion(pmbp, S, dg_ddd); // AnalyticSurfaceNullExpansion(S); // 

    H_integrated = S->Integrate(H);
  
    std::cout << "Itr " << itr+1 << "   Norm of H: " << std::abs(H_integrated) << "\t" << "Radius: " << S->pointwise_radius.h_view(0) << "\t" 
    << "H spectral 0th: "<< H_spectral.h_view(0) <<std::endl;
    if (std::abs(H_integrated)<=1e-5) {
      std::cout << "target residual achieved in " << itr+1 << " iterations; terminating horizon finder..." << std::endl;
      break;
    }
  }

  delete S;
  return;
}