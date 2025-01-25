#pragma once
#include <algorithm>
#include <bit>
#include <variant>
#include "defs.hpp"

class compounds;

constexpr u8 g0_comp = 0b00'01'11'01;
constexpr u8 g1_comp = 0b01'10'01'11;
constexpr u8 start_quad = 0b1010;
constexpr u8 stop_quad = 0b1110; // should be one of the double-covered blocks
constexpr const char *quads_chr = "0123456789abcdef";
constexpr const char *quads_chr_nostop = "0123456789abcdf";
inline constexpr u8 chr_to_quad(char c) {
	return c >= 'a' ? u8(c - ('a' - 10)) : u8(c - '0');
}
struct block {
	u8 atoms;
	/// 0 ~ straight, -1 ~ ccw rotation, 1 ~ cw rotation
	i8 shape;
};
constexpr inline block blocks[] = {
	{0b11'00'11'10, 0},
	{0b01'00'11'10, 0},
	{0b00'00'11'10, 0},
	{0b11'00'10'10, 0},
	{0b01'00'01'10, 0},
	{0b01'00'00'10, 0},
	{0b10'00'01'10, 0},
	{0b00'11'11'10, -1},
	{0b00'01'11'10, -1},
	{0b00'11'01'10, -1},
	{0b00'01'01'10, -1},
	{0b11'11'00'10, 1},
	{0b11'01'00'10, 1},
	{0b01'01'00'10, 1},
};
constexpr inline usize block_count = sizeof(blocks)/sizeof(blocks[0]);
constexpr inline u8 min_rot(u8 x) {
	return std::min({x, std::rotr(x, 2), std::rotr(x, 4), std::rotr(x, 6)});
}
constexpr inline u8 block_compounds[] = {
	min_rot(blocks[0].atoms), min_rot(blocks[1].atoms), min_rot(blocks[2].atoms),
	min_rot(blocks[3].atoms), min_rot(blocks[4].atoms), min_rot(blocks[5].atoms),
	min_rot(blocks[6].atoms), min_rot(blocks[7].atoms), min_rot(blocks[8].atoms),
	min_rot(blocks[9].atoms), min_rot(blocks[10].atoms), min_rot(blocks[11].atoms),
	min_rot(blocks[12].atoms), min_rot(blocks[13].atoms),
};

struct catalyzer { f32 effect; u8 compound; };
struct protein_info {
	std::vector<catalyzer> catalyzers;
	std::vector<u8> reaction_input;
	std::vector<u8> reaction_output;
	std::vector<bool> genome_binder;
	bool is_genome_polymerase;
	bool is_genome_repair;
	bool is_positive_factor;
	i32 energy_balance;
};

class folder {
	usize width, height;
	std::vector<std::vector<u8>> placed;

	std::vector<u8> &at(i32 x, i32 y);
	const std::vector<u8> &at(i32 x, i32 y) const;
public:
	void place(const std::vector<block> &chain);
	void replace(const std::vector<block> &chain);
	protein_info analyze(const compounds &comp) const;
	void draw() const;
};

struct empty_protein { };
struct chem_protein {
	std::vector<u8> inputs;
	std::vector<u8> outputs;
	f32 K;
};
struct transcription_factor {
	std::vector<usize> bind_points;
	bool positive;
	f32 curr_effect;
};
enum class special_action { division, repair };
struct special_chem_protein : public chem_protein {
	special_action act;
	i32 energy_balance;
};

struct protein {
	std::vector<catalyzer> catalyzers;
	using effect_t = std::variant<empty_protein, chem_protein, transcription_factor, special_chem_protein>;
	effect_t effect;
	usize genome_start;
	usize genome_end;
	std::array<f32, block_count> cost;
	f32 conc;
	bool direct_transcription;
};

