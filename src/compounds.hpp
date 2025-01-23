#pragma once
#include <array>
#include <bitset>
#include "cmake_defs.hpp"
#include "defs.hpp"

struct compound {
	u8 atoms;

	compound(u8 a);
	compound(u8 a, u8 b, u8 c, u8 d);
	u8 rot(u8 amount=1) const;
};
struct compound_info {
	u8 parts[4];
	u16 energy;

	void imgui(f32 sz=16.f, f32 w=5.f, f32 sp=3.f) const;
};

class compounds {
public:
	static constexpr usize count = 70;
	std::array<u8, 4*4*4*4> atoms_to_id;
	std::array<compound_info, count> infos;
private:
	std::array<std::array<f32, compile_options::particle_count>, count> concentrations;
public:
	std::bitset<count> locked;

	compounds();
	f32 &at(usize compound, usize particle);
};

