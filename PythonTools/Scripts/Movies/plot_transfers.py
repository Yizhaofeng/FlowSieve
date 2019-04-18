import numpy as np
import matplotlib as mpl
import matplotlib.pyplot as plt
import cmocean, sys, PlotTools, os, shutil, datetime, glob
from netCDF4 import Dataset
from matplotlib.colors import ListedColormap

dpi = PlotTools.dpi

# List of variables to (try to) plot
variables = ['energy_transfer', 'Lambda_m', 'PEtoKE', 'div_Jtransport']

try: # Try using mpi
    from mpi4py import MPI
    comm = MPI.COMM_WORLD
    rank = comm.Get_rank()
    num_procs = comm.Get_size()
except:
    rank = 0
    num_procs = 1
print("Proc {0:d} of {1:d}".format(rank+1,num_procs))

# Get the available filter files
files = glob.glob('filter_*.nc')

# If the Figures directory doesn't exist, create it.
# Same with the Figures/tmp
out_direct = os.getcwd() + '/Videos'
tmp_direct = out_direct + '/tmp'

if (rank == 0):
    print("Saving outputs to " + out_direct)
    print("  will use temporary directory " + tmp_direct)

    if not(os.path.exists(out_direct)):
        os.makedirs(out_direct)

    if not(os.path.exists(tmp_direct)):
        os.makedirs(tmp_direct)

source = Dataset('input.nc', 'r')
try:
    units = source.variables['latitude'].units
except:
    units = ''

# Create cmap for mask data
ref_cmap = cmocean.cm.gray
mask_cmap = ref_cmap(np.arange(ref_cmap.N))
mask_cmap[:,-1] = np.linspace(1, 0, ref_cmap.N)
mask_cmap = ListedColormap(mask_cmap)

# Some parameters for plotting
cbar_props     = dict(pad = 0.02, shrink = 0.85)
gridspec_props = dict(wspace = 0.15, hspace = 0.15, left = 0.1, right = 0.95, bottom = 0.1, top = 0.9)

# Loop through filters
for fp in files:
    with Dataset(fp, 'r') as results:
        scale = results.filter_scale

        # Get the grid from the first filter
        if units == 'm':
            latitude  = results.variables['latitude'][:] / 1e3
            longitude = results.variables['longitude'][:] / 1e3
        else:
            latitude  = results.variables['latitude'][:]
            longitude = results.variables['longitude'][:]
        LON, LAT = np.meshgrid(longitude, latitude)

        depth = results.variables['depth'][:]
        time  = results.variables['time'][:] * (60*60) # convert hours to seconds
        mask  = results.variables['mask'][:]

        Ntime = len(time)

        # Do some time handling tp adjust the epochs
        # appropriately
        epoch       = datetime.datetime(1950,1,1)   # the epoch of the time dimension
        dt_epoch    = datetime.datetime.fromtimestamp(0)  # the epoch used by datetime
        epoch_delta = dt_epoch - epoch  # difference
        time        = time - epoch_delta.total_seconds()  # shift

        # lat/lon lines to draw
        meridians = np.round(np.linspace(longitude.min(), longitude.max(), 5))
        parallels = np.round(np.linspace(latitude.min(),  latitude.max(), 5))

        # Map projection
        proj = PlotTools.MapProjection(longitude, latitude)
        Xp, Yp = proj(LON, LAT, inverse=False)

        rat   = (Xp.max() - Xp.min()) / (Yp.max() - Yp.min())
        rat  *= 1.2
        fig_h = 8.

        # Plot stuff
        for Itime in range(rank, Ntime, num_procs):    

            timestamp = datetime.datetime.fromtimestamp(time[Itime])
            sup_title = "{0:02d} - {1:02d} - {2:04d} ( {3:02d}:{4:02d} )".format(
                timestamp.day, timestamp.month, timestamp.year, 
                timestamp.hour, timestamp.minute)

            for var_name in variables:
                if var_name in results.variables:

                    # Initialize figure
                    fig, axes = plt.subplots(1, 1,
                        sharex=True, sharey=True, squeeze=False, 
                        gridspec_kw = gridspec_props,
                        figsize=(fig_h*rat, fig_h))

                    fig.suptitle(sup_title)

                    to_plot = results.variables[var_name][Itime, 0, :, :]
                    if np.max(np.abs(to_plot)) > 0:
                        PlotTools.SignedLogPlot_onMap(LON, LAT, to_plot, 
                                axes[0,0], fig, proj, num_ords = 2, percentile=99.9)

                    # Add land and lat/lon lines
                    axes[0,0].pcolormesh(Xp, Yp, mask, vmin=-1, vmax=1, cmap=mask_cmap)
                    PlotTools.AddParallels_and_Meridians(axes[0,0], proj, 
                        parallels, meridians, latitude, longitude)

                    for ax in axes.ravel():
                        ax.set_aspect('equal')
        
                    plt.savefig(tmp_direct + '/{0:.4g}_{1:s}_{2:04d}.png'.format(
                        scale/1e3, var_name, Itime), dpi=dpi)
                    plt.close()

    
if (rank > 0):
    sys.exit()

# If more than one time point, create mp4s
if Ntime > 1:
    for fp in files:
        with Dataset(fp, 'r') as results:
            scale = results.filter_scale
            for var_name in variables:
                if var_name in results.variables:
                    PlotTools.merge_to_mp4(tmp_direct + '/{0:.04g}_{1:s}_%04d.png'.format(scale/1e3, var_name),
                            out_direct + '/{0:.04g}km/{1:s}.mp4'.format(scale/1e3, var_name), fps=12)
else:
    for fp in files:
        with Dataset(fp, 'r') as results:
            scale = results.filter_scale
            for var_name in variables:
                if var_name in results.variables:
                    shutil.move(tmp_direct + '/{0:.04g}_{1:s}_0000.png'.format(scale/1e3, var_name),
                            out_direct + '/{0:.04g}km/{1:s}.png'.format(scale/1e3, var_name))

# Now delete the frames
shutil.rmtree(tmp_direct)