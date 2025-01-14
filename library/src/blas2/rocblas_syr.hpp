/* ************************************************************************
 * Copyright 2016-2021 Advanced Micro Devices, Inc.
 * ************************************************************************ */

#pragma once

#include "check_numerics_vector.hpp"
#include "handle.hpp"
#include "rocblas.h"

template <rocblas_int DIM_X, rocblas_int DIM_Y, typename T, typename U, typename V, typename W>
ROCBLAS_KERNEL __launch_bounds__(DIM_X* DIM_Y) void rocblas_syr_kernel(rocblas_fill uplo,
                                                                       rocblas_int  n,
                                                                       U alpha_device_host,
                                                                       rocblas_stride stride_alpha,
                                                                       V              xa,
                                                                       ptrdiff_t      shiftx,
                                                                       rocblas_int    incx,
                                                                       rocblas_stride stridex,
                                                                       W              Aa,
                                                                       ptrdiff_t      shiftA,
                                                                       rocblas_int    lda,
                                                                       rocblas_stride strideA)
{
    auto alpha = load_scalar(alpha_device_host, hipBlockIdx_z, stride_alpha);
    if(!alpha)
        return;

    rocblas_int tx = hipBlockIdx_x * hipBlockDim_x + hipThreadIdx_x;
    rocblas_int ty = hipBlockIdx_y * hipBlockDim_y + hipThreadIdx_y;

    const auto* __restrict__ x = load_ptr_batch(xa, hipBlockIdx_z, shiftx, stridex);
    T* A                       = load_ptr_batch(Aa, hipBlockIdx_z, shiftA, strideA);

    if(uplo == rocblas_fill_lower ? tx < n && ty <= tx : ty < n && tx <= ty)
        A[tx + lda * ty] += alpha * x[tx * incx] * x[ty * incx];
}

template <typename T, typename U, typename V, typename W>
inline rocblas_status rocblas_syr_arg_check(rocblas_fill   uplo,
                                            rocblas_int    n,
                                            U              alpha,
                                            rocblas_stride stride_alpha,
                                            V              x,
                                            rocblas_int    offsetx,
                                            rocblas_int    incx,
                                            rocblas_stride stridex,
                                            W              A,
                                            rocblas_int    offseta,
                                            rocblas_int    lda,
                                            rocblas_stride strideA,
                                            rocblas_int    batch_count)
{
    if(uplo != rocblas_fill_lower && uplo != rocblas_fill_upper)
        return rocblas_status_invalid_value;

    if(n < 0 || !incx || lda < n || lda < 1 || batch_count < 0)
        return rocblas_status_invalid_size;

    if(!n || !batch_count)
        return rocblas_status_success;

    if(!alpha || !x || !A)
        return rocblas_status_invalid_pointer;

    return rocblas_status_continue;
}

template <typename T, typename U, typename V, typename W>
ROCBLAS_INTERNAL_EXPORT_NOINLINE rocblas_status
    rocblas_internal_syr_template(rocblas_handle handle,
                                  rocblas_fill   uplo,
                                  rocblas_int    n,
                                  U              alpha,
                                  rocblas_stride stride_alpha,
                                  V              x,
                                  rocblas_int    offsetx,
                                  rocblas_int    incx,
                                  rocblas_stride stridex,
                                  W              A,
                                  rocblas_int    offseta,
                                  rocblas_int    lda,
                                  rocblas_stride strideA,
                                  rocblas_int    batch_count)
{
    // Quick return
    if(!n || batch_count == 0)
        return rocblas_status_success;

    hipStream_t rocblas_stream = handle->get_stream();

    static constexpr int GEMV_DIM_X = 128;
    static constexpr int GEMV_DIM_Y = 8;
    rocblas_int          blocksX    = (n - 1) / GEMV_DIM_X + 1;
    rocblas_int          blocksY    = (n - 1) / GEMV_DIM_Y + 1;

    dim3 syr_grid(blocksX, blocksY, batch_count);
    dim3 syr_threads(GEMV_DIM_X, GEMV_DIM_Y);

    // in case of negative inc shift pointer to end of data for negative indexing tid*inc
    auto shiftx = incx < 0 ? offsetx - ptrdiff_t(incx) * (n - 1) : offsetx;

    if(rocblas_pointer_mode_device == handle->pointer_mode)
    {
        hipLaunchKernelGGL((rocblas_syr_kernel<GEMV_DIM_X, GEMV_DIM_Y, T>),
                           syr_grid,
                           syr_threads,
                           0,
                           rocblas_stream,
                           uplo,
                           n,
                           alpha,
                           stride_alpha,
                           x,
                           shiftx,
                           incx,
                           stridex,
                           A,
                           offseta,
                           lda,
                           strideA);
    }
    else
    {
        hipLaunchKernelGGL((rocblas_syr_kernel<GEMV_DIM_X, GEMV_DIM_Y, T>),
                           syr_grid,
                           syr_threads,
                           0,
                           rocblas_stream,
                           uplo,
                           n,
                           *alpha,
                           stride_alpha,
                           x,
                           shiftx,
                           incx,
                           stridex,
                           A,
                           offseta,
                           lda,
                           strideA);
    }
    return rocblas_status_success;
}

//TODO :-Add rocblas_check_numerics_sy_matrix_template for checking Matrix `A` which is a Symmetric Matrix
template <typename T, typename U>
rocblas_status rocblas_syr_check_numerics(const char*    function_name,
                                          rocblas_handle handle,
                                          rocblas_int    n,
                                          T              A,
                                          rocblas_int    offset_a,
                                          rocblas_int    lda,
                                          rocblas_stride stride_a,
                                          U              x,
                                          rocblas_int    offset_x,
                                          rocblas_int    inc_x,
                                          rocblas_stride stride_x,
                                          rocblas_int    batch_count,
                                          const int      check_numerics,
                                          bool           is_input)
{
    rocblas_status check_numerics_status
        = rocblas_internal_check_numerics_vector_template(function_name,
                                                          handle,
                                                          n,
                                                          x,
                                                          offset_x,
                                                          inc_x,
                                                          stride_x,
                                                          batch_count,
                                                          check_numerics,
                                                          is_input);

    return check_numerics_status;
}
