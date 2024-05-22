#pragma once

#include <type_traits>
#include <string>

template <typename T, typename... U>
struct is_any_of : public std::disjunction<std::is_same<T, U>...>
{};
template <typename T, typename... U>
inline constexpr bool is_any_of_v = is_any_of<T, U...>::value;

template <typename T, typename... U>
struct is_same_all : public std::is_same<std::is_same<T, U>...>
{};
template <typename T, typename... U>
inline constexpr bool is_same_all_v = is_same_all<T, U...>::value;

template <typename Fn, typename... Args>
inline constexpr bool is_void_result_v = std::is_same_v<void, std::invoke_result_t<Fn, Args...>>;

template <typename T, typename = void, typename = void>
struct is_qstring_like : public std::false_type
{};
template <typename T>
struct is_qstring_like<T, std::void_t<decltype(std::declval<T>().fromStdString(std::string{}))>, std::void_t<decltype(std::declval<T>().toStdString())>> : public std::true_type {};
template <class QStringLike>
inline constexpr bool is_qstring_like_v = is_qstring_like<QStringLike>::value;

#ifdef __CXX20
template <typename T>
concept QStringLike = requires(T a) {
  { a.toStdString() } -> std::convertible_to<std::string>;
  { T::fromStdString(std::string{}) } -> std::same_as<void>;
};
#endif

template <typename T>
struct is_numeric : public std::enable_if<std::is_integral_v<T> || std::is_floating_point_v<T>>
{};
template <typename T>
inline constexpr bool is_numeric_v = is_numeric<T>::value;
