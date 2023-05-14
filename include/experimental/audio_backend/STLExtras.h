//===- llvm/ADT/STLExtras.h - Useful STL related functions ------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// This file contains some templates that are useful if you are working with
/// the STL at all.
///
/// No library is required when using these functions.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_ADT_STLEXTRAS_H
#define LLVM_ADT_STLEXTRAS_H

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <initializer_list>
#include <iterator>
#include <limits>
#include <memory>
#include <optional>
#include <tuple>
#include <type_traits>
#include <utility>

namespace llvm {

/// This class provides various trait information about a callable object.
///   * To access the number of arguments: Traits::num_args
///   * To access the type of an argument: Traits::arg_t<Index>
///   * To access the type of the result:  Traits::result_t
template <typename T, bool isClass = std::is_class<T>::value>
struct function_traits : public function_traits<decltype(&T::operator())> {};

/// Overload for class function types.
template <typename ClassType, typename ReturnType, typename... Args>
struct function_traits<ReturnType (ClassType::*)(Args...) const, false> {
  /// The number of arguments to this function.
  enum { num_args = sizeof...(Args) };

  /// The result type of this function.
  using result_t = ReturnType;

  /// The type of an argument to this function.
  template <size_t Index>
  using arg_t = std::tuple_element_t<Index, std::tuple<Args...>>;
};
/// Overload for class function types.
template <typename ClassType, typename ReturnType, typename... Args>
struct function_traits<ReturnType (ClassType::*)(Args...), false>
    : public function_traits<ReturnType (ClassType::*)(Args...) const> {};
/// Overload for non-class function types.
template <typename ReturnType, typename... Args>
struct function_traits<ReturnType (*)(Args...), false> {
  /// The number of arguments to this function.
  enum { num_args = sizeof...(Args) };

  /// The result type of this function.
  using result_t = ReturnType;

  /// The type of an argument to this function.
  template <size_t i>
  using arg_t = std::tuple_element_t<i, std::tuple<Args...>>;
};
template <typename ReturnType, typename... Args>
struct function_traits<ReturnType (*const)(Args...), false>
    : public function_traits<ReturnType (*)(Args...)> {};
/// Overload for non-class function type references.
template <typename ReturnType, typename... Args>
struct function_traits<ReturnType (&)(Args...), false>
    : public function_traits<ReturnType (*)(Args...)> {};

/// traits class for checking whether type T is one of any of the given
/// types in the variadic list.
template <typename T, typename... Ts>
using is_one_of = std::disjunction<std::is_same<T, Ts>...>;

/// traits class for checking whether type T is a base class for all
///  the given types in the variadic list.
template <typename T, typename... Ts>
using are_base_of = std::conjunction<std::is_base_of<T, Ts>...>;

namespace detail {
template <typename T, typename... Us> struct TypesAreDistinct;
template <typename T, typename... Us>
struct TypesAreDistinct
    : std::integral_constant<bool, !is_one_of<T, Us...>::value &&
                                       TypesAreDistinct<Us...>::value> {};
template <typename T> struct TypesAreDistinct<T> : std::true_type {};
} // namespace detail

/// Determine if all types in Ts are distinct.
///
/// Useful to statically assert when Ts is intended to describe a non-multi set
/// of types.
///
/// Expensive (currently quadratic in sizeof(Ts...)), and so should only be
/// asserted once per instantiation of a type which requires it.
template <typename... Ts> struct TypesAreDistinct;
template <> struct TypesAreDistinct<> : std::true_type {};
template <typename... Ts>
struct TypesAreDistinct
    : std::integral_constant<bool, detail::TypesAreDistinct<Ts...>::value> {};

/// Find the first index where a type appears in a list of types.
///
/// FirstIndexOfType<T, Us...>::value is the first index of T in Us.
///
/// Typically only meaningful when it is otherwise statically known that the
/// type pack has no duplicate types. This should be guaranteed explicitly with
/// static_assert(TypesAreDistinct<Us...>::value).
///
/// It is a compile-time error to instantiate when T is not present in Us, i.e.
/// if is_one_of<T, Us...>::value is false.
template <typename T, typename... Us> struct FirstIndexOfType;
template <typename T, typename U, typename... Us>
struct FirstIndexOfType<T, U, Us...>
    : std::integral_constant<size_t, 1 + FirstIndexOfType<T, Us...>::value> {};
template <typename T, typename... Us>
struct FirstIndexOfType<T, T, Us...> : std::integral_constant<size_t, 0> {};

/// Find the type at a given index in a list of types.
///
/// TypeAtIndex<I, Ts...> is the type at index I in Ts.
template <size_t I, typename... Ts>
using TypeAtIndex = std::tuple_element_t<I, std::tuple<Ts...>>;

} // namespace llvm

#endif // LLVM_ADT_STLEXTRAS_H
