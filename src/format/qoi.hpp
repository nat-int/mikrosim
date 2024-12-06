#pragma once
#include <istream>
#include <vector>
#include "../defs.hpp"
#include "defs.hpp"

namespace format::qoi {
	constexpr magic_t magic = { 'q', 'o', 'i', 'f' };
	enum channels_count : u8 {
		channels_rgb = 3,
		channels_rgba = 4,
	};
	enum colorspaces : u8 {
		colorpsace_srgb_alpha_linear = 0,
		colorspace_linear = 1,
	};
	struct header {
		magic_t magic;
		u32 width;
		u32 height;
		u8 channels;
		u8 colorspace;
	};
	class decoder {
	public:
		u32 width;
		u32 height;
		channels_count channels;
		colorspaces colorspace;
		std::vector<u8> raw_data;
		
		decoder(const header *header_data);
		void decode(const u8 *data, usize nbytes);
		void decode(std::istream &is);
		void rgb(u8 r, u8 g, u8 b);
		void rgba(u8 r, u8 g, u8 b, u8 a);
		void index(u8 j);
		void diff(u8 d);
		void luma(u8 d, u8 d2);
		void run(u8 l);
	private:
		color_t last;
		std::array<color_t, 64> table;
		usize i;
		u8 state;
		std::array<u8, 3> sdata;

		void push();
	};
}
