#pragma once

#include <tcb/span.hpp>

template <typename T>
using ArrayView = tcb::span<T, tcb::dynamic_extent>;

template <typename T>
constexpr auto MakeView(T&& v)
{
	using ValueType = typename std::decay_t<T>::value_type;
	using Ret = std::conditional_t<std::is_const_v<std::remove_reference_t<T>>, const ValueType, ValueType>;
	return ArrayView<Ret>{ v.data(), v.size() };
}