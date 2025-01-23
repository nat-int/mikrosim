#include "util.hpp"

#include <bit>

rw_lock::rw_lock() : readers(0), writer(false), writer_wait(false) {}
void rw_lock::lockr() {
	std::unique_lock<std::mutex> lock(mut);
	reader_cond.wait(lock, [this] { return writer == 0 && writer_wait == 0; });
	readers++;
}
void rw_lock::unlockr() {
	std::lock_guard<std::mutex> lock(mut);
	if (--readers == 0 && writer_wait) {
		writer_cond.notify_one();
	}
}
void rw_lock::lockw() {
	std::unique_lock<std::mutex> lock(mut);
	writer_wait = true;
	writer_cond.wait(lock, [this] { return readers == 0 && !writer; });
	writer = true;
	writer_wait = false;
}
void rw_lock::unlockw() {
	std::lock_guard<std::mutex> lock(mut);
	writer = false;
	reader_cond.notify_all();
	writer_cond.notify_one();
}
std::string utf32_to_utf8(std::u32string_view s) {
	std::string out; out.reserve(s.size());
	for (char32_t cc : s) {
		u32 c = cc;
		if (c < 0x80) {
			out.push_back(char(c));
		} else if (c < 0x800) {
			out.push_back(char(u8(0xc0 | c >> 6)));
			out.push_back(char(u8(0x80 | (c & 0x3f))));
		} else if (c < 0x10000) {
			out.push_back(char(u8(0xe0 | c >> 12)));
			out.push_back(char(u8(0x80 | (c >> 6 & 0x3f))));
			out.push_back(char(u8(0x80 | (c & 0x3f))));
		} else {
			u32 ip = u32(out.size());
			u8 bl = 5;
			while (c >> bl) {
				out.insert(ip, 1, char(u8(0x80 | (c & 0x3f))));
				c >>= 6;
				bl--;
			}
			out.insert(ip, 1, char(u8(0xffu << (bl+1) | c)));
		}
	}
	return out;
}
std::u32string utf8_to_utf32(std::string_view s) {
	std::u32string out; out.reserve(s.size() / 2);
	u32 buf = 0;
	for (char _c : s) {
		u8 c = u8(_c);
		if (c < 0x80) {
			if (buf) {
				out.push_back(buf);
				buf = 0;
			}
			out.push_back(c);
		} else {
			u8 bl = u8(std::countl_one(c));
			buf = buf << (7 - bl) | ((0xff >> bl) & c);
		}
	}
	return out;
}

void cpu_timestamps::start() {
	times.clear();
	start_time = clock::now();
}
void cpu_timestamps::stamp() {
	times.push_back(u64(std::chrono::duration_cast<std::chrono::nanoseconds>(clock::now() - start_time).count()));
}

std::string format_time(u64 ns) {
	constexpr static const char *pfx[] = { "ns", "us", "ms", "s", "min", "h", "d", "y" };
	constexpr static const u64 factors[] = { 1000, 1000, 1000, 60, 60, 24, 365, 0xffffffffu };
	usize i = 0;
	for (; ns >= factors[i] * 3; i++) { ns /= factors[i]; }
	return std::to_string(ns) + pfx[i];
}

f32 randf() {
	if constexpr (RAND_MAX > 0xffffu) { return (u32(rand()) % 0xffffu) / f32(0xffffu); }
	else { return f32(rand()) / f32(RAND_MAX); }
}

