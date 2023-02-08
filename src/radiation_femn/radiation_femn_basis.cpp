//========================================================================================
// Radiation FEM_N code for Athena
// Copyright (C) 2023 Maitraya Bhattacharyya <mbb6217@psu.edu> and David Radice <dur566@psu.edu>
// AthenaXX copyright(C) James M. Stone <jmstone@ias.edu> and the Athena code team
// Licensed under the 3-clause BSD License (the "LICENSE")
//========================================================================================
//! \file radiation_femn_basis.cpp
//  \brief implementation of the radiation FEM_N basis functions and helper functions

#include <iostream>
#include <string>

#include "athena.hpp"
#include "units/units.hpp"
#include "radiation_femn/radiation_femn.hpp"

namespace radiationfemn {

    // ---------------------------------------------------------------------------------------------
    // Convert Barycentric coordinates to Cartesian coordinates given vertices of triangle
    KOKKOS_INLINE_FUNCTION
    void BarycentricToCartesian(double x1, double y1, double z1, double x2, double y2, double z2, double x3,
                                double y3,
                                double z3, double xi1, double xi2,
                                double xi3, double &xval, double &yval, double &zval) {

        xval = xi1 * x1 + xi2 * x2 + xi3 * x3;
        yval = xi1 * y1 + xi2 * y2 + xi3 * y3;
        zval = xi1 * z1 + xi2 * z2 + xi3 * z3;

    }

    // ------------------------------------------------------------------------------------------------
    // Given index numbers of two vertices, finds if they share an edge and if so, return triangle info
    // If a = b, this return all triangles which share the vertex
    void RadiationFEMN::FindTriangles(int a, int b, bool &is_edge) {

        is_edge = false;
        Kokkos::deep_copy(edge_triangles, -42.);

        if (a == b) {
            size_t index{0};
            for (size_t i = 0; i < num_triangles; i++) {
                {
                    if (triangles(i, 0) == a || triangles(i, 1) == a || triangles(i, 2) == a) {
                        is_edge = true;
                        edge_triangles(index, 0) = triangles(i, 0);
                        edge_triangles(index, 1) = triangles(i, 1);
                        edge_triangles(index, 2) = triangles(i, 2);
                        index++;
                    }
                }
            }
        }

        else if (a != b) {
            size_t index{0};
            for (size_t i = 0; i < num_triangles; i++) {
                if ((triangles(i, 0) == a && triangles(i, 1) == b) || (triangles(i, 0) == a && triangles(i, 2) == b) ||
                    (triangles(i, 0) == b && triangles(i, 1) == a) || (triangles(i, 0) == b && triangles(i, 2) == a) ||
                    (triangles(i, 1) == a && triangles(i, 2) == b) || (triangles(i, 1) == b && triangles(i, 2) == a)) {
                        is_edge = true;
                        edge_triangles(index,0) = triangles(i,0);
                        edge_triangles(index,1) = triangles(i,1);
                        edge_triangles(index,2) = triangles(i,2);
                        index++;
                }
            }
        }

    }

    // --------------------------------------------------------------------
    // Basis 1: 'overlapping tent
    KOKKOS_INLINE_FUNCTION
    double RadiationFEMN::FEMBasis1Type1(double xi1, double xi2, double xi3) {
        return 2. * xi1 + xi2 + xi3 - 1.;
    }

    KOKKOS_INLINE_FUNCTION
    double RadiationFEMN::FEMBasis2Type1(double xi1, double xi2, double xi3) {
        return xi1 + 2. * xi2 + xi3 - 1.;
    }

    KOKKOS_INLINE_FUNCTION
    double RadiationFEMN::FEMBasis3Type1(double xi1, double xi2, double xi3) {
        return xi1 + xi2 + 2. * xi3 - 1.;
    }

    // -------------------------------------------------------------------
    // Basis 2: 'small tent'
    KOKKOS_INLINE_FUNCTION
    double RadiationFEMN::FEMBasis1Type2(double xi1, double xi2, double xi3) {
        return (xi1 >= 0.5) * (xi1 - xi2 - xi3);
    }

    KOKKOS_INLINE_FUNCTION
    double RadiationFEMN::FEMBasis2Type2(double xi1, double xi2, double xi3) {
        return (xi2 >= 0.5) * (xi2 - xi3 - xi1);
    }

    KOKKOS_INLINE_FUNCTION
    double RadiationFEMN::FEMBasis3Type2(double xi1, double xi2, double xi3) {
        return (xi3 >= 0.5) * (xi3 - xi1 - xi2);
    }

    // --------------------------------------------------------------------
    // Basis 3: 'overlapping honeycomb'
    KOKKOS_INLINE_FUNCTION
    double RadiationFEMN::FEMBasis1Type3(double xi1, double xi2, double xi3) {
        return 1.;
    }

    KOKKOS_INLINE_FUNCTION
    double RadiationFEMN::FEMBasis2Type3(double xi1, double xi2, double xi3) {
        return 1.;
    }

    KOKKOS_INLINE_FUNCTION
    double RadiationFEMN::FEMBasis3Type3(double xi1, double xi2, double xi3) {
        return 1.;
    }

    // -------------------------------------------------------------------
    // Basis 4: 'non-overlapping honeycomb'
    KOKKOS_INLINE_FUNCTION
    double RadiationFEMN::FEMBasis1Type4(double xi1, double xi2, double xi3) {
        return (xi1 >= xi2) * (xi1 > xi3) * 1.;
    }

    KOKKOS_INLINE_FUNCTION
    double RadiationFEMN::FEMBasis2Type4(double xi1, double xi2, double xi3) {
        return (xi2 >= xi3) * (xi2 > xi1) * 1.;
    }

    KOKKOS_INLINE_FUNCTION
    double RadiationFEMN::FEMBasis3Type4(double xi1, double xi2, double xi3) {
        return (xi3 >= xi1) * (xi3 > xi2) * 1.;
    }

    // ---------------------------------------------------------------------
    // FEM basis in barycentric coordinates
    KOKKOS_INLINE_FUNCTION
    double RadiationFEMN::FEMBasis(double xi1, double xi2, double xi3, int basis_index, int basis_choice) {
        if (basis_index == 1 && basis_choice == 1) {
            return FEMBasis1Type1(xi1, xi2, xi3);
        } else if (basis_index == 1 && basis_choice == 2) {
            return FEMBasis1Type2(xi1, xi2, xi3);
        } else if (basis_index == 1 && basis_choice == 3) {
            return FEMBasis1Type3(xi1, xi2, xi3);
        } else if (basis_index == 1 && basis_choice == 4) {
            return FEMBasis1Type4(xi1, xi2, xi3);
        } else if (basis_index == 2 && basis_choice == 1) {
            return FEMBasis2Type1(xi1, xi2, xi3);
        } else if (basis_index == 2 && basis_choice == 2) {
            return FEMBasis2Type2(xi1, xi2, xi3);
        } else if (basis_index == 2 && basis_choice == 3) {
            return FEMBasis2Type3(xi1, xi2, xi3);
        } else if (basis_index == 2 && basis_choice == 4) {
            return FEMBasis2Type4(xi1, xi2, xi3);
        } else if (basis_index == 3 && basis_choice == 1) {
            return FEMBasis3Type1(xi1, xi2, xi3);
        } else if (basis_index == 3 && basis_choice == 2) {
            return FEMBasis3Type2(xi1, xi2, xi3);
        } else if (basis_index == 3 && basis_choice == 3) {
            return FEMBasis3Type3(xi1, xi2, xi3);
        } else if (basis_index == 3 && basis_choice == 4) {
            return FEMBasis3Type4(xi1, xi2, xi3);
        } else {
            std::cout << "Incorrect basis_choice of basis function in radiation-femn block!" << std::endl;
            exit(EXIT_FAILURE);
        }
    }

    // -----------------------------------------------------------------------
    // Product of two FEM basis given their index and triangle info
    //KOKKOS_INLINE_FUNCTION
    double RadiationFEMN::FEMBasisABasisB(int a, int b, int t1, int t2, int t3, double xi1, double xi2, double xi3, int basis_choice) {

        int basis_index_a = (a == t1) * 1 + (a == t2) * 2 + (a == t3) * 3;
        int basis_index_b = (b == t1) * 1 + (b == t2) * 2 + (b == t3) * 3;

        auto FEMBasisA = FEMBasis(xi1, xi2, xi3, basis_index_a, basis_choice);
        auto FEMBasisB = FEMBasis(xi1, xi2, xi3, basis_index_b, basis_choice);

        return FEMBasisA * FEMBasisB;
    }

    // -------------------------------------------------------------------------
    // FEM basis given its index and triangle information
    KOKKOS_INLINE_FUNCTION
    double RadiationFEMN::FEMBasisA(int a, int t1, int t2, int t3, double xi1, double xi2, double xi3, int basis_choice) {

        int basis_index_a = (a == t1) * 1 + (a == t2) * 2 + (a == t3) * 3;

        auto result = FEMBasis(xi1, xi2, xi3, basis_index_a, basis_choice);

        return result;
    }

    KOKKOS_INLINE_FUNCTION
    double
    CosPhiSinTheta(double x1, double y1, double z1, double x2, double y2, double z2, double x3, double y3, double z3,
                   double xi1, double xi2, double xi3) {
        double xval, yval, zval;
        BarycentricToCartesian(x1, y1, z1, x2, y2, z2, x3, y3, z3, xi1, xi2, xi3, xval, yval, zval);

        double rval = sqrt(xval * xval + yval * yval + zval * zval);
        double thetaval = acos(zval / rval);
        double phival = atan2(yval, xval);

        return cos(phival) * sin(thetaval);
    }

    KOKKOS_INLINE_FUNCTION
    double
    SinPhiSinTheta(double x1, double y1, double z1, double x2, double y2, double z2, double x3, double y3, double z3,
                   double xi1, double xi2, double xi3) {
        double xval, yval, zval;
        BarycentricToCartesian(x1, y1, z1, x2, y2, z2, x3, y3, z3, xi1, xi2, xi3, xval, yval, zval);

        double rval = sqrt(xval * xval + yval * yval + zval * zval);
        double thetaval = acos(zval / rval);
        double phival = atan2(yval, xval);

        return sin(phival) * sin(thetaval);
    }

    KOKKOS_INLINE_FUNCTION
    double CosTheta(double x1, double y1, double z1, double x2, double y2, double z2, double x3, double y3, double z3,
                    double xi1, double xi2, double xi3) {
        double xval, yval, zval;
        BarycentricToCartesian(x1, y1, z1, x2, y2, z2, x3, y3, z3, xi1, xi2, xi3, xval, yval, zval);

        double rval = sqrt(xval * xval + yval * yval + zval * zval);
        double thetaval = acos(zval / rval);

        return cos(thetaval);
    }

    KOKKOS_INLINE_FUNCTION
    double SinTheta(double x1, double y1, double z1, double x2, double y2, double z2, double x3, double y3, double z3,
                    double xi1, double xi2, double xi3) {
        double xval, yval, zval;
        BarycentricToCartesian(x1, y1, z1, x2, y2, z2, x3, y3, z3, xi1, xi2, xi3, xval, yval, zval);

        double rval = sqrt(xval * xval + yval * yval + zval * zval);
        double thetaval = acos(zval / rval);

        return sin(thetaval);
    }
}