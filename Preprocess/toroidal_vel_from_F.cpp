#include "../constants.hpp"
#include "../functions.hpp"
#include "../differentiation_tools.hpp"
#include <algorithm>
#include <vector>
#include <omp.h>
#include <math.h>

void toroidal_vel_from_F(  
        std::vector<double> & vel_lon,
        std::vector<double> & vel_lat,
        const std::vector<double> & F,
        const std::vector<double> & longitude,
        const std::vector<double> & latitude,
        const int Ntime,
        const int Ndepth,
        const int Nlat,
        const int Nlon,
        const std::vector<bool> & mask
    ) {

    int Itime, Idepth, Ilat, Ilon, index;
    double dFdlon, dFdlat, cos_lat, tmp_lon, tmp_lat;
    std::vector<double*> lon_deriv_vals, lat_deriv_vals;
    std::vector<const std::vector<double>*> deriv_fields;
    bool is_pole;

    deriv_fields.push_back(&F);

    #pragma omp parallel \
    default(none) \
    shared( latitude, longitude, mask, F, vel_lon, vel_lat, deriv_fields)\
    private(Itime, Idepth, Ilat, Ilon, index, cos_lat, tmp_lon, tmp_lat, \
            dFdlon, dFdlat, lon_deriv_vals, lat_deriv_vals, is_pole) \
    firstprivate( Nlon, Nlat, Ndepth, Ntime )
    {

        lon_deriv_vals.push_back(&dFdlon);
        lat_deriv_vals.push_back(&dFdlat);

        #pragma omp for collapse(1) schedule(guided)
        for (index = 0; index < (int)F.size(); ++index) {

            tmp_lon = 0.;
            tmp_lat = 0.;

            if (mask.at(index)) { // Skip land areas

                Index1to4(index, Itime, Idepth, Ilat, Ilon,
                                 Ntime, Ndepth, Nlat, Nlon);

                spher_derivative_at_point(
                        lon_deriv_vals, deriv_fields,
                        longitude, "lon",
                        Itime, Idepth, Ilat, Ilon,
                        Ntime, Ndepth, Nlat, Nlon,
                        mask);

                spher_derivative_at_point(
                        lat_deriv_vals, deriv_fields,
                        latitude, "lat",
                        Itime, Idepth, Ilat, Ilon,
                        Ntime, Ndepth, Nlat, Nlon,
                        mask);

                if (constants::CARTESIAN) {
                    tmp_lon = - dFdlat;
                    tmp_lat =   dFdlon;
                } else {
                    // If we're too close to the pole (less than 0.01 degrees), bad things happen
                    is_pole = std::fabs( std::fabs( latitude.at(Ilat) * 180.0 / M_PI ) - 90 ) < 0.01;
                    cos_lat = cos(latitude.at(Ilat));

                    tmp_lon = is_pole ? 0. : - dFdlat /  constants::R_earth;
                    tmp_lat = is_pole ? 0. :   dFdlon / (constants::R_earth * cos_lat);
                }

            }
            vel_lon.at(index) = tmp_lon;
            vel_lat.at(index) = tmp_lat;
        }
    }
}
