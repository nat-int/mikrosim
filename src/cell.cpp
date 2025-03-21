#include "cell.hpp"
#include <algorithm>
#include <limits>
#include <boost/math/tools/roots.hpp>
#include <boost/math/tools/cubic_roots.hpp>
#include <boost/math/tools/quartic_roots.hpp>
#include <glm/exponential.hpp>
#include "compounds.hpp"
#include "log/log.hpp"
#include "util.hpp"

static constexpr u32 max_health = 1000;

cell::cell() : s(state::none), membrane_particles(0) { }
cell::cell(u32 gi, glm::vec2 p, glm::vec2 v) : s(state::active), gpu_id(gi), pos(p), vel(v),
	division_pos(0), next_update(0), next_create(0), health(max_health), membrane_particles(0),
	big_struct_id(u8(-1)), small_struct(0), small_struct_effective(0), big_struct(0),
	big_struct_effective(0), membrane_add(0), flagellum_add(0), membrane_structs(0),
	structs_used(0), big_struct_force(0.f, 0.f), net_movement(0.f) { }
template<class... Ts> struct overloaded : Ts... { using Ts::operator()...; };
template<class... Ts> overloaded(Ts...) -> overloaded<Ts...>;
void cell::update(compounds &comps, bool protein_create) {
	health--;
	if (membrane_structs < 4 && health > 0)
		health--;
	if (proteins.empty()) return;
	if (protein_create) {
		create_tick(comps, proteins[next_create]);
		next_create = (next_create + 1) % proteins.size();
	} else {
		update_tick(comps, proteins[next_update]);
		next_update = (next_update + 1) % proteins.size();
	}
	if (big_struct_id != u8(-1)) {
		glm::vec2 r = big_struct_pos - pos;
		if (glm::dot(r, r) < 5.f) {
			big_struct_force = net_movement * glm::normalize(glm::vec2(-r.y, r.x));
		} else {
			big_struct_force = glm::vec2(0.f, 0.f);
		}
		net_movement *= .999f;
	} else {
		big_struct_force = glm::vec2(0.f, 0.f);
	}
}
bool cell::add_protein(const compounds &comps, usize s, bool direct_transcription) {
	usize blocks_boundary;
	for (usize i = s; ; i++) {
		if (i >= genome.size() - 3) return false;
		if (genome_quad(i) == start_quad) {
			blocks_boundary = i;
			break;
		}
	}
	std::array<f32, block_count> cost{};
	std::vector<block> bls;
	for (usize i = blocks_boundary; ; i += 4) {
		if (i >= genome.size() - 3) return false;
		u8 q = genome_quad(i);
		bls.push_back(blocks[q % block_count]);
		cost[q % block_count] += 1.f / 1024.f;
		if (q == stop_quad) {
			blocks_boundary = i+4;
			break;
		}
	}
	folder f;
	f.place(bls);
	const auto info = f.analyze(comps);
	u32 loc = info.end_marker;
	protein::effect_t e;
	if (!info.genome_binder.empty()) {
		std::vector<usize> bind_points;
		for (usize i = 0; i < genome.size() - info.genome_binder.size(); i++) {
			if (std::equal(info.genome_binder.begin(), info.genome_binder.end(), genome.begin() + isize(i)))
				bind_points.push_back(i);
		}
		e = transcription_factor{bind_points, info.is_positive_factor, 0.f};
		loc = 0;
	} else if (info.is_small_struct) {
		e = struct_protein{false, 0};
	} else if (info.is_big_struct) {
		e = struct_protein{true, 0};
	} else if (info.reaction_input.empty()) {
		e = empty_protein{};
	} else {
		static constexpr f32 c = 0.018f; // just looked nice-ish in desmos :P, can choose because the energy units are undecided before
		f32 K = glm::exp(f32(-info.energy_balance) * c); // based on K = exp(-dG / RT)
		if (info.is_genome_polymerase && info.energy_balance < 0) {
			e = special_chem_protein{{info.reaction_input, info.reaction_output, K},
				special_action::division, info.energy_balance, 0.f};
			loc = 0;
		} else if (info.is_genome_repair && info.energy_balance < 0) {
			e = special_chem_protein{{info.reaction_input, info.reaction_output, K},
				special_action::repair, info.energy_balance, 0.f};
			loc = 0;
		} else if (info.is_struct_synthesizer && info.energy_balance < 0) {
			e = special_chem_protein{{info.reaction_input, info.reaction_output, K},
				special_action::struct_synthesize, info.energy_balance, 0.f};
		} else if (info.is_motor && info.energy_balance < 0) {
			e = special_chem_protein{{info.reaction_input, info.reaction_output, K},
				info.end_marker ? special_action::move_cw : special_action::move_ccw,
				info.energy_balance, 0.f};
			loc = 1;
		} else {
			e = chem_protein{info.reaction_input, info.reaction_output, K};
		}
	}
	proteins.push_back({info.catalyzers, e, s, blocks_boundary, cost, 0.f, info.stability,
		direct_transcription, loc});
	return true;
}
bool cell::add_protein_rec(const compounds &comps, usize s, bool direct_transcription, u8 max_depth) {
	bool out = add_protein(comps, s, direct_transcription);
	if (out && std::holds_alternative<transcription_factor>(proteins.back().effect) &&
		std::get<transcription_factor>(proteins.back().effect).positive) {
		const auto &tf = std::get<transcription_factor>(proteins.back().effect);
		if (max_depth > 0) {
			for (usize i : tf.bind_points) {
				add_protein_rec(comps, i, false, max_depth - 1);
			}
		}
	}
	return out;
}
void cell::analyze(const compounds &comps) {
	bound_factors.resize(genome.size(), 0.f);
	if (genome.size() < 6) return;
	for (usize i = 0; i < genome.size() - 6; i++) {
		bool is_start = true;
		for (usize j = 0; j < 6; j++) {
			if (genome[i+j])
				is_start = false;
		}
		if (is_start) {
			add_protein_rec(comps, i+6, true);
			i += 5;
		}
	}
}
u8 cell::genome_quad(usize i) const {
	return u8(u8(genome[i]) | u8(genome[i+1]) << 1 | u8(genome[i+2]) << 2 | u8(genome[i+3]) << 3);
}
u32 cell::id(bool in_big_struct) const {
	if (in_big_struct) {
		if (big_struct_id == u8(-1))
			return u32(-1);
		return compile_options::membrane_particle_start +
			4*(gpu_id - compile_options::env_particle_count) + big_struct_id;
	}
	return gpu_id;
}
void cell::create_tick(compounds &comps, protein &prot) {
	// TODO: creating proteins shouldn't be free (in energy)
	u32 at = id(prot.location);
	if (at == u32(-1))
		return;
	f32 denat = (1.f - powf(prot.stability, f32(proteins.size()))) * prot.conc;
	f32 create_pos = (prot.direct_transcription ? .5f : -bound_factors[prot.genome_start]);
	for (usize i = prot.genome_start + 1; i < prot.genome_end; i++) {
		create_pos -= std::max(bound_factors[i], 0.f); // only negative factors afterwards
	}
	f32 max_create = 1.f;
	for (usize i = 0; i < block_count; i++) {
		if (prot.cost[i] > 0.00001f) {
			max_create = std::min(max_create, comps.at(comps.atoms_to_id[block_compounds[i]], at) / prot.cost[i]);
		}
	}
	f32 create = std::clamp(1.f - powf(1.f - create_pos, f32(proteins.size())),
		0.f, std::max(max_create, 0.f));
	f32 delta = create - denat;
	prot.conc += delta;
	for (usize i = 0; i < block_count; i++) { // "denaturated" "proteins" "decompose" instantly :)
		comps.at(comps.atoms_to_id[block_compounds[i]], at) -= delta * prot.cost[i];
	}
	if (std::holds_alternative<struct_protein>(prot.effect)) {
		auto &sp = std::get<struct_protein>(prot.effect);
		const u32 eff = u32(std::max(prot.conc, 0.f) * 200.f);
		if (sp.big) {
			big_struct -= sp.curr_effect;
			sp.curr_effect = eff;
			big_struct += sp.curr_effect;
		} else {
			small_struct -= sp.curr_effect;
			sp.curr_effect = eff;
			small_struct += sp.curr_effect;
		}
	}
}
void cell::update_tick(compounds &comps, protein &prot) {
	u32 at = id(prot.location);
	if (at == u32(-1))
		return;
	f32 cata_effect = 1.f;
	f32 req_cata_worst = 0.f;
	for (const auto &c : prot.catalyzers) {
		f32 conc = comps.at(c.compound, at);
		f32 cata;
		if (c.effect > 0) {
			cata = conc * conc - 1 + c.effect;
		} else {
			f32 cata_fun = conc * conc - 1;
			f32 inhib_fun = -conc;
			// this might be nice to precompute, but it's probably not going to make a relevant difference
			f32 eff2 = c.effect * c.effect;
			f32 eff3 = eff2 * -c.effect;
			f32 cata_coef = 2 * eff3 - 3 * eff2 + 1; // smoothstep interpolation
			f32 inhib_coef = -2 * eff3 + 3 * eff2;
			cata_effect += inhib_fun * inhib_coef;
			cata = cata_fun * cata_coef;
		}
		if (cata > 0.f)
			cata_effect += cata;
		else
			req_cata_worst = std::min(cata, req_cata_worst);
	}
	cata_effect = std::max(req_cata_worst+cata_effect, 0.f);
	std::visit(overloaded{
		[](empty_protein &) {},
		[this, &prot, cata_effect](transcription_factor &tf) {
			f32 effect = prot.conc * std::clamp(cata_effect, 0.f, 1.f) * (tf.positive ? -1.f : 1.f);
			f32 delta = effect - tf.curr_effect;
			tf.curr_effect = effect;
			for (usize i : tf.bind_points) {
				bound_factors[i] += delta;
			}
		},
		[this, at, &comps, &prot, cata_effect](chem_protein &cp) {
			for (u8 c : cp.inputs) { if (comps.locked[c]) return; }
			for (u8 c : cp.outputs) { if (comps.locked[c]) return; }
			f32 delta = reaction_delta(comps, at, prot, cata_effect, cp.K, cp.inputs, cp.outputs);
			for (u8 c : cp.inputs) {
				comps.at(c, at) -= delta;
			}
			for (u8 c : cp.outputs) {
				comps.at(c, at) += delta;
			}
		},
		[this, at, &comps, &prot, cata_effect](special_chem_protein &scp) {
			for (u8 c : scp.inputs) { if (comps.locked[c]) return; }
			for (u8 c : scp.outputs) { if (comps.locked[c]) return; }
			f32 delta = reaction_delta(comps, at, prot, cata_effect, scp.K, scp.inputs, scp.outputs);
			if (delta < 0.001f) {
				net_movement -= scp.movement;
				scp.movement = 0.f;
				return;
			}
			f32 energy = delta * f32(-scp.energy_balance);
			if (energy < 0.001f) {
				net_movement -= scp.movement;
				scp.movement = 0.f;
				return;
			}
			f32 spent_energy = 0.f;
			switch (scp.act) {
			case special_action::division:{
				if (comps.locked[comps.atoms_to_id[g0_comp]]) { return; }
				if (comps.locked[comps.atoms_to_id[g1_comp]]) { return; }
				static constexpr f32 bit_cost = 1.f / 4096.f;
				static constexpr f32 incorrect_unbind_chance = 0.9995f;
				f32 &zero_conc = comps.at(comps.atoms_to_id[g0_comp], at);
				f32 &one_conc = comps.at(comps.atoms_to_id[g1_comp], at);
				const auto push_bit = [this, &zero_conc, &one_conc](bool bit) {
					f32 &conc = bit ? one_conc : zero_conc;
					if (conc > bit_cost) {
						division_genome.push_back(bit);
						conc -= bit_cost;
					}
				};
				for (; division_pos < genome.size() && spent_energy < energy - .003f; spent_energy += .003f) {
					f32 conc_sum = zero_conc + one_conc;
					if (conc_sum < bit_cost * 64.f)
						break;
					if (rand() % 65536 == 0) { // length mutation
						if (rand() % 20 == 0) { // jump to random location
							division_pos = usize(rand()) % genome.size();
						} if (rand() % 2 == 0) { // skip some bits
							if (rand() % 2 == 0) {
								division_pos += 4;
							} else {
								division_pos++;
							}
						} else { // insert some bits
							if (rand() % 2 == 0) {
								push_bit(randf() * conc_sum > zero_conc);
								push_bit(randf() * conc_sum > zero_conc);
								push_bit(randf() * conc_sum > zero_conc);
								push_bit(randf() * conc_sum > zero_conc);
							} else {
								push_bit(randf() * conc_sum > zero_conc);
							}
						}
					} else {
						f32 correct_prob = (genome[division_pos] ? one_conc : zero_conc) / conc_sum;
						// prob that the right base will be selected is (r = incorrect_unbind_chance)
						// p_corr + p_corr * (1-p_corr) * r + p_corr * (1-p_corr)^2 * r^2 ...
						// ^ right           ^ on second try           ^ third try...
						// = p_corr / (1 - (1 - p_corr) * r)   (geometric series)
						push_bit((1.f - (1.f - correct_prob) * incorrect_unbind_chance) * randf() <
							correct_prob ? genome[division_pos] : !genome[division_pos]);
						division_pos++;
					}
				}
				} break;
			case special_action::repair:{
				const u32 repair = u32(std::max(i32(energy * 1000.f), 0));
				health = std::min(health + repair, max_health);
				spent_energy = f32(repair) * .001f;
				} break;
			case special_action::struct_synthesize:{
				if (comps.locked[comps.atoms_to_id[struct_a_comp]]) { return; }
				if (comps.locked[comps.atoms_to_id[struct_b_comp]]) { return; }
				const f32 struct_cost = 1.f / 128.f;
				f32 &conca = comps.at(comps.atoms_to_id[struct_a_comp], at);
				f32 &concb = comps.at(comps.atoms_to_id[struct_b_comp], at);
				const u32 built = u32(std::max(0,
					std::min({i32(energy * 100.f), i32(conca / struct_cost), i32(concb / struct_cost)})));
				conca -= f32(built) * struct_cost;
				concb -= f32(built) * struct_cost;
				spent_energy = f32(built) * .01f;
				(prot.location ? flagellum_add : membrane_add) += built;
				} break;
			case special_action::move_cw:{
				spent_energy = std::min(energy, 0.5f);
				f32 nm = spent_energy * 0.01f;
				net_movement += nm - scp.movement;
				scp.movement = nm;
				break;
			}
			case special_action::move_ccw:{
				spent_energy = std::min(energy, 0.5f);
				f32 nm = -spent_energy * 0.01f;
				net_movement += nm - scp.movement;
				scp.movement = nm;
				break;
			}
			default: return;
			}
			delta = delta * spent_energy / energy;
			for (u8 c : scp.inputs) {
				comps.at(c, at) -= delta;
			}
			for (u8 c : scp.outputs) {
				comps.at(c, at) += delta;
			}
		},
		[](struct_protein &) { }
		}, prot.effect);
}
static f32 min_norm(f32 x, f32 n) {
	if (x > n || x < -n) return x;
	if (x < 0) return -n;
	return n;
}
template<typename Range>
f32 select_root(const Range &root_list) {
	f32 out = std::numeric_limits<f32>::max();
	for (f32 r : root_list) {
		if (!std::isnan(r) && abs(r) < abs(out)) {
			out = r;
		}
	}
	return abs(out) > 10.f ? 0.f : out;
}
template<>
f32 select_root<std::pair<f32, f32>>(const std::pair<f32, f32> &root_list) {
	return select_root(std::array<f32, 2>{root_list.first, root_list.second});
}
f32 cell::reaction_delta(compounds &comps, u32 at, const protein &prot, f32 cata_effect, f32 K,
	const std::vector<u8> &inp, const std::vector<u8> &out) const {
	if (inp.empty()) return 0.f;
	// PROD c_p / PROD c_e should approach K, so
	// let curr_k = PROD c_p / PROD c_e
	// let next_k = lerp(k', K, protein_effect_strength)
	// solve for d here: PROD (c_p + d) - PROD (c_e - d) * next_k = 0
	// fi and fo are factors of those polynomials, PROD (c_e - d) and PROD (c_p + d)
	f32 out_prod = 1.f;
	f32 inp_prod = 1.f;
	// (inp count = out count)
	std::vector<f32> fi(inp.size()+1, 0.f), fo(inp.size()+1, 0.f);
	fi[0] = 1.f;
	fo[0] = 1.f;
	f32 max_inp_take = 1.f;
	for (u8 c : inp) {
		f32 conc = comps.at(c, at);
		usize count = 0;
		for (u8 c2 : inp) { if (c == c2) count++; }
		max_inp_take = std::min(conc / f32(count), max_inp_take);
		inp_prod *= conc > 0.f ? conc : 0.f;
		for (usize i = fi.size() - 1; i > 0; i--) {
			fi[i] = fi[i] * conc - fi[i-1];
		}
		fi[0] *= conc;
	}
	f32 max_out_take = 1.f;
	for (u8 c : out) {
		f32 conc = comps.at(c, at);
		usize count = 0;
		for (u8 c2 : out) { if (c == c2) count++; }
		max_out_take = std::min(conc / f32(count), max_out_take);
		out_prod *= conc > 0.f ? conc : 0.f;
		for (usize i = fo.size() - 1; i > 0; i--) {
			fo[i] = fo[i] * conc + fo[i-1];
		}
		fo[0] *= conc;
	}
	if (std::max(inp_prod, out_prod) < 0.0001f) {
		return 0.f;
	}
	const f32 curr_conc_ratio = out_prod / std::max(inp_prod, 0.0001f);
	const f32 t = std::clamp(1.f - powf(1.f - .003f * cata_effect * prot.conc, f32(proteins.size())), 0.f, 1.f);
	const f32 next_conc_ratio = std::lerp(curr_conc_ratio, K, t);
	for (usize i = 0; i < fo.size(); i++) {
		fo[i] -= fi[i] * next_conc_ratio;
	}
	switch (fo.size()) {
	case 2: // linear
		return std::clamp(-fo[0] / min_norm(fo[1], 0.001f), -max_out_take, max_inp_take);
	case 3:
		return std::clamp(select_root(boost::math::tools::quadratic_roots(fo[2], fo[1], fo[0])),
			-max_out_take, max_inp_take);
	case 4:
		return std::clamp(select_root(boost::math::tools::cubic_roots(fo[3], fo[2], fo[1], fo[0])),
			-max_out_take, max_inp_take);
	case 5:
		return std::clamp(select_root(boost::math::tools::quartic_roots(fo[4], fo[3], fo[2], fo[1], fo[0])),
			-max_out_take, max_inp_take);
	default:
		logs::errorln("cell", "reaction_delta was called with invalid reaction size! (cell id = ",
			gpu_id, ")");
		return 0.f;
	}
}

void cell::test() {
	logs::infoln("cell test", "running cell test");
	std::unique_ptr<compounds> c = std::make_unique<compounds>();
#define ZZZZ 0.f, 0.f, 0.f, 0.f,
#define Z14 ZZZZ ZZZZ ZZZZ 0.f, 0.f
	for (usize n = 1; n < 5; n++) {
		proteins.clear();
		std::vector<u8> in;
		std::vector<u8> out;
		for (usize i = 0; i < n; i++) {
			in.push_back(u8(i));
			out.push_back(u8(i + n));
		}
		proteins.push_back(protein{{}, chem_protein{in, out, 1.f}, 0, 0, {Z14}, 1.f, 0.05f, false, 0});
		for (usize i = 0; i < 100; i++) {
			auto &p = proteins[0];
			p.catalyzers.clear();
			usize catas = usize(rand() % 32);
			for (usize i = 0; i < catas; i++) {
				p.catalyzers.push_back({randf()*2.f-1.f, u8(usize(rand())%compounds::count)});
			}
			f32 K = powf(2.f, 4*randf()-2.f);
			std::get<chem_protein>(proteins[0].effect).K = K;
			for (usize i = 0; i < compounds::count; i++) {
				c->at(i, gpu_id) = randf() * 2.f;
			}
			const auto k = [this, &c, n]() {
				f32 in = 0.f;
				f32 out = 0.f;
				for (usize i = 0; i < n; i++) {
					in *= c->at(i, gpu_id);
					out *= c->at(i+n, gpu_id);
				}
				return out / std::max(in, 0.00001f);
			};
			f32 sk = k();
			f32 pk = sk;
			for (usize i = 0; i < 100; i++) {
				update_tick(*c, proteins[0]);
				f32 ck = k();
				static constexpr const char *types[] = {"linear", "quadratic", "cubic", "quartic"};
				if (abs(ck - K) > abs(pk - K) + 0.0001f) {
					logs::errorln("cell test", "[chem protein].[", types[n-1], "]: k doesn't approach K");
					logs::errorln("cell test", "\tk = ", ck, ", prev k = ", pk, ", K = ", K);
				}
				for (usize j = 0; j < compounds::count; j++) {
					if (c->at(j, gpu_id) < -0.0001f) {
						logs::errorln("cell test", "[chem protein].[", types[n-1], "]: concetration below 0");
						logs::errorln("cell test", "\tconcentrations[", j, "] = ", c->at(j, gpu_id));
					} else if (c->at(j, gpu_id) > 7.f) {
						logs::warnln("cell test", "[chem protein].[", types[n-1], "]: concetration suspiciously high");
						logs::warnln("cell test", "\tconcentrations[", j, "] = ", c->at(j, gpu_id));
					}
				}
				pk = ck;
			}
		}
	}
	logs::infoln("cell test", "done");
}

