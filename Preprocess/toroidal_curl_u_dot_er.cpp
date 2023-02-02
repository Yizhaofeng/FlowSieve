#include "../constants.hpp"
#include "../functions.hpp"
#include "../differentiation_tools.hpp"
#include "../preprocess.hpp"
#include <algorithm>
#include <vector>
#include <omp.h>
#include <math.h>

void toroidal_curl_u_dot_er(
        std::vector<double> & out_arr,
        const std::vector<double> & u_lon,
        const std::vector<double> & u_lat,
        const std::vector<double> & longitude,
        const std::vector<double> & latitude,
        const int Itime,
        const int Idepth,
        const int Ntime,
        const int Ndepth,
        const int Nlat,
        const int Nlon,
        const std::vector<bool>   & mask,
        const std::vector<double> * seed
        ) {

    // ret = ddlon(vel_lat) / cos_lat - ddlat( u_lon * cos_lat ) / cos_lat 
    //     = ddlon(vel_lat) / cos_lat - ddlat( u_lon ) + u_lon * tan_lat

    size_t index, index_sub;
    int Ilat, Ilon;
    double dulat_dlon, dulon_dlat, tmp;
    std::vector<double*> lon_deriv_vals, lat_deriv_vals;
    std::vector<const std::vector<double>*> deriv_fields;
    bool is_pole;

    deriv_fields.push_back(&u_lon);
    deriv_fields.push_back(&u_lat);
    
    #pragma omp parallel \
    default(none) \
    shared( out_arr, latitude, longitude, mask,\
            u_lon, u_lat, deriv_fields, seed )\
    private(Ilat, Ilon, index, index_sub, tmp, is_pole, \
            dulat_dlon, dulon_dlat, lon_deriv_vals, lat_deriv_vals ) \
    firstprivate( Nlon, Nlat, Ndepth, Ntime, Idepth, Itime )
    {

        lon_deriv_vals.push_back(NULL);
        lon_deriv_vals.push_back(&dulat_dlon);

        lat_deriv_vals.push_back(&dulon_dlat);
        lat_deriv_vals.push_back(NULL);

        #pragma omp for collapse(2) schedule(guided)
        for (Ilat = 0; Ilat < Nlat; ++Ilat) {
            for (Ilon = 0; Ilon < Nlon; ++Ilon) {

                index = Index(Itime, Idepth, Ilat, Ilon, Ntime, Ndepth, Nlat, Nlon);
                index_sub = Index(0, 0, Ilat, Ilon, 1, 1, Nlat, Nlon);
                tmp = 0.;

                if (mask.at(index)) { // Skip land areas

                    spher_derivative_at_point(
                            lon_deriv_vals, deriv_fields, longitude, "lon",
                            Itime, Idepth, Ilat, Ilon, Ntime, Ndepth, Nlat, Nlon,
                            mask);

                    spher_derivative_at_point(
                            lat_deriv_vals, deriv_fields, latitude, "lat",
                            Itime, Idepth, Ilat, Ilon, Ntime, Ndepth, Nlat, Nlon,
                            mask);

                    // If we're too close to the pole (less than 0.01 degrees), bad things happen
                    is_pole = std::fabs( std::fabs( latitude.at(Ilat) * 180.0 / M_PI ) - 90 ) < 0.01;

                    if (is_pole) {
                        tmp = 0.;
                    } else {
                        //  = ddlon(vel_lat) / cos_lat - ddlat( u_lon ) + u_lon * tan_lat
                        tmp =   dulat_dlon / cos(latitude.at(Ilat))
                              - dulon_dlat 
                              + u_lon.at(index) * tan(latitude.at(Ilat));
                        tmp *= 1. / constants::R_earth;
                    }

                    // If we have a seed, then subtract it off now
                    if ( not(seed == NULL) ) { tmp = tmp - seed->at(index_sub); }

                }

                out_arr.at(index_sub) = tmp;

            } // end Ilon
        } // end Ilat
    } // end pragma
}
