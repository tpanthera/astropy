// Licensed under a 3-clause BSD style license - see LICENSE.rst

/*----------------------------- WARNING! -----------------------------
 * The C functions below are NOT designed to be called externally to
 * the Python function astropy/astropy/convolution/convolve.py.
 * They do NOT include any of the required correct usage checking.
 *
 *------------------------------- NOTES ------------------------------
 *
 * The simplest implementation of convolution does not deal with any boundary
 * treatment, and pixels within half a kernel width of the edge of the image are
 * set to zero. In cases where a boundary mode is set, we pad the input array in
 * the Python code. In the 1D case, this means that the input array to the C
 * code has a size nx + nkx where nx is the original array size and nkx is the
 * size of the kernel. If we also padded the results array, then we could use
 * the exact same C code for the convolution, provided that the results array
 * was 'unpadded' in the Python code after the C code.
 *
 * However, to avoid needlessly padding the results array, we instead adjust the
 * index when accessing the results array - for example in the 1D case we shift
 * the index in the results array compared to the input array by half the kernel
 * size. This is done via the 'result_index' variable, and this behavior is
 * triggered by the 'padded' setting.
 *
 */


#include <assert.h>
#include <math.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stddef.h>

#include "convolve.h"

// Distutils on Windows automatically exports ``PyInit__convolve``,
// create dummy to prevent linker complaining about missing symbol.
#if defined(_MSC_VER)
void PyInit__convolve(void)
{
    return;
}
#endif

#ifdef _OPENMP
#include <omp.h>
#endif


void convolveNd_c(DTYPE * const result,
        const DTYPE * const f,
        const unsigned n_dim,
        const size_t * const image_shape,
        const DTYPE * const g,
        const size_t * const kernel_shape,
        const bool nan_interpolate,
        const bool padded,
        const unsigned n_threads)
{
#ifdef NDEBUG
    if (!result || !f || !g || !image_shape || !kernel_shape)
        return;
#else
    assert(result);
    assert(f);
    assert(g);
    assert(image_shape);
    assert(kernel_shape);
#endif

    if (n_dim == 1)
        convolve1d_c(result, f,
                image_shape[0],
                g, kernel_shape[0],
                nan_interpolate, padded, n_threads);
    else if (n_dim == 2)
        convolve2d_c(result, f,
                image_shape[0], image_shape[1],
                g, kernel_shape[0], kernel_shape[1],
                nan_interpolate, padded, n_threads);
    else if (n_dim == 3)
        convolve3d_c(result, f,
                        image_shape[0], image_shape[1], image_shape[2],
                        g, kernel_shape[0], kernel_shape[1], kernel_shape[2],
                        nan_interpolate, padded, n_threads);
    else
        assert(0); // Unimplemented: n_dim > 3
}

/*-------------------------PERFORMANCE NOTES--------------------------------
 * The function wrappers below are designed to take advantage of the following:
 * The preprocessor will inline convolve<N>d(), effectively
 * expanding the two logical branches, replacing nan_interpolate
 * for their literal equivalents. The corresponding conditionals
 * within these functions will then be optimized away, this
 * being the goal - removing the unnecessary conditionals from
 * the loops without duplicating code.
 *--------------------------------------------------------------------------
 */

void convolve1d_c(DTYPE * const result,
        const DTYPE * const f, const size_t nx,
        const DTYPE * const g, const size_t nkx,
        const bool nan_interpolate, const bool padded,
        const unsigned n_threads)
{
#ifdef NDEBUG
    if (!result || !f || !g)
        return;
#else
    assert(result);
    assert(f);
    assert(g);
#endif

    if (nan_interpolate) {
      if (padded)
        convolve1d(result, f, nx, g, nkx, true, true, n_threads);
      else
        convolve1d(result, f, nx, g, nkx, true, false, n_threads);
    } else {
      if (padded)
        convolve1d(result, f, nx, g, nkx, false, true, n_threads);
      else
        convolve1d(result, f, nx, g, nkx, false, false, n_threads);
    }
}

void convolve2d_c(DTYPE * const result,
        const DTYPE * const f, const size_t nx, const size_t ny,
        const DTYPE * const g, const size_t nkx, const size_t nky,
        const bool nan_interpolate, const bool padded,
        const unsigned n_threads)
{
#ifdef NDEBUG
    if (!result || !f || !g)
        return;
#else
    assert(result);
    assert(f);
    assert(g);
#endif

    if (nan_interpolate) {
      if (padded)
        convolve2d(result, f, nx, ny, g, nkx, nky, true, true, n_threads);
      else
        convolve2d(result, f, nx, ny, g, nkx, nky, true, false, n_threads);
    } else {
      if (padded)
        convolve2d(result, f, nx, ny, g, nkx, nky, false, true, n_threads);
      else
        convolve2d(result, f, nx, ny, g, nkx, nky, false, false, n_threads);
    }
}

void convolve3d_c(DTYPE * const result,
        const DTYPE * const f, const size_t nx, const size_t ny, const size_t nz,
        const DTYPE * const g, const size_t nkx, const size_t nky, const size_t nkz,
        const bool nan_interpolate, const bool padded,
        const unsigned n_threads)
{
#ifdef NDEBUG
    if (!result || !f || !g)
        return;
#else
    assert(result);
    assert(f);
    assert(g);
#endif

    if (nan_interpolate) {
      if (padded)
        convolve3d(result, f, nx, ny, nz, g, nkx, nky, nkz, true, true, n_threads);
      else
        convolve3d(result, f, nx, ny, nz, g, nkx, nky, nkz, true, false, n_threads);
    } else {
      if (padded)
        convolve3d(result, f, nx, ny, nz, g, nkx, nky, nkz, false, true, n_threads);
      else
        convolve3d(result, f, nx, ny, nz, g, nkx, nky, nkz, false, false, n_threads);
    }
}

// 1D
FORCE_INLINE void convolve1d(DTYPE * const result,
        const DTYPE * const f, const size_t _nx,
        const DTYPE * const g, const size_t _nkx,
        const bool _nan_interpolate, const bool _padded,
        const unsigned n_threads)
{
#ifdef NDEBUG
    if (!result || !f || !g)
        return;
#else
    assert(result);
    assert(f);
    assert(g);
#endif

    const size_t _wkx = _nkx / 2;

#ifdef NDEBUG
    if (!(_nx > 2*_wkx))
        return;
#else
    assert(_nx > 2*_wkx);
#endif

#ifdef _OPENMP
    omp_set_num_threads(n_threads); // Set number of threads to use
#pragma omp parallel
    { // Code within this block is threaded
#endif

    // Copy these to thread locals to allow compiler to optimize (hoist/loads licm)
    // when threaded. Without these, compile time constant conditionals may
    // not be optimized away.
    const size_t nx = _nx;
    const bool nan_interpolate = _nan_interpolate;
    const bool padded = _padded;

    // Thread locals
    const size_t wkx = _wkx;
    size_t wkx_plus_i;
    size_t ker_i;
    const omp_iter_var nx_minus_wkx = nx - wkx;
    size_t i_minus_wkx;
    omp_iter_var i_plus_wkx_plus_1;
    size_t result_index;

    DTYPE top, bot=0., ker, val;

    {omp_iter_var i;
#ifdef _OPENMP
#pragma omp for schedule(dynamic)
#endif
    for (i = wkx; i < nx_minus_wkx; ++i)
    {
        wkx_plus_i = wkx + i; // wkx + i
        i_minus_wkx = i - wkx; //i - wkx
        i_plus_wkx_plus_1 = wkx_plus_i + 1; // i + wkx + 1

        top = 0.;
        if (nan_interpolate) // compile time constant
            bot = 0.;
        {omp_iter_var ii;
        for (ii = i_minus_wkx; ii < i_plus_wkx_plus_1; ++ii)
        {
            ker_i = wkx_plus_i - ii; // nkx - 1 - (wkx + ii - i)
            val = f[ii];
            ker = g[ker_i];
            if (nan_interpolate) // compile time constant
            {
                if (!isnan(val))
                {
                    top += val * ker;
                    bot += ker;
                }
            }
            else
                top += val * ker;
        }}

        if (padded) { // compile time constant
          result_index = i_minus_wkx;
        } else {
          result_index = i;
        }

        if (nan_interpolate) // compile time constant
        {
            if (bot == 0) // This should prob be np.isclose(kernel_sum, 0, atol=normalization_zero_tol)
                result[result_index]  = f[i];
            else
                result[result_index]  = top / bot;
        }
        else
            result[result_index] = top;
    }}
#ifdef _OPENMP
    }//end parallel scope
#endif
}

// 2D
FORCE_INLINE void convolve2d(DTYPE * const result,
        const DTYPE * const f, const size_t _nx, const size_t _ny,
        const DTYPE * const g, const size_t _nkx, const size_t _nky,
        const bool _nan_interpolate, const bool _padded,
        const unsigned n_threads)
{
#ifdef NDEBUG
    if (!result || !f || !g)
        return;
#else
    assert(result);
    assert(f);
    assert(g);
#endif

    const size_t _wkx = _nkx / 2;
    const size_t _wky = _nky / 2;
#ifdef NDEBUG
    if (!(_nx > 2*_wkx) || !(_ny > 2*_wky))
        return;
#else
    assert(_nx > 2*_wkx);
    assert(_ny > 2*_wky);
#endif

#ifdef _OPENMP
    omp_set_num_threads(n_threads); // Set number of threads to use
#pragma omp parallel
    { // Code within this block is threaded
#endif

    // Copy these to thread locals to allow compiler to optimize (hoist/loads licm)
    // when threaded. Without these, compile time constant conditionals may
    // not be optimized away.
    const size_t nx = _nx, ny = _ny;
    const size_t nky = _nky;
    const bool nan_interpolate = _nan_interpolate;
    const bool padded = _padded;

    // Thread locals
    const size_t wkx = _wkx;
    const size_t wky = _wky;
    size_t wkx_plus_i, wky_plus_j;
    size_t ker_i, ker_j;
    const omp_iter_var nx_minus_wkx = nx - wkx;
    const omp_iter_var ny_minus_wky = ny - wky;
    const size_t ny_minus_2wky = ny - 2 * wky;
    size_t i_minus_wkx, j_minus_wky;
    omp_iter_var i_plus_wkx_plus_1, j_plus_wky_plus_1;
    size_t result_index;

    DTYPE top, bot=0., ker, val;

    {omp_iter_var i;
#ifdef _OPENMP
#pragma omp for schedule(dynamic)
#endif
    for (i = wkx; i < nx_minus_wkx; ++i)
    {
        wkx_plus_i = wkx + i; // wkx + 1
        i_minus_wkx = i - wkx; //i - wkx
        i_plus_wkx_plus_1 = wkx_plus_i + 1; // i + wkx + 1

        {omp_iter_var j;
        for (j = wky; j < ny_minus_wky; ++j)
        {
            wky_plus_j = wky + j; // wky + j
            j_minus_wky = j - wky; // j - wky
            j_plus_wky_plus_1 = wky_plus_j + 1; // j + wky + 1

            top = 0.;
            if (nan_interpolate) // compile time constant
                bot = 0.;
            {omp_iter_var ii;
            for (ii = i_minus_wkx; ii < i_plus_wkx_plus_1; ++ii)
            {
                ker_i = wkx_plus_i - ii; // nkx - 1 - (wkx + ii - i)
                {omp_iter_var jj;
                for (jj = j_minus_wky; jj < j_plus_wky_plus_1; ++jj)
                {
                    ker_j = wky_plus_j - jj; // nky - 1 - (wky + jj - j)
                    val = f[ii*ny + jj]; //[ii, jj];
                    ker = g[ker_i*nky + ker_j]; // [ker_i, ker_j];
                    if (nan_interpolate) // compile time constant
                    {
                        if (!isnan(val))
                        {
                            top += val * ker;
                            bot += ker;
                        }
                    }
                    else
                        top += val * ker;
                }}
            }}

            if (padded) { // compile time constant
              result_index = i_minus_wkx * ny_minus_2wky + j_minus_wky;
            } else {
              result_index = i*ny + j;
            }

            if (nan_interpolate) // compile time constant
            {
                if (bot == 0) // This should prob be np.isclose(kernel_sum, 0, atol=normalization_zero_tol)
                    result[result_index]  = f[i*ny + j] ;
                else
                    result[result_index]  = top / bot;
            }
            else
                result[result_index] = top;
        }}
    }}
#ifdef _OPENMP
    }//end parallel scope
#endif
}

// 3D
FORCE_INLINE void convolve3d(DTYPE * const result,
        const DTYPE * const f, const size_t _nx, const size_t _ny, const size_t _nz,
        const DTYPE * const g, const size_t _nkx, const size_t _nky, const size_t _nkz,
        const bool _nan_interpolate, const bool _padded,
        const unsigned n_threads)
{
#ifdef NDEBUG
    if (!result || !f || !g)
        return;
#else
    assert(result);
    assert(f);
    assert(g);
#endif

    const size_t _wkx = _nkx / 2;
    const size_t _wky = _nky / 2;
    const size_t _wkz = _nkz / 2;
#ifdef NDEBUG
    if (!(_nx > 2*_wkx) || !(_ny > 2*_wky) || !(_nz > 2*_wkz))
        return;
#else
    assert(_nx > 2*_wkx);
    assert(_ny > 2*_wky);
    assert(_nz > 2*_wkz);
#endif

#ifdef _OPENMP
    omp_set_num_threads(n_threads); // Set number of threads to use
#pragma omp parallel
    { // Code within this block is threaded
#endif

    // Copy these to thread locals to allow compiler to optimize (hoist/loads licm)
    // when threaded. Without these, compile time constant conditionals may
    // not be optimized away.
    const size_t nx = _nx, ny = _ny, nz = _nz;
    const size_t nky = _nky, nkz = _nkz;
    const bool nan_interpolate = _nan_interpolate;
    const bool padded = _padded;

    // Thread locals
    const size_t wkx = _wkx;
    const size_t wky = _wky;
    const size_t wkz = _wkz;
    size_t wkx_plus_i, wky_plus_j, wkz_plus_k;
    size_t ker_i, ker_j, ker_k;
    const size_t nx_minus_wkx = nx - wkx;
    const omp_iter_var ny_minus_wky = ny - wky;
    const omp_iter_var nz_minus_wkz = nz - wkz;
    const size_t ny_minus_2wky = ny - 2 * wky;
    const size_t nz_minus_2wkz = nz - 2 * wkz;

    size_t i_minus_wkx, j_minus_wky, k_minus_wkz;
    omp_iter_var i_plus_wkx_plus_1, j_plus_wky_plus_1, k_plus_wkz_plus_1;
    size_t result_index;

    DTYPE top, bot=0., ker, val;

    {omp_iter_var i;
#ifdef _OPENMP
#pragma omp for schedule(dynamic)
#endif
    for (i = wkx; i < nx_minus_wkx; ++i)
    {
        wkx_plus_i = wkx + i; // wkx + i
        i_minus_wkx = i - wkx; //i - wkx
        i_plus_wkx_plus_1 = wkx_plus_i + 1; // i + wkx + 1

        {omp_iter_var j;
        for (j = wky; j < ny_minus_wky; ++j)
        {
            wky_plus_j = wky + j; // wky + j
            j_minus_wky = j - wky; // j - wky
            j_plus_wky_plus_1 = wky_plus_j + 1; // j + wky + 1

            {omp_iter_var k;
            for (k = wkz; k < nz_minus_wkz; ++k)
            {
                wkz_plus_k = wkz + k; // wkz + k
                k_minus_wkz = k - wkz; // k - wkz
                k_plus_wkz_plus_1 = wkz_plus_k + 1; // k + wkz + 1

                top = 0.;
                if (nan_interpolate) // compile time constant
                    bot = 0.;
                {omp_iter_var ii;
                for (ii = i_minus_wkx; ii < i_plus_wkx_plus_1; ++ii)
                {
                    ker_i = wkx_plus_i - ii; // nkx - 1 - (wkx + ii - i)
                    {omp_iter_var jj;
                    for (jj = j_minus_wky; jj < j_plus_wky_plus_1; ++jj)
                    {
                        ker_j = wky_plus_j - jj; // nky - 1 - (wky + jj - j)
                        {omp_iter_var kk;
                        for (kk = k_minus_wkz; kk < k_plus_wkz_plus_1; ++kk)
                        {
                            ker_k = wkz_plus_k - kk; // nkz - 1 - (wkz + kk - k)

                            val = f[(ii*ny + jj)*nz + kk]; //[ii, jj, kk];
                            ker = g[(ker_i*nky + ker_j)*nkz + ker_k]; // [ker_i, ker_j, ker_k];
                            if (nan_interpolate) // compile time constant
                            {
                                if (!isnan(val))
                                {
                                    top += val * ker;
                                    bot += ker;
                                }
                            }
                            else
                                top += val * ker;
                        }}
                    }}
                }}

                if (padded) { // compile time constant
                    result_index = (i_minus_wkx*ny_minus_2wky + j_minus_wky)*nz_minus_2wkz + k_minus_wkz;
                } else {
                    result_index = (i*ny + j)*nz + k;
                }

                if (nan_interpolate) // compile time constant
                {
                    if (bot == 0) // This should prob be np.isclose(kernel_sum, 0, atol=normalization_zero_tol)
                        result[result_index]  = f[(i*ny + j)*nz + k] ;
                    else
                        result[result_index]  = top / bot;
                }
                else
                    result[result_index] = top;
            }}
        }}
    }}
#ifdef _OPENMP
    }//end parallel scope
#endif
}
