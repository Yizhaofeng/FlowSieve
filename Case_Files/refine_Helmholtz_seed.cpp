#include <fenv.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <algorithm>
#include <math.h>
#include <vector>
#include <mpi.h>
#include <omp.h>
#include <cassert>

#include "../netcdf_io.hpp"
#include "../functions.hpp"
#include "../constants.hpp"
#include "../preprocess.hpp"

int main(int argc, char *argv[]) {
    
    // PERIODIC_Y implies UNIFORM_LAT_GRID
    static_assert( (constants::UNIFORM_LAT_GRID) or (not(constants::PERIODIC_Y)),
            "PERIODIC_Y requires UNIFORM_LAT_GRID.\n"
            "Please update constants.hpp accordingly.\n");
    static_assert( not(constants::CARTESIAN),
            "Toroidal projection now set to handle Cartesian coordinates.\n"
            );

    // Specify the number of OpenMP threads
    //   and initialize the MPI world
    int thread_safety_provided;
    MPI_Init_thread(&argc, &argv, MPI_THREAD_MULTIPLE, &thread_safety_provided);
    //MPI_Comm_set_errhandler(MPI_COMM_WORLD, MPI::ERRORS_THROW_EXCEPTIONS);

    int wRank=-1, wSize=-1;
    MPI_Comm_rank( MPI_COMM_WORLD, &wRank );
    MPI_Comm_size( MPI_COMM_WORLD, &wSize );

    //
    //// Parse command-line arguments
    //
    InputParser input(argc, argv);
    if(input.cmdOptionExists("--version")){
        if (wRank == 0) { print_compile_info(NULL); } 
        return 0;
    }

    // first argument is the flag, second argument is default value (for when flag is not present)
    const std::string   &coarse_fname   = input.getCmdOption("--coarse_file",   "coarse.nc"),
                        &fine_fname     = input.getCmdOption("--fine_file",     "fine.nc"),
                        &output_fname   = input.getCmdOption("--output_file",   "coarse_vel.nc");

    const std::string   &time_dim_name      = input.getCmdOption("--time",        "time"),
                        &depth_dim_name     = input.getCmdOption("--depth",       "depth"),
                        &latitude_dim_name  = input.getCmdOption("--latitude",    "latitude"),
                        &longitude_dim_name = input.getCmdOption("--longitude",   "longitude");

    const std::string &latlon_in_degrees  = input.getCmdOption("--is_degrees",   "true");

    const std::string   &Nprocs_in_time_string  = input.getCmdOption("--Nprocs_in_time",  "1"),
                        &Nprocs_in_depth_string = input.getCmdOption("--Nprocs_in_depth", "1");
    const int   Nprocs_in_time_input  = stoi(Nprocs_in_time_string),
                Nprocs_in_depth_input = stoi(Nprocs_in_depth_string);

    const std::string  &var_name_coarse = input.getCmdOption("--var_in_coarse", "F");
    const std::string  &var_name_output = input.getCmdOption("--var_in_output", "seed");

    // Print processor assignments
    const int max_threads = omp_get_max_threads();
    omp_set_num_threads( max_threads );

    // Print some header info, depending on debug level
    print_header_info();

    // Initialize dataset class instance
    dataset coarse_data, fine_data;

    // Read in source data / get size information
    #if DEBUG >= 1
    if (wRank == 0) { fprintf(stdout, "Reading in source data.\n\n"); }
    #endif

    // Read in the grid coordinates
    coarse_data.load_time(      time_dim_name,      coarse_fname );
    coarse_data.load_depth(     depth_dim_name,     coarse_fname );
    coarse_data.load_latitude(  latitude_dim_name,  coarse_fname );
    coarse_data.load_longitude( longitude_dim_name, coarse_fname );

    fine_data.load_time(      time_dim_name,      fine_fname );
    fine_data.load_depth(     depth_dim_name,     fine_fname );
    fine_data.load_latitude(  latitude_dim_name,  fine_fname );
    fine_data.load_longitude( longitude_dim_name, fine_fname );

    // Apply some cleaning to the processor allotments if necessary. 
    coarse_data.check_processor_divisions( Nprocs_in_time_input, Nprocs_in_depth_input );
     
    // Convert to radians, if appropriate
    if ( latlon_in_degrees == "true" ) {
        convert_coordinates( coarse_data.longitude, coarse_data.latitude );
        convert_coordinates( fine_data.longitude,   fine_data.latitude );
    }

    // Read in velocity fields
    coarse_data.load_variable( var_name_coarse, var_name_coarse, coarse_fname, true, true );

    const int   full_Ntime  = coarse_data.full_Ntime,
                Ntime       = coarse_data.myCounts[0],
                Ndepth      = coarse_data.myCounts[1],
                Nlat_coarse = coarse_data.Nlat,
                Nlon_coarse = coarse_data.Nlon,
                Nlat_fine   = fine_data.Nlat,
                Nlon_fine   = fine_data.Nlon;

    #if DEBUG >= 1
    if (wRank == 0) {
        fprintf( stdout, " c(%d,%d,%d,%d) -> f(%d,%d,%d,%d)\n", Ntime, Ndepth, Nlat_coarse, Nlon_coarse,
                                                                Ntime, Ndepth, Nlat_fine,   Nlon_fine );
    }
    #endif

    // Now coarsen the velocity fields
    const size_t    Npts_fine = Ntime * Ndepth * Nlat_fine * Nlon_fine,
                    Npts_coarse = coarse_data.variables.at(var_name_coarse).size();
    std::vector<double> var_fine(Npts_fine);

    // Next, the coarse velocities
    int Itime, Idepth, Ilat_fine, Ilon_fine, Ilat_coarse, Ilon_coarse, lat_lb, lon_lb;
    double target_lat, target_lon;
    size_t II_fine, II_coarse;
    #pragma omp parallel \
    default(none) \
    shared( coarse_data, fine_data, var_fine, var_name_coarse, stdout ) \
    private( lat_lb, lon_lb, target_lat, target_lon, \
             Itime, Idepth, II_fine, Ilat_fine, Ilon_fine, \
             II_coarse, Ilat_coarse, Ilon_coarse )
    {
        #pragma omp for collapse(1) schedule(static)
        for (II_fine = 0; II_fine < Npts_fine; ++II_fine) {

            Index1to4( II_fine, Itime, Idepth, Ilat_fine, Ilon_fine,
                                Ntime, Ndepth, Nlat_fine, Nlon_fine );

            //fprintf( stdout, "%d, %d, %d, %d\n", Itime, Idepth, Ilat_fine, Ilon_fine );


            // lat_lb is the smallest index such that coarse_lat(lat_lb) >= fine_lat(Ilat_fine)
            target_lat = fine_data.latitude.at(Ilat_fine);
            lat_lb =  std::lower_bound( coarse_data.latitude.begin(), coarse_data.latitude.end(), target_lat )
                    - coarse_data.latitude.begin();
            lat_lb = (lat_lb < 0) ? 0 : (lat_lb >= Nlat_coarse) ? Nlat_coarse - 1 : lat_lb;
            //fprintf( stdout, "  LAT: (%d, %g -> %d) \n", Ilat_fine, target_lat, lat_lb );
            if ( ( lat_lb > 0 ) and (   ( target_lat                      - coarse_data.latitude.at(lat_lb-1) ) 
                                      < ( coarse_data.latitude.at(lat_lb) - target_lat                        ) 
                                    ) 
               ) {
                lat_lb--;
            }


            // lon_lb is the smallest index such that coarse_lon(lon_lb) >= fine_lon(Ilon_fine)
            target_lon = fine_data.longitude.at(Ilon_fine);
            lon_lb =  std::lower_bound( coarse_data.longitude.begin(), coarse_data.longitude.end(), target_lon )
                    - coarse_data.longitude.begin();
            lon_lb = (lon_lb < 0) ? 0 : (lon_lb >= Nlon_coarse) ? Nlon_coarse - 1 : lon_lb;
            //fprintf( stdout, "  LON: (%d, %g -> %d) \n", Ilon_fine, target_lon, lon_lb );
            if ( ( lon_lb > 0 ) and (   ( target_lon                       - coarse_data.longitude.at(lon_lb-1) ) 
                                      < ( coarse_data.longitude.at(lon_lb) - target_lon                         ) 
                                    ) 
               ) {
                lon_lb--;
            }


            // Get the corresponding index in the coarse grid
            II_coarse = Index( Itime, Idepth, lat_lb,      lon_lb,
                               Ntime, Ndepth, Nlat_coarse, Nlon_coarse );

            // And drop into the fine grid
            //fprintf( stdout, "  MAP: c(%zu) -> f(%zu)\n", II_coarse, II_fine );
            var_fine.at(II_fine) = coarse_data.variables.at(var_name_coarse).at(II_coarse);
        }
    }
    fprintf( stdout, "Done refining the grid.\n" );

    // Compute the area of each 'cell' which will be necessary for creating the output file
    fprintf( stdout, "Computing cell areas.\n" );
    fine_data.compute_cell_areas();

    // Initialize file and write out coarsened fields
    fprintf( stdout, "Preparing output file\n" );
    std::vector<std::string> vars_to_write = { var_name_output, };
    initialize_output_file( fine_data.time, fine_data.depth, fine_data.longitude, fine_data.latitude, fine_data.areas, 
                            vars_to_write, output_fname.c_str() );

    fprintf( stdout, "Writing refined field\n" );
    size_t starts[4] = { coarse_data.myStarts.at(0), coarse_data.myStarts.at(1), 0,              0              };
    size_t counts[4] = { coarse_data.myCounts.at(0), coarse_data.myCounts.at(1), fine_data.Nlat, fine_data.Nlon };
    write_field_to_output( var_fine, var_name_output, starts, counts, output_fname, NULL );

    fprintf( stdout, "Storing seed count to file\n" );
    add_attr_to_file( "seed_count", full_Ntime, output_fname.c_str() );

    //
    fprintf(stdout, "Processor %d / %d waiting to finalize.\n", wRank + 1, wSize);
    MPI_Finalize();
    return 0;
}