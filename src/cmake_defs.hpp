#pragma once

#include "defs.hpp"

namespace version {
	constexpr u32 major = 0;
	constexpr u32 minor = 1;
	constexpr u32 patch = 0;
	constexpr u32 tweak = 0;

	constexpr u32 combined = major << 24 | minor << 16 | patch << 8 | tweak;
}
namespace compile_options {
	constexpr bool vulkan_validation = 1;
	constexpr bool vulkan_validation_supress_loader_info = 1;
	constexpr bool vulkan_validation_abort = 0;
	constexpr u32 frames_in_flight = 2;
	constexpr u32 particle_count = 512;
	constexpr u32 cells_x = 10;
	constexpr u32 cells_y = 10;
#define WINDOW_CALLBACK_GENERATOR(name) on_##name
}

