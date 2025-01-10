#pragma once
#include <array>
#include <bitset>
#include "cmake_defs.hpp"
#include "defs.hpp"

class compounds {
public:
	static constexpr usize count = 4*4*4*4;
private:
	std::array<std::array<f32, compile_options::particle_count>, count> concentrations;
public:
	std::bitset<count> locked;

	f32 &at(usize compound, usize particle);
};

