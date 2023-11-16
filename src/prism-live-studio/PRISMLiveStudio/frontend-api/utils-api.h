#pragma once

#include "frontend-api-global.h"

#include <array>

#ifdef Q_OS_WIN
#include <malloc.h>
#else
#include <sys/malloc.h>
#endif

#include <utility>
#include <qobject.h>
#include <map>
#include <qpointer.h>
#include <mutex>
#include <type_traits>
#include <initializer_list>
#include <string.h>

#include "libutils-api.h"

template<typename T> T *pls_remove_const(const T *ptr)
{
	return const_cast<T *>(ptr);
}

template<typename T> T &pls_remove_const(const T &ptr)
{
	return const_cast<T &>(ptr);
}

template<typename Key, typename Value> using std_map = std::map<Key, Value>;

template<typename I, typename Fn> void pls_for(I i, I count, Fn fn)
{
	for (; i < count; ++i) {
		fn(i);
	}
}
template<typename Iter, typename Fn> void pls_for_each(Iter begin, Iter end, Fn fn)
{
	for (int i = 0; begin != end; ++begin, ++i) {
		fn(*begin, i);
	}
}
template<typename Iter, typename Fn> void pls_chk_for_each(Iter begin, Iter end, Fn fn)
{
	for (int i = 0; begin != end; ++begin, ++i) {
		if (auto v = *begin; v) {
			fn(v, i);
		}
	}
}
template<typename Iter, typename Chk, typename Fn> void pls_chk_for_each(Iter begin, Iter end, Chk chk, Fn fn)
{
	for (int i = 0; begin != end; ++begin, ++i) {
		if (auto v = *begin; chk(v)) {
			fn(v, i);
		}
	}
}
template<typename Container, typename Fn> void pls_for_each_brk_locked(std::mutex &mutex, Container &container, Fn fn)
{
	std::lock_guard<std::mutex> lock(mutex);
	auto begin = container.begin();
	auto end = container.end();
	for (int i = 0; begin != end; ++begin, ++i) {
		if (!fn(*begin, i)) {
			break;
		}
	}
}
template<typename Iter, typename Fn> bool pls_contains(Iter begin, Iter end, Fn fn)
{
	for (int i = 0; begin != end; ++begin, ++i) {
		if (fn(*begin, i)) {
			return true;
		}
	}
	return false;
}
template<typename Iter, typename Fn> Iter pls_find(Iter begin, Iter end, Fn fn)
{
	for (int i = 0; begin != end; ++begin, ++i) {
		if (fn(*begin, i)) {
			return begin;
		}
	}
	return end;
}
template<typename Container, typename Fn> typename Container::iterator pls_find(Container &container, Fn fn)
{
	return pls_find(container.begin(), container.end(), fn);
}
template<typename Iter, typename Fn> int pls_find_index(Iter begin, Iter end, Fn fn)
{
	for (int i = 0; begin != end; ++begin, ++i) {
		if (fn(*begin, i)) {
			return i;
		}
	}
	return -1;
}
template<typename Container, typename Fn> int pls_find_index(Container &container, Fn fn)
{
	return pls_find_index(container.begin(), container.end(), fn);
}
template<typename Vector, typename Fn> int pls_find_index(Vector &vec, typename Vector::size_type i, Fn fn)
{
	for (typename Vector::size_type c = vec.count(); i < c; ++i) {
		if (fn(vec[i], i)) {
			return i;
		}
	}
	return -1;
}
template<typename Vector, typename Fn> int pls_count_brk(Vector &vec, typename Vector::size_type i, Fn fn)
{
	int count = 0;
	for (typename Vector::size_type c = vec.count(); i < c; ++i, ++count) {
		if (fn(vec[i], i)) {
			break;
		}
	}
	return count;
}
template<typename Container> void pls_clear_locked(std::mutex &mutex, Container &container)
{
	std::lock_guard<std::mutex> lock(mutex);
	container.clear();
}
template<typename Cb, typename... Args> auto pls_code_block(Cb cb, Args &&...args) -> decltype(std::declval<Cb>()(std::declval<Args>()...))
{
	if constexpr (!std::is_same_v<void, decltype(std::declval<Cb>()(std::declval<Args>()...))>) {
		return cb(std::forward<Args>(args)...);
	} else {
		cb(std::forward<Args>(args)...);
	}
}
template<typename Vector> typename Vector::value_type pls_vec_ptr_at(Vector &vec, typename Vector::size_type i)
{
	if ((i >= 0) && (i < vec.size())) {
		return vec[i];
	}
	return nullptr;
}

inline bool pls_is_or(bool c1, bool c2)
{
	return c1 || c2;
}
inline bool pls_is_or(bool c1, bool c2, bool c3)
{
	return c1 || c2 || c3;
}
inline bool pls_is_or(bool c1, bool c2, bool c3, bool c4)
{
	return c1 || c2 || c3 || c4;
}
inline bool pls_is_or(bool c1, bool c2, bool c3, bool c4, bool c5)
{
	return c1 || c2 || c3 || c4 || c5;
}
inline bool pls_is_and(bool c1, bool c2)
{
	return c1 && c2;
}
inline bool pls_is_and(bool c1, bool c2, bool c3)
{
	return c1 && c2 && c3;
}
inline bool pls_is_and(bool c1, bool c2, bool c3, bool c4)
{
	return c1 && c2 && c3 && c4;
}
inline bool pls_is_and(bool c1, bool c2, bool c3, bool c4, bool c5)
{
	return c1 && c2 && c3 && c4 && c5;
}
template<typename T, typename Fn> inline bool pls_chk_ptr_invoke(Fn fn, T ptr)
{
	if (ptr)
		return fn(ptr);
	return false;
}
template<typename T, typename... Values> inline bool pls_is_in(const T &origin, Values &&...values)
{
	for (const T &value : pls_make_array<T>(std::forward<Values>(values)...)) {
		if (origin == value) {
			return true;
		}
	}
	return false;
}
template<typename T, typename IsEqual, typename... Values> inline bool pls_is_in(IsEqual is_equal, const T &origin, Values &&...values)
{
	for (const T &value : pls_make_array<T>(std::forward<Values>(values)...)) {
		if (is_equal(origin, value)) {
			return true;
		}
	}
	return false;
}
template<typename... Values> inline bool pls_is_in_str(const char *origin, Values &&...values)
{
	bool (*is_equal)(const char *, const char *) = &pls_is_equal;
	return pls_is_in<const char *>(is_equal, origin, std::forward<Values>(values)...);
}
template<typename Fn> inline void pls_conditional_invoke(bool condition, Fn fn)
{
	if (condition) {
		fn();
	}
}
template<typename R, typename Fn> inline void pls_conditional_invoke(R &r, bool condition, Fn fn)
{
	if (condition) {
		r = fn();
	}
}
template<typename T> inline bool pls_is_visible(const T *widget)
{
	return widget ? widget->isVisible() : false;
}
template<typename T> inline void pls_set_visible(T *widget, bool visible)
{
	if (widget) {
		widget->setVisible(visible);
	}
}
template<typename T> inline int pls_get_visible_width(const T *widget, int def_width = 0)
{
	return pls_is_visible(widget) ? widget->width() : def_width;
}
template<typename T> inline bool pls_is_enabled(const T *widget)
{
	return widget ? widget->isEnabled() : false;
}
template<typename T> inline void pls_set_enabled(T *widget, bool enabled)
{
	if (widget) {
		widget->setEnabled(enabled);
	}
}
template<typename T> inline T pls_set_value(T &var, const T &value)
{
	var = value;
	return value;
}
