#ifndef DEFER_HPP
#define DEFER_HPP

#include <functional>

struct defer {
	std::function<void()> func;

public:
	defer() = default;
	defer(const defer &) = delete;
	defer(defer &&) = default;
	defer &operator=(const defer &) = delete;
	defer &operator=(defer &&second) noexcept { swap(func, second.func); }
	~defer() {
		if (func)
			func();
	}
	template <typename Fn> defer(Fn f) : func{move(f)} {}
};

#endif /* end of include guard: DEFER_HPP */
