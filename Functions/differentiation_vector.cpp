#include "../functions.hpp"
// differentaition_vector sets the vector needed to compute 
//   a fourth-order derivative on a five-point stencil.
// index specifies the index at which we want the derivative to be computed

void differentiation_vector(
        double * diff_array, /**< [in] the array into which the differentiation coefficients should be placed */
        const double delta,  /**< [in] the uniform grid spacing */
        const int index      /**< [in] at which position in the five-point stencil do we want the derivative */
        ) {

    if (index == 0) {
        diff_array[0] = -6.25 / 3;
        diff_array[1] =  4.      ;
        diff_array[2] = -3.      ;
        diff_array[3] =  4.   / 3;
        diff_array[4] = -0.75 / 3;
    } else if (index == 1) {
        diff_array[0] = -0.75 / 3;
        diff_array[1] = -2.5  / 3;
        diff_array[2] =  4.5  / 3;
        diff_array[3] = -1.5  / 3;
        diff_array[4] =  0.25 / 3;
    } else if (index == 2) {
        diff_array[0] =  0.25 / 3;
        diff_array[1] = -2.   / 3;
        diff_array[2] =  0.      ;
        diff_array[3] =  2.   / 3;
        diff_array[4] = -0.25 / 3;
    } else if (index == 3) {
        diff_array[0] = -0.25 / 3;
        diff_array[1] =  1.5  / 3;
        diff_array[2] = -4.5  / 3;
        diff_array[3] =  2.5  / 3;
        diff_array[4] =  0.75 / 3;
    } else if (index == 4) {
        diff_array[0] =  0.75 / 3;
        diff_array[1] = -4.   / 3;
        diff_array[2] =  3.      ;
        diff_array[3] = -4.      ;
        diff_array[4] =  6.25 / 3;
    }

    for (int II = 0; II < 5; II++) {
        diff_array[II] *= 1./delta;
    }
}
