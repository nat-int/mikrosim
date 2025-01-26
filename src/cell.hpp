#pragma once
#include <vector>
#include <glm/vec2.hpp>
#include "defs.hpp"
#include "protein.hpp"

class compounds;

class cell {
public:
	enum class state { none, active };

	state s;
	u32 gpu_id;
	glm::vec2 pos;
	glm::vec2 vel;
	std::vector<bool> genome;
	std::vector<bool> division_genome;
	usize division_pos;
	std::vector<f32> bound_factors;
	std::vector<protein> proteins;
	usize next_update;
	usize next_create;
	u32 health;

	cell();
	cell(u32 gi, glm::vec2 p, glm::vec2 v);
	void update(compounds &comps, bool protein_create);
	bool add_protein(const compounds &comps, usize s, bool direct_transcription);
	bool add_protein_rec(const compounds &comps, usize s, bool direct_transcription, u8 max_depth=4);
	void analyze(const compounds &comps);
	u8 genome_quad(usize i) const;
	void test();
private:
	void create_tick(compounds &comps, protein &prot);
	void update_tick(compounds &comps, protein &prot);
	f32 reaction_delta(compounds &comps, const protein &prot, f32 cata_effect, f32 K,
		const std::vector<u8> &inp, const std::vector<u8> &out) const;
};

