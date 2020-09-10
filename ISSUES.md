# Solutions to Common Problems

## Interpolation

### alglib::ap_error
When running the interpolator.x script, you may encounter the error message 
"terminate called after throwing an instance of 'alglib::ap_error'".
So far, this seems to come up as the result of NaN values. There are two main reasons
that have caused this so far.
1. The input fields (potential temparture, salinity, sea level anomaly) have different mask,
which is not accounted for in the input file. (The code has been modified so as to mostly avoid this).
2. The input fields are using unexpected units or have erroneous values. Please check variable values.
(e.g. temperature should be in Celsius, salinity values should not be negative)

The output file pre_interp.nc (requires DEBUG >= 1) shows the values computed before interpolation.
Checking those values may help to clarify the issue.