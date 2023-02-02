#include <vector>
#include <stdio.h>
#include <iostream>
#include <omp.h>
#include "../functions.hpp"
#include "../constants.hpp"
#include "../differentiation_tools.hpp"

/*!
 * \brief Compute the enstrophy transfer through the current filter scale
 *
 * In particular, computes \f$ \rho_0 * ( \omega \tau_{j,j} - (\omega \tau_{j})_{,j}  ) \f$
 * where \f$ \tau_{j} = \overline{\omega u_j} - \overline{\omega}\, \overline{u_j} \f$
 * 
 * This computation is applied to the Cartesian velocity components
 *
 * @param[in,out]   enstrophy_transfer              where to store the computed values (array)
 * @param[in]       source_data                     dataset class instance containing data (Psi, Phi, etc)
 * @param[in]       ux,uy,uz                        coarse Cartesian velocity components
 * @param[in]       coarse_vort_r                   coarse radial vorticity
 * @param[in]       vort_ux,vort_uy,vort_uz         coarse vort-velocity products (e.g. bar(omega * u_x) )  
 * @param[in]       comm                            MPI communicator object
 *
 */
void compute_Z(
        std::vector<double> & enstrophy_transfer,
        const dataset & source_data,
        const std::vector<double> & ux,
        const std::vector<double> & uy,
        const std::vector<double> & uz,
        const std::vector<double> & coarse_vort_r,
        const std::vector<double> & vort_ux,
        const std::vector<double> & vort_uy,
        const std::vector<double> & vort_uz,
        const MPI_Comm comm
        ) {

    const std::vector<bool> &mask = source_data.mask;

    #if DEBUG >= 2
    int wRank, wSize;
    MPI_Comm_rank( comm, &wRank );
    MPI_Comm_size( comm, &wSize );

    if (wRank == 0) { fprintf(stdout, "  Starting Z computation.\n"); }
    #endif

    double Z_tmp;
    int Itime, Idepth, Ilat, Ilon, jj;
    size_t index;
    const size_t Npts = enstrophy_transfer.size();

    double tau_ij_j, u_i_tau_ij_j;
    std::vector<double> tau_ij;
    std::vector<double> u_i_tau_ij;
    tau_ij.resize(ux.size());
    u_i_tau_ij.resize(ux.size());

    // Some convenience handles
    //   note: the pointers aren't constant, but the things
    //         to which they are pointing are
    double omega_loc, uj_loc, omega_uj_loc;
    const std::vector<double> *omega_uj, *omega, *uj;

    // Set up the derivatives to pass through the differentiation functions
    std::vector<double*> x_deriv_vals, y_deriv_vals, z_deriv_vals;
    std::vector<const std::vector<double>*> deriv_fields;

    deriv_fields.push_back(&tau_ij);
    deriv_fields.push_back(&u_i_tau_ij);

    // Zero out enstrophy transfer before we start
    std::fill( enstrophy_transfer.begin(), enstrophy_transfer.end(), 0. );

    // omega
    omega = &coarse_vort_r;

    for (jj = 0; jj < 3; jj++) {

        //   Assign the handy pointers: omega_uj, uj
        //
        //   0 -> x
        //   1 -> y
        //   2 -> z

        // uj
        uj = ( jj == 0 ) ? &ux : ( jj == 1 ) ? &uy : &uz;

        // omega_uj 
        omega_uj = ( jj == 0 ) ? &vort_ux : ( jj == 1 ) ? &vort_uy : &vort_uz;
        
            
        // First, compute the appropriate
        //   tau_ij and u_i * tau_ij
        #pragma omp parallel \
        default(none) \
        shared( source_data, tau_ij, u_i_tau_ij, mask, omega, uj, omega_uj)\
        private(index, omega_uj_loc, omega_loc, uj_loc) \
        firstprivate( Npts )
        {
            #pragma omp for collapse(1) schedule(guided)
            for (index = 0; index < Npts; index++) {

                if ( mask.at(index) ) {
                    omega_loc    = omega->at( index );
                    uj_loc       = uj->at( index );
                    omega_uj_loc = omega_uj->at( index );

                    tau_ij.at(index) = omega_uj_loc - omega_loc * uj_loc;
                    u_i_tau_ij.at(index) = omega_loc * tau_ij.at(index);

                }
            }
        }

        #pragma omp parallel \
        default(none) \
        shared( source_data, enstrophy_transfer, mask, \
                jj, omega, tau_ij, u_i_tau_ij, deriv_fields,std::cout)\
        private(Itime, Idepth, Ilat, Ilon, index, \
                Z_tmp, tau_ij_j, u_i_tau_ij_j,\
                x_deriv_vals, y_deriv_vals, z_deriv_vals) \
        firstprivate( Npts )
        {

            x_deriv_vals.resize(2);
            y_deriv_vals.resize(2);
            z_deriv_vals.resize(2);

            // Now set the appropriate derivative pointers in order to compute
            //     tau_ij,j
            //     (u_i * tau_ij)_,j
            x_deriv_vals.at(0) = (jj == 0) ? &tau_ij_j     : NULL;
            x_deriv_vals.at(1) = (jj == 0) ? &u_i_tau_ij_j : NULL;

            y_deriv_vals.at(0) = (jj == 1) ? &tau_ij_j     : NULL;
            y_deriv_vals.at(1) = (jj == 1) ? &u_i_tau_ij_j : NULL;

            z_deriv_vals.at(0) = (jj == 2) ? &tau_ij_j     : NULL;
            z_deriv_vals.at(1) = (jj == 2) ? &u_i_tau_ij_j : NULL;

            // Now actually compute Z -  in particular, compute
            //           u_i * tau_ij,j - (u_i * tau_ij)_,j               
            #pragma omp for collapse(1) schedule(guided)
            for (index = 0; index < Npts; index++) {

                if ( mask.at(index) ) {

                    source_data.index1to4_local( index, Itime, Idepth, Ilat, Ilon);

                    // Compute the desired derivatives
                    Cart_derivatives_at_point(
                            x_deriv_vals, y_deriv_vals, z_deriv_vals, deriv_fields,
                            source_data, Itime, Idepth, Ilat, Ilon,
                            1, constants::DiffOrd);

                    // u_i * tau_ij,j - (u_i * tau_ij)_,j
                    Z_tmp = omega->at(index) * tau_ij_j  -  u_i_tau_ij_j;
                    enstrophy_transfer.at(index) += constants::rho0 * Z_tmp;
                        
                }
            }
        }
    }
    #if DEBUG >= 2
    if (wRank == 0) { fprintf(stdout, "     ... done.\n"); }
    #endif
}
