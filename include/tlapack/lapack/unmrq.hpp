/// @file unmrq.hpp Multiplies the general m-by-n matrix C by Q from
/// tlapack::gerqf()
/// @author Thijs Steel, KU Leuven, Belgium
//
// Copyright (c) 2021-2023, University of Colorado Denver. All rights reserved.
//
// This file is part of <T>LAPACK.
// <T>LAPACK is free software: you can redistribute it and/or modify it under
// the terms of the BSD 3-Clause license. See the accompanying LICENSE file.

#ifndef TLAPACK_UNMRQ_HH
#define TLAPACK_UNMRQ_HH

#include "tlapack/base/utils.hpp"
#include "tlapack/lapack/larfb.hpp"
#include "tlapack/lapack/larft.hpp"

namespace tlapack {

/**
 * Options struct for unmrq
 */
template <TLAPACK_INDEX idx_t = size_t>
struct UnmrqOpts {
    idx_t nb = 32;  ///< Block size
};

/** Worspace query of unmrq()
 *
 * @param[in] side Specifies which side op(Q) is to be applied.
 *      - Side::Left:  C := op(Q) C;
 *      - Side::Right: C := C op(Q).
 *
 * @param[in] trans The operation $op(Q)$ to be used:
 *      - Op::NoTrans:      $op(Q) = Q$;
 *      - Op::ConjTrans:    $op(Q) = Q^H$.
 *      Op::Trans is a valid value if the data type of A is real. In this case,
 *      the algorithm treats Op::Trans as Op::ConjTrans.
 *
 * @param[in] A
 *      - side = Side::Left:    k-by-m matrix;
 *      - side = Side::Right:   k-by-n matrix.
 *      Contains the vectors that define the reflectors
 *
 * @param[in] tau Vector of length k
 *      Contains the scalar factors of the elementary reflectors.
 *
 * @param[in] C m-by-n matrix.
 *
 * @param[in] opts Options.
 *
 * @return WorkInfo The amount workspace required.
 *
 * @ingroup workspace_query
 *
 * @see unmrq
 */
template <class T,
          TLAPACK_SMATRIX matrixA_t,
          TLAPACK_SMATRIX matrixC_t,
          TLAPACK_SVECTOR tau_t,
          TLAPACK_SIDE side_t,
          TLAPACK_OP trans_t>
inline constexpr WorkInfo unmrq_worksize(
    side_t side,
    trans_t trans,
    const matrixA_t& A,
    const tau_t& tau,
    const matrixC_t& C,
    const UnmrqOpts<size_type<matrixC_t>>& opts = {})
{
    using idx_t = size_type<matrixC_t>;
    using matrixT_t = matrix_type<matrixA_t, tau_t>;
    using range = pair<idx_t, idx_t>;

    // Constants
    const idx_t k = size(tau);
    const idx_t nb = min<idx_t>(opts.nb, k);

    // Local workspace sizes
    WorkInfo workinfo =
        (is_same_v<T, type_t<matrixT_t>>) ? WorkInfo(nb, nb) : WorkInfo(0);

    // larfb:
    {
        // Constants
        const idx_t m = nrows(C);
        const idx_t n = ncols(C);
        const idx_t nA = (side == Side::Left) ? m : n;

        // Empty matrices
        const auto V = slice(A, range{0, nb}, range{0, nA});
        const auto matrixT = slice(A, range{0, nb}, range{0, nb});

        // Internal workspace queries
        workinfo += larfb_worksize<T>(
            side, (trans == Op::NoTrans) ? Op::ConjTrans : Op::NoTrans,
            BACKWARD, ROWWISE_STORAGE, V, matrixT, C);
    }

    return workinfo;
}

/** Applies orthogonal matrix op(Q) to a matrix C using a blocked code.
 *
 * - side = Side::Left  & trans = Op::NoTrans:    $C := Q C$;
 * - side = Side::Right & trans = Op::NoTrans:    $C := C Q$;
 * - side = Side::Left  & trans = Op::ConjTrans:  $C := C Q^H$;
 * - side = Side::Right & trans = Op::ConjTrans:  $C := Q^H C$.
 *
 * The matrix Q is represented as a product of elementary reflectors
 * \[
 *          Q = H_1 H_2 ... H_k,
 * \]
 * where k = min(m,n). Each H_i has the form
 * \[
 *          H_i = I - tau * v * v',
 * \]
 * where tau is a scalar, and v is a vector with
 * \[
 *          v[0] = v[1] = ... = v[i-1] = 0; v[i] = 1,
 * \]
 * with v[i+1] through v[m-1] stored on exit below the diagonal
 * in the ith column of A, and tau in tau[i].
 *
 * @tparam side_t Either Side or any class that implements `operator Side()`.
 * @tparam trans_t Either Op or any class that implements `operator Op()`.
 *
 * @param[in] side Specifies which side op(Q) is to be applied.
 *      - Side::Left:  C := op(Q) C;
 *      - Side::Right: C := C op(Q).
 *
 * @param[in] trans The operation $op(Q)$ to be used:
 *      - Op::NoTrans:      $op(Q) = Q$;
 *      - Op::ConjTrans:    $op(Q) = Q^H$.
 *      Op::Trans is a valid value if the data type of A is real. In this case,
 *      the algorithm treats Op::Trans as Op::ConjTrans.
 *
 * @param[in] A
 *      - side = Side::Left:    k-by-m matrix;
 *      - side = Side::Right:   k-by-n matrix.
 *
 * @param[in] tau Vector of length k
 *      Contains the scalar factors of the elementary reflectors.
 *
 * @param[in,out] C m-by-n matrix.
 *      On exit, C is replaced by one of the following:
 *      - side = Side::Left  & trans = Op::NoTrans:    $C := Q C$;
 *      - side = Side::Right & trans = Op::NoTrans:    $C := C Q$;
 *      - side = Side::Left  & trans = Op::ConjTrans:  $C := C Q^H$;
 *      - side = Side::Right & trans = Op::ConjTrans:  $C := Q^H C$.
 *
 * @param[in] opts Options.
 *
 * @ingroup computational
 */
template <TLAPACK_SMATRIX matrixA_t,
          TLAPACK_SMATRIX matrixC_t,
          TLAPACK_SVECTOR tau_t,
          TLAPACK_SIDE side_t,
          TLAPACK_OP trans_t>
int unmrq(side_t side,
          trans_t trans,
          const matrixA_t& A,
          const tau_t& tau,
          matrixC_t& C,
          const UnmrqOpts<size_type<matrixC_t>>& opts = {})
{
    using TA = type_t<matrixA_t>;
    using idx_t = size_type<matrixC_t>;
    using matrixT_t = matrix_type<matrixA_t, tau_t>;
    using T = type_t<matrixT_t>;

    using range = pair<idx_t, idx_t>;

    // Functor
    Create<matrixT_t> new_matrix;

    // Constants
    const idx_t m = nrows(C);
    const idx_t n = ncols(C);
    const idx_t k = size(tau);
    const idx_t nA = (side == Side::Left) ? m : n;
    const idx_t nb = min<idx_t>(opts.nb, k);

    // check arguments
    tlapack_check_false(side != Side::Left && side != Side::Right);
    tlapack_check_false(trans != Op::NoTrans && trans != Op::Trans &&
                        trans != Op::ConjTrans);
    tlapack_check_false(trans == Op::Trans && is_complex<TA>);

    // quick return
    if ((m == 0) || (n == 0) || (k == 0)) return 0;

    // Allocates workspace
    WorkInfo workinfo = unmrq_worksize<T>(side, trans, A, tau, C, opts);
    std::vector<T> work_;
    auto work = new_matrix(work_, workinfo.m, workinfo.n);
    auto matrixT = slice(work, range{workinfo.m - nb, workinfo.m},
                         range{workinfo.n - nb, workinfo.n});

    // Preparing loop indexes
    const bool positiveInc =
        (((side == Side::Left) && !(trans == Op::NoTrans)) ||
         (!(side == Side::Left) && (trans == Op::NoTrans)));
    const idx_t i0 = (positiveInc) ? 0 : ((k - 1) / nb) * nb;
    const idx_t iN = (positiveInc) ? ((k - 1) / nb + 1) * nb : -nb;
    const idx_t inc = (positiveInc) ? nb : -nb;

    // Main loop
    for (idx_t i = i0; i != iN; i += inc) {
        idx_t ib = min<idx_t>(nb, k - i);
        const auto V = slice(A, range{i, i + ib}, range{0, nA - k + i + ib});
        const auto taui = slice(tau, range{i, i + ib});
        auto matrixTi = slice(matrixT, range{0, ib}, range{0, ib});

        // Form the triangular factor of the block reflector
        // $H = H(i) H(i+1) ... H(i+ib-1)$
        larft(BACKWARD, ROWWISE_STORAGE, V, taui, matrixTi);

        // H or H**H is applied to either C[0:m-k+i+1,0:n] or C[0:m,0:n-k+i+1]
        auto Ci = (side == Side::Left)
                      ? slice(C, range{0, m - k + i + ib}, range{0, n})
                      : slice(C, range{0, m}, range{0, n - k + i + ib});

        // Apply H or H**H
        larfb_work(side, (trans == Op::NoTrans) ? Op::ConjTrans : Op::NoTrans,
                   BACKWARD, ROWWISE_STORAGE, V, matrixTi, Ci, work);
    }

    return 0;
}

}  // namespace tlapack

#endif  // TLAPACK_UNMRQ_HH
