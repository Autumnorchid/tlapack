/// @file plugins/starpu.hpp
/// @author Weslley S Pereira, University of Colorado Denver, USA
//
// Copyright (c) 2021-2023, University of Colorado Denver. All rights reserved.
//
// This file is part of <T>LAPACK.
// <T>LAPACK is free software: you can redistribute it and/or modify it under
// the terms of the BSD 3-Clause license. See the accompanying LICENSE file.

#ifndef TLAPACK_STARPU_HH
#define TLAPACK_STARPU_HH

#include "tlapack/base/arrayTraits.hpp"
#include "tlapack/base/workspace.hpp"
#include "tlapack/starpu/Matrix.hpp"

namespace tlapack {

template <class real_t, std::enable_if_t<!is_complex<real_t>::value, int> = 0>
constexpr real_t real(const real_t&);
template <class real_t, std::enable_if_t<!is_complex<real_t>::value, int> = 0>
constexpr real_t imag(const real_t&);
template <class real_t, std::enable_if_t<!is_complex<real_t>::value, int> = 0>
constexpr real_t conj(const real_t&);

template <class T>
inline constexpr real_type<T> real(const starpu::internal::data<T>& x)
{
    return real(T(x));
}
template <class T>
inline constexpr real_type<T> imag(const starpu::internal::data<T>& x)
{
    return imag(T(x));
}
template <class T>
inline constexpr T conj(const starpu::internal::data<T>& x)
{
    return conj(T(x));
}

}  // namespace tlapack

namespace tlapack {

// -----------------------------------------------------------------------------
// Data descriptors

// Number of rows
template <class T>
constexpr auto nrows(const starpu::Matrix<T>& A)
{
    return A.nrows();
}
// Number of columns
template <class T>
constexpr auto ncols(const starpu::Matrix<T>& A)
{
    return A.ncols();
}
// Size
template <class T>
constexpr auto size(const starpu::Matrix<T>& A)
{
    return A.nrows() * A.ncols();
}

// -----------------------------------------------------------------------------
// Block operations for starpu::Matrix

#define isSlice(SliceSpec) \
    std::is_convertible<SliceSpec, std::tuple<uint32_t, uint32_t>>::value

template <
    class T,
    class SliceSpecRow,
    class SliceSpecCol,
    typename std::enable_if<isSlice(SliceSpecRow) && isSlice(SliceSpecCol),
                            int>::type = 0>
constexpr auto slice(const starpu::Matrix<T>& A,
                     SliceSpecRow&& rows,
                     SliceSpecCol&& cols)
{
    const uint32_t row0 = std::get<0>(rows);
    const uint32_t col0 = std::get<0>(cols);
    const uint32_t row1 = std::get<1>(rows);
    const uint32_t col1 = std::get<1>(cols);
    const uint32_t nrows = row1 - row0;
    const uint32_t ncols = col1 - col0;

    const uint32_t mb = A.nblockrows();
    const uint32_t nb = A.nblockcols();

    tlapack_check(row0 % mb == 0);
    tlapack_check(col0 % nb == 0);
    tlapack_check((nrows % mb == 0) ||
                  (row1 == A.nrows() && ((nrows - 1) % mb == 0)));
    tlapack_check((ncols % nb == 0) ||
                  (col1 == A.ncols() && ((ncols - 1) % nb == 0)));

    return A.get_const_tiles(
        row0 / mb, col0 / nb,
        (nrows == 0) ? 0 : std::max<uint32_t>(nrows / mb, 1),
        (ncols == 0) ? 0 : std::max<uint32_t>(ncols / nb, 1));
}
template <
    class T,
    class SliceSpecRow,
    class SliceSpecCol,
    typename std::enable_if<isSlice(SliceSpecRow) && isSlice(SliceSpecCol),
                            int>::type = 0>
constexpr auto slice(starpu::Matrix<T>& A,
                     SliceSpecRow&& rows,
                     SliceSpecCol&& cols)
{
    const uint32_t row0 = std::get<0>(rows);
    const uint32_t col0 = std::get<0>(cols);
    const uint32_t row1 = std::get<1>(rows);
    const uint32_t col1 = std::get<1>(cols);
    const uint32_t nrows = row1 - row0;
    const uint32_t ncols = col1 - col0;

    const uint32_t mb = A.nblockrows();
    const uint32_t nb = A.nblockcols();

    tlapack_check(row0 % mb == 0);
    tlapack_check(col0 % nb == 0);
    tlapack_check((nrows % mb == 0) ||
                  (row1 == A.nrows() && ((nrows - 1) % mb == 0)));
    tlapack_check((ncols % nb == 0) ||
                  (col1 == A.ncols() && ((ncols - 1) % nb == 0)));

    return A.get_tiles(row0 / mb, col0 / nb,
                       (nrows == 0) ? 0 : std::max<uint32_t>(nrows / mb, 1),
                       (ncols == 0) ? 0 : std::max<uint32_t>(ncols / nb, 1));
}

#undef isSlice

template <class T, class SliceSpec>
constexpr auto slice(const starpu::Matrix<T>& v,
                     SliceSpec&& range,
                     uint32_t colIdx)
{
    return slice(v, std::forward<SliceSpec>(range),
                 std::make_tuple(colIdx, colIdx + 1));
}
template <class T, class SliceSpec>
constexpr auto slice(starpu::Matrix<T>& v, SliceSpec&& range, uint32_t colIdx)
{
    return slice(v, std::forward<SliceSpec>(range),
                 std::make_tuple(colIdx, colIdx + 1));
}

template <class T, class SliceSpec>
constexpr auto slice(const starpu::Matrix<T>& v,
                     uint32_t rowIdx,
                     SliceSpec&& range)
{
    return slice(v, std::make_tuple(rowIdx, rowIdx + 1),
                 std::forward<SliceSpec>(range));
}
template <class T, class SliceSpec>
constexpr auto slice(starpu::Matrix<T>& v, uint32_t rowIdx, SliceSpec&& range)
{
    return slice(v, std::make_tuple(rowIdx, rowIdx + 1),
                 std::forward<SliceSpec>(range));
}

template <class T, class SliceSpec>
constexpr auto slice(const starpu::Matrix<T>& v, SliceSpec&& range)
{
    assert((v.nrows() <= 1 || v.ncols() <= 1) && "Matrix is not a vector");

    if (v.nrows() > 1)
        return slice(v, std::forward<SliceSpec>(range), std::make_tuple(0, 1));
    else
        return slice(v, std::make_tuple(0, 1), std::forward<SliceSpec>(range));
}
template <class T, class SliceSpec>
constexpr auto slice(starpu::Matrix<T>& v, SliceSpec&& range)
{
    assert((v.nrows() <= 1 || v.ncols() <= 1) && "Matrix is not a vector");

    if (v.nrows() > 1)
        return slice(v, std::forward<SliceSpec>(range), std::make_tuple(0, 1));
    else
        return slice(v, std::make_tuple(0, 1), std::forward<SliceSpec>(range));
}

template <class T>
constexpr auto col(const starpu::Matrix<T>& A, uint32_t colIdx)
{
    return slice(A, std::make_tuple(0, A.nrows()),
                 std::make_tuple(colIdx, colIdx + 1));
}
template <class T>
constexpr auto col(starpu::Matrix<T>& A, uint32_t colIdx)
{
    return slice(A, std::make_tuple(0, A.nrows()),
                 std::make_tuple(colIdx, colIdx + 1));
}

template <class T, class SliceSpec>
constexpr auto cols(const starpu::Matrix<T>& A, SliceSpec&& cols)
{
    return slice(A, std::make_tuple(0, A.nrows()),
                 std::forward<SliceSpec>(cols));
}
template <class T, class SliceSpec>
constexpr auto cols(starpu::Matrix<T>& A, SliceSpec&& cols)
{
    return slice(A, std::make_tuple(0, A.nrows()),
                 std::forward<SliceSpec>(cols));
}

template <class T>
constexpr auto row(const starpu::Matrix<T>& A, uint32_t rowIdx)
{
    return slice(A, std::make_tuple(rowIdx, rowIdx + 1),
                 std::make_tuple(0, A.ncols()));
}
template <class T>
constexpr auto row(starpu::Matrix<T>& A, uint32_t rowIdx)
{
    return slice(A, std::make_tuple(rowIdx, rowIdx + 1),
                 std::make_tuple(0, A.ncols()));
}

template <class T, class SliceSpec>
constexpr auto rows(const starpu::Matrix<T>& A, SliceSpec&& rows)
{
    return slice(A, std::forward<SliceSpec>(rows),
                 std::make_tuple(0, A.ncols()));
}
template <class T, class SliceSpec>
constexpr auto rows(starpu::Matrix<T>& A, SliceSpec&& rows)
{
    return slice(A, std::forward<SliceSpec>(rows),
                 std::make_tuple(0, A.ncols()));
}

}  // namespace tlapack

#endif  // TLAPACK_STARPU_HH
