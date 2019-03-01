
#include "../netcdf_io.hpp"

#ifndef DEBUG
    #define DEBUG 0
#endif


// Write to netcdf file
void read_source(
        int & Nlon,          /**< [in] Size of longitude dimension*/
        int & Nlat,          /**< [in] Size of latitude dimension*/
        int & Ntime,         /**< [in] Size of time dimension*/
        int & Ndepth,        /**< [in] Size of depth dimension*/
        double ** longitude, /**< [in] Pointer to longitude array to be created*/
        double ** latitude,  /**< [in] Pointer to latitude array to be created*/
        double ** time,      /**< [in] Pointer to time array to be created*/
        double ** depth,     /**< [in] Pointer to depth array to be created*/
        double ** u_r,       /**< [in] Pointer to u_r array to be created*/
        double ** u_lon,     /**< [in] Pointer to u_lon array to be created*/
        double ** u_lat,     /**< [in] Pointer to u_lat array to be created*/
        double ** mask       /**< [in] Pointer to mask array to be created*/
        ) {

    // Open the NETCDF file
    int FLAG = NC_NETCDF4 | NC_NOWRITE;
    int ncid=0, retval;
    char buffer [50];
    snprintf(buffer, 50, "input.nc");
    if (( retval = nc_open(buffer, FLAG, &ncid) )) { NC_ERR(retval, __LINE__, __FILE__); }
    
    // Define the dimensions
    int lon_dimid, lat_dimid, time_dimid, depth_dimid;
    if ((retval = nc_inq_dimid(ncid, "time",      &time_dimid  ))) { NC_ERR(retval, __LINE__, __FILE__); }
    if ((retval = nc_inq_dimid(ncid, "depth",     &depth_dimid ))) { NC_ERR(retval, __LINE__, __FILE__); }
    if ((retval = nc_inq_dimid(ncid, "latitude",  &lat_dimid   ))) { NC_ERR(retval, __LINE__, __FILE__); }
    if ((retval = nc_inq_dimid(ncid, "longitude", &lon_dimid   ))) { NC_ERR(retval, __LINE__, __FILE__); }

    size_t Ntime_st, Ndepth_st, Nlon_st, Nlat_st;
    if ((retval = nc_inq_dim(ncid, time_dimid , NULL, &Ntime_st  ))) { NC_ERR(retval, __LINE__, __FILE__); }
    if ((retval = nc_inq_dim(ncid, depth_dimid, NULL, &Ndepth_st ))) { NC_ERR(retval, __LINE__, __FILE__); }
    if ((retval = nc_inq_dim(ncid, lat_dimid  , NULL, &Nlat_st   ))) { NC_ERR(retval, __LINE__, __FILE__); }
    if ((retval = nc_inq_dim(ncid, lon_dimid  , NULL, &Nlon_st   ))) { NC_ERR(retval, __LINE__, __FILE__); }


    // Cast the sizes to integers (to resolve some compile errors)
    //   Unless we're dealing with truly massive grids, this
    //   shouldn't be an issue.
    Ntime  = static_cast<int>(Ntime_st);
    Ndepth = static_cast<int>(Ndepth_st);
    Nlon   = static_cast<int>(Nlon_st);
    Nlat   = static_cast<int>(Nlat_st);

    #if DEBUG >= 1
    fprintf(stdout, "\n");
    fprintf(stdout, "Nlon   = %d\n", Nlon);
    fprintf(stdout, "Nlat   = %d\n", Nlat);
    fprintf(stdout, "Ntime  = %d\n", Ntime);
    fprintf(stdout, "Ndepth = %d\n", Ndepth);
    fprintf(stdout, "\n");
    #endif

    // For the moment, as a precaution stop if we hit something too large.
    if ( (Nlon > 1e4) or (Nlat > 1e4) or (Ntime > 1e2) or (Ndepth > 1e2) ) {
        if ((retval = nc_close(ncid))) { NC_ERR(retval, __LINE__, __FILE__); }
        fprintf(stdout, "Data dimensions too large to continue. (Line %d of %s)\n", __LINE__, __FILE__);
        return;
    }

    //
    //// Allocate memory for the fields
    //

    time[0]      = new double[Ntime];
    depth[0]     = new double[Ndepth];
    longitude[0] = new double[Nlon];
    latitude[0]  = new double[Nlat];

    u_r[0]   = new double[Ntime * Ndepth * Nlat * Nlon];
    u_lon[0] = new double[Ntime * Ndepth * Nlat * Nlon];
    u_lat[0] = new double[Ntime * Ndepth * Nlat * Nlon];

    mask[0]  = new double[                 Nlat * Nlon];

    //
    //// Get fields from IC file
    //

    // Define coordinate variables
    int lon_varid, lat_varid, time_varid, depth_varid;
    if ((retval = nc_inq_varid(ncid, "time",      &time_varid  ))) { NC_ERR(retval, __LINE__, __FILE__); }
    if ((retval = nc_inq_varid(ncid, "depth",     &depth_varid ))) { NC_ERR(retval, __LINE__, __FILE__); }
    if ((retval = nc_inq_varid(ncid, "longitude", &lon_varid   ))) { NC_ERR(retval, __LINE__, __FILE__); }
    if ((retval = nc_inq_varid(ncid, "latitude",  &lat_varid   ))) { NC_ERR(retval, __LINE__, __FILE__); }

    // Declare variables
    int ulon_varid, ulat_varid;
    if ((retval = nc_inq_varid(ncid, "uo", &ulon_varid))) { NC_ERR(retval, __LINE__, __FILE__); }
    if ((retval = nc_inq_varid(ncid, "vo", &ulat_varid))) { NC_ERR(retval, __LINE__, __FILE__); }

    // Get the scale factors for the velocities
    double u_lon_scale, u_lat_scale;
    if ((retval = nc_get_att_double(ncid, ulon_varid, "scale_factor", &u_lon_scale))) { NC_ERR(retval, __LINE__, __FILE__); }
    if ((retval = nc_get_att_double(ncid, ulat_varid, "scale_factor", &u_lat_scale))) { NC_ERR(retval, __LINE__, __FILE__); }

    double u_lon_fill, u_lat_fill;
    if ((retval = nc_get_att_double(ncid, ulon_varid, "_FillValue", &u_lon_fill))) { NC_ERR(retval, __LINE__, __FILE__); }
    if ((retval = nc_get_att_double(ncid, ulat_varid, "_FillValue", &u_lat_fill))) { NC_ERR(retval, __LINE__, __FILE__); }

    // Get the coordinate variables
    size_t start_coord[1], count_coord[1];

    start_coord[0] = 0;
    count_coord[0] = Nlon;
    if ((retval = nc_get_vara_double(ncid, lon_varid, start_coord, count_coord, longitude[0]))) { NC_ERR(retval, __LINE__, __FILE__); }

    start_coord[0] = 0;
    count_coord[0] = Nlat;
    if ((retval = nc_get_vara_double(ncid, lat_varid, start_coord, count_coord, latitude[0] ))) { NC_ERR(retval, __LINE__, __FILE__); }

    start_coord[0] = 0;
    count_coord[0] = Ntime;
    if ((retval = nc_get_vara_double(ncid, lat_varid, start_coord, count_coord, time[0] ))) { NC_ERR(retval, __LINE__, __FILE__); }

    start_coord[0] = 0;
    count_coord[0] = Ndepth;
    if ((retval = nc_get_vara_double(ncid, lat_varid, start_coord, count_coord, depth[0] ))) { NC_ERR(retval, __LINE__, __FILE__); }

    // Get u_lon (uo) and u_lat (vo)
    size_t start[4], count[4];
    start[0] = 0;
    start[1] = 0;
    start[2] = 0;
    start[3] = 0;
    count[0] = Ntime;
    count[1] = Ndepth;
    count[2] = Nlat;
    count[3] = Nlon;

    if ((retval = nc_get_vara_double(ncid, ulon_varid, start, count, u_lon[0]))) { NC_ERR(retval, __LINE__, __FILE__); }
    if ((retval = nc_get_vara_double(ncid, ulat_varid, start, count, u_lat[0]))) { NC_ERR(retval, __LINE__, __FILE__); }

    // At the moment there's no u_r in the data, so just zero it out
    //     also apply the scale factors to the velocity fields
    for (int index = 0; index < Ntime * Ndepth * Nlat * Nlon; index++) {
        u_r[0][index] = 0;
        u_lon[0][index] *= u_lon_scale;
        u_lat[0][index] *= u_lat_scale;
    }

    // Determine the mask (i.e. where land is) by checking
    //   where abs(velocity) > 90% of the fill value
    //   mask = 0 implies LAND
    //   mask = 1 implies WATER

    int num_land = 0;
    int num_water = 0;

    for (int index = 0; index < Nlat * Nlon; index++) {
        if (    (abs(u_r[  0][index]) > 0.9 * abs(u_lon_fill              )) 
             or (abs(u_lon[0][index]) > 0.9 * abs(u_lon_fill * u_lon_scale))
             or (abs(u_lat[0][index]) > 0.9 * abs(u_lat_fill * u_lat_scale)) ) {
            mask[0][index] = 0;
            num_land++;
        } else {
            mask[0][index] = 1;
            num_water++;
        }
    }

    #if DEBUG >= 1
    fprintf(stdout, "Number of land  cells: %d (%.2g %%)\n", num_land, 100 * ( (double) num_land ) / (num_water + num_land));
    fprintf(stdout, "Number of water cells: %d\n", num_water);
    fprintf(stdout, "\n");
    #endif

    // Close the file
    if ((retval = nc_close(ncid))) { NC_ERR(retval, __LINE__, __FILE__); }

}
