#include <assert.h>
#include "mkl_dnn_types.h"

#include "c_types_map.hpp"
#include "memory_desc_wrapper.hpp"
#include "type_helpers.hpp"

namespace mkl_dnn {
namespace impl {

namespace {
using mkl_dnn::impl::array_set;
using namespace mkl_dnn::impl::status;
using namespace mkl_dnn::impl::memory_format;

status_t fill_n(blocking_desc_t &blk, const tensor_desc_t &tensor) {
    const uint32_t ndims = types::ndims(tensor);
    if (ndims != 1) return invalid_arguments;
    array_set(blk.padding_dims, 0, ndims);
    array_set(blk.block_dims, 1, ndims);
    array_set(blk.strides[1], 1, ndims);
    blk.strides[0][0] = 1;
    return success;
}

/* TODO: improve me maybe... and put this to utils */
inline void set_default_strides(dims_t strides, const dims_t sizes,
        uint32_t ndims, const uint32_t *perm = NULL) {
    uint32_t id_perm[ndims];
    for (uint32_t i = 0; i < ndims; ++i)
        id_perm[i] = i;
    if (perm == NULL)
        perm = id_perm;

    strides[perm[ndims - 1]] = 1;
    for (uint32_t d = 1; d < ndims; ++d) {
        const uint32_t prev_idx = perm[ndims - d];
        const uint32_t curr_idx = perm[ndims - 1 - d];

        strides[curr_idx] = sizes[curr_idx] == 0
            ? 1
            : strides[prev_idx] * nstl::max(1u, sizes[prev_idx]);
    }
}

status_t fill_nonblocked(blocking_desc_t &blk, const tensor_desc_t &tensor,
        const uint32_t perm[]) {
    const uint32_t ndims = types::ndims(tensor);
    array_set(blk.padding_dims, 0, ndims);
    array_set(blk.block_dims, 1, ndims);
    array_set(blk.strides[1], 1, ndims);
    set_default_strides(blk.strides[0], tensor.dims, ndims, perm);
    return success;
}

status_t fill_contiguous_blocked(blocking_desc_t &blk,
        const tensor_desc_t &tensor, const uint32_t block_dims[],
        const uint32_t perm[]) {
    /* TODO: check for dims[d] % block_dims[d] != 0 */
    const uint32_t ndims = types::ndims(tensor);
    array_set(blk.padding_dims, 0, ndims);
    array_copy(blk.block_dims, block_dims, ndims);

    uint32_t unrolled_dims[2*ndims];
    uint32_t unrolled_strides[2*ndims];
    for (uint32_t d = 0; d < ndims; ++d) {
        unrolled_dims[d] = tensor.dims[d] / block_dims[d];
        unrolled_dims[ndims + d] = block_dims[d];
    }

    set_default_strides(unrolled_strides, unrolled_dims, 2*ndims, perm);
    array_copy(blk.strides[0], &unrolled_strides[0], ndims);
    array_copy(blk.strides[1], &unrolled_strides[ndims], ndims);
    return success;
}

status_t fill_nc(blocking_desc_t &blk, const tensor_desc_t &tensor) {
    bool args_ok = tensor.ndims_batch == 1
            && tensor.ndims_channels == 1
            && tensor.ndims_spatial == 0;
    if (!args_ok) return invalid_arguments;

    const uint32_t perm[2] = {0, 1};
    return fill_nonblocked(blk, tensor, perm);
}

status_t fill_nchw(blocking_desc_t &blk, const tensor_desc_t &tensor) {
    bool args_ok = tensor.ndims_batch == 1
            && tensor.ndims_channels == 1
            && tensor.ndims_spatial == 2;
    if (!args_ok) return invalid_arguments;

    const uint32_t perm[4] = {0, 1, 2, 3};
    return fill_nonblocked(blk, tensor, perm);
}

status_t fill_nhwc(blocking_desc_t &blk, const tensor_desc_t &tensor) {
    bool args_ok = tensor.ndims_batch == 1
            && tensor.ndims_channels == 1
            && tensor.ndims_spatial == 2;
    if (!args_ok) return invalid_arguments;

    const uint32_t perm[4] = {0, 2, 3, 1};
    return fill_nonblocked(blk, tensor, perm);
}

status_t fill_nChw8c(blocking_desc_t &blk, const tensor_desc_t &tensor) {
    bool args_ok = tensor.ndims_batch == 1
            && tensor.ndims_channels == 1
            && tensor.ndims_spatial == 2;
    if (!args_ok) return invalid_arguments;

    const uint32_t block_dims[] = {1, 8, 1, 1};
    const uint32_t perm[] = {
        0, 1, 2, 3,
        4, 5, 6, 7};
    return fill_contiguous_blocked(blk, tensor, block_dims, perm);
}

status_t fill_oi(blocking_desc_t &blk, const tensor_desc_t &tensor) {
    bool args_ok = tensor.ndims_batch == 0
            && tensor.ndims_channels == 2
            && tensor.ndims_spatial == 0;
    if (!args_ok) return invalid_arguments;

    const uint32_t perm[2] = {0, 1};
    return fill_nonblocked(blk, tensor, perm);
}

status_t fill_oihw(blocking_desc_t &blk, const tensor_desc_t &tensor) {
    bool args_ok = tensor.ndims_batch == 0
            && tensor.ndims_channels == 2
            && tensor.ndims_spatial == 2;
    if (!args_ok) return invalid_arguments;

    const uint32_t perm[4] = {0, 1, 2, 3};
    return fill_nonblocked(blk, tensor, perm);
}

status_t fill_OIhw8i8o(blocking_desc_t &blk, const tensor_desc_t &tensor) {
    bool args_ok = tensor.ndims_batch == 0
            && tensor.ndims_channels == 2
            && tensor.ndims_spatial == 2;
    if (!args_ok) return invalid_arguments;

    const uint32_t block_dims[] = {8, 8, 1, 1};
    const uint32_t perm[] = {
        0, 1, 2, 3,
        5, 4, 6, 7};
    return fill_contiguous_blocked(blk, tensor, block_dims, perm);
}

status_t fill_goihw(blocking_desc_t &blk, const tensor_desc_t &tensor) {
    bool args_ok = tensor.ndims_batch == 1
            && tensor.ndims_channels == 2
            && tensor.ndims_spatial == 2;
    if (!args_ok) return invalid_arguments;

    const uint32_t perm[5] = {0, 1, 2, 3, 4};
    return fill_nonblocked(blk, tensor, perm);
}

status_t fill_gOIhw8i8o(blocking_desc_t &blk, const tensor_desc_t &tensor) {
    bool args_ok = tensor.ndims_batch == 1
            && tensor.ndims_channels == 2
            && tensor.ndims_spatial == 2;
    if (!args_ok) return invalid_arguments;

    const uint32_t block_dims[] = {1, 8, 8, 1, 1};
    const uint32_t perm[] = {
        0, 1, 2, 3, 4,
        5, 7, 6, 8, 9};
    return fill_contiguous_blocked(blk, tensor, block_dims, perm);
}

}

status_t memory_desc_wrapper::compute_blocking(memory_desc_t &memory_desc)
{
    if (types::ndims(memory_desc.tensor_desc) == 0)
        return invalid_arguments;

    const tensor_desc_t &tensor = memory_desc.tensor_desc;
    blocking_desc_t &blk = memory_desc.blocking_desc;

    switch (memory_desc.format) {
    case n: return fill_n(blk, tensor);
    case nc: return fill_nc(blk, tensor);
    case nchw: return fill_nchw(blk, tensor);
    case nhwc: return fill_nhwc(blk, tensor);
    case nChw8c: return fill_nChw8c(blk, tensor);
    case oi: return fill_oi(blk, tensor);
    case oihw: return fill_oihw(blk, tensor);
    case OIhw8i8o: return fill_OIhw8i8o(blk, tensor);
    case goihw: return fill_goihw(blk, tensor);
    case gOIhw8i8o: return fill_gOIhw8i8o(blk, tensor);
    default: break;
    }

    return invalid;
}

}
}

// vim: et ts=4 sw=4 cindent cino^=l0,\:0,N-s
