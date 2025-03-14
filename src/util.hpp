#pragma once

#include <array>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <string>
#include <vector>
#include "defs.hpp"

template<typename T, usize N, typename ...Ts>
inline std::array<T, N> array_init(Ts &&...args) {
	return std::apply([&args...](auto... dummy) {return std::array{((void)dummy, T(std::forward<Ts>(args)...))...};}, std::array<int, N>{});
}
constexpr static const char *hex_chars = "0123456789abcdef";

class rw_lock {
public:
	rw_lock();
	void lockr();
	void unlockr();
	void lockw();
	void unlockw();
private:
	std::mutex mut;
	std::condition_variable reader_cond;
	std::condition_variable writer_cond;
	u32 readers;
	bool writer;
	bool writer_wait;
};

std::string utf32_to_utf8(std::u32string_view s);
std::u32string utf8_to_utf32(std::string_view s);

struct cpu_timestamps {
public:
	using clock = std::chrono::high_resolution_clock;
	clock::time_point start_time;
	std::vector<u64> times;

	void start();
	void stamp();
};

std::string format_time(u64 ns);

f32 randf();

