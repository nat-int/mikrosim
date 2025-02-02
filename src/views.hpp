#pragma once
#include <string>
#include <glm/vec2.hpp>
#include <glm/vec4.hpp>
#include "compounds.hpp"
#include "protein.hpp"
#include "cell.hpp"

namespace input { class input_handler; };
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

struct metabolism_subview {
	struct node {
		enum class kind {
			compound,
			protein
		};
		glm::vec2 pos;
		usize id;
		kind k;
	};
	struct edge {
		usize a;
		usize b;
		ImU32 col;
	};
	std::vector<node> nodes;
	std::vector<edge> edges;
	glm::vec2 view_pos;
	f32 view_scale;
	usize drag;
	bool lock;

	void set(const cell &c);
	void update();
	void draw(const input::input_handler &inp, const compounds &comps);
private:
	void add_prot(usize id, const std::vector<u8> &inp, const std::vector<u8> &out, bool spec);
	void add_comp_edge(usize from, bool rev, u8 comp, ImU32 col);
};
struct cell_view {
	cell ext_cell;
	cell *c;
	std::string file_path;
	std::string save_file_path;
	bool follow;
	metabolism_subview msv;

	cell_view();
	void set(cell *_c);
	void draw(const input::input_handler &inp, const compounds &comps, protein_view &pv);
};

