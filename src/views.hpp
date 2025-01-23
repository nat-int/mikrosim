#pragma once
#include <string>
#include <glm/vec4.hpp>
#include "compounds.hpp"
#include "protein.hpp"
#include "cell.hpp"

struct protein_view {
	folder f;
	protein_info info;
	u32 random_len;
	std::string genome;
	std::vector<std::string> generation;
	bool genome_readonly;
	glm::ivec4 reqs;
	bool generating;

	protein_view();
	void set_rand(const compounds &comps, usize len);
	void set(const compounds &comps, const std::string &g);
	void draw(const compounds &comps);
private:
	void set(const compounds &comps, const std::vector<block> &bls);
	void generator_gen(const compounds &comps);
};

struct cell_view {
	cell ext_cell;
	cell *c;
	std::string file_path;
	bool follow;

	cell_view();
	void draw(const compounds &comps, protein_view &pv);
};

