#pragma once
#include <array>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <string_view>
#include <vector>
#include <glm/gtx/string_cast.hpp>

namespace logs {
	namespace {
		inline constexpr const char *pre_dbg = " \x1b[105m\x1b[30m DEB  \x1b[0m";
		inline constexpr const char *pre_info = " \x1b[107m\x1b[30m INFO \x1b[0m";
		inline constexpr const char *pre_warn = " \x1b[103m\x1b[30m WARN \x1b[0m";
		inline constexpr const char *pre_error = " \x1b[101m\x1b[30m ERR  \x1b[0m";

		template<typename T>
		inline void log_helper(T val) {
			std::cout << val;
		}
		template<typename TI>
		inline void log_helper_range(TI begin, TI end) {
			std::cout << "[ ";
			bool first = true;
			for (; begin != end; begin++) {
				if (first) {
					first = false;
				} else {
					std::cout << ", ";
				}
				log_helper(*begin);
			}
			std::cout << " ]";
		}
		template<size_t L, typename T, glm::qualifier Q> inline void log_helper(const glm::vec<L, T, Q> &val) { std::cout << glm::to_string(val); }
		template<size_t C, size_t R, typename T, glm::qualifier Q> inline void log_helper(const glm::mat<C, R, T, Q> &val) { std::cout << glm::to_string(val); }
		template<typename T, glm::qualifier Q> inline void log_helper(const glm::qua<T, Q> &val) { std::cout << glm::to_string(val); }
		template<typename T, size_t N> inline void log_helper(const std::array<T, N> &arr) { log_helper_range(arr.begin(), arr.end()); }
		template<typename T> inline void log_helper(const std::vector<T> &vec) { log_helper_range(vec.begin(), vec.end()); }
		template<typename ...Ts>
		inline void log(std::string_view name, Ts ...vals) {
			std::cout << " \x1b[92m[" << name << "]\x1b[0m ";
			(log_helper(vals), ...);
		}
		inline void log_time() {
			auto t = std::time(nullptr);
			auto tm = *std::localtime(&t);
			std::cout << "\x1b[92m" << std::put_time(&tm, "%d.%m.%Y %H:%M:%S");
		}
	}
	template<typename ...Ts> inline void debug(std::string_view name, Ts ...vals)  { log_time(); std::cout << pre_dbg; log(name, vals...); }
	template<typename ...Ts> inline void debugln(std::string_view name, Ts ...vals){ log_time(); std::cout << pre_dbg; log(name, vals...); std::cout << std::endl; }
	template<typename ...Ts> inline void info(std::string_view name, Ts ...vals)   { log_time(); std::cout << pre_info; log(name, vals...); }
	template<typename ...Ts> inline void infoln(std::string_view name, Ts ...vals) { log_time(); std::cout << pre_info; log(name, vals...); std::cout << std::endl; }
	template<typename ...Ts> inline void warn(std::string_view name, Ts ...vals)   { log_time(); std::cout << pre_warn; log(name, vals...); }
	template<typename ...Ts> inline void warnln(std::string_view name, Ts ...vals) { log_time(); std::cout << pre_warn; log(name, vals...); std::cout << std::endl; }
	template<typename ...Ts> inline void error(std::string_view name, Ts ...vals)  { log_time(); std::cout << pre_error; log(name, vals...); }
	template<typename ...Ts> inline void errorln(std::string_view name, Ts ...vals){ log_time(); std::cout << pre_error; log(name, vals...); std::cout << std::endl; }
}
