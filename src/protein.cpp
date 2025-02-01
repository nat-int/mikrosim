#include "protein.hpp"
#include <bit>
#include <glm/common.hpp>
#include <glm/geometric.hpp>
#include <glm/exponential.hpp>
#include <glm/trigonometric.hpp>
#include <glm/vec2.hpp>
#include <glm/mat2x2.hpp>
#include <imgui.h>
#include "compounds.hpp"

std::vector<u8> &folder::at(i32 x, i32 y) {
	return placed[usize(x) * height + usize(y)];
}
const std::vector<u8> &folder::at(i32 x, i32 y) const {
	return placed[usize(x) * height + usize(y)];
}
void folder::replace(const std::vector<block> &chain) {
	placed.clear();
	place(chain);
}
void folder::place(const std::vector<block> &chain) {
	glm::ivec2 min = {0,0};
	glm::ivec2 max = {0,0};
	glm::ivec2 pos = {0,0};
	u8 dir = 0;
	static constexpr glm::ivec2 dir_shifts[] = { {1,0}, {0,1}, {-1,0}, {0,-1} };
	for (const block &b : chain) {
		min = glm::min(min, pos);
		max = glm::max(max, pos);
		dir = u8((dir + 4 + b.shape) % 4);
		pos += dir_shifts[dir];
	}
	pos = -min;
	max -= min;
	width = usize(max.x+1);
	height = usize(max.y+1);
	placed.resize(width * height);
	dir = 0;
	for (const block &b : chain) {
		at(pos.x, pos.y).push_back(std::rotr(b.atoms, dir*2));
		dir = u8((dir + 4 + b.shape) % 4);
		pos += dir_shifts[dir];
	}
}
struct binding_site {
	u8 l, r, u, d;
	usize i;
	std::array<u8, 4> comp_set4() const {
		std::array<u8, 4> out;
		for (u8 i = 0; i < 4; i++) {
			const u8 map[5] = { 2, 1, 0, 3, i };
			out[i] = compound(map[u%251], map[r%251], map[d%251], map[l%251]).atoms;
		}
		return out;
	}
	u8 comp_set1() const {
		const u8 map[4] = { 2, 1, 0, 3 };
		return compound(map[u], map[r], map[d], map[l]).atoms;
	}
	u8 comp_nr() const {
		const u8 map[4] = { 2, 1, 0, 3 };
		return u8(map[u] << 6 | map[r] << 4 | map[d] << 2 | map[l]);
	}
	glm::vec2 pos(usize height) const { return {f32(u32(i / height)), f32(i % height)}; }
};
protein_info folder::analyze(const compounds &comp) const {
	protein_info out{};
	f32 stability_factor = 0.f;
	// find interesting places in the placed protein
	std::vector<binding_site> active_sites;
	std::vector<binding_site> catalysis_sites;
	for (usize i = 0; i < placed.size(); i++) {
		if (!placed[i].empty()) {
			if (placed[i].size() > 1) {
				stability_factor += 0.05f;
				if (i / height == 0 || i / height == width-1 || i % height == 0 || i % height == height-1 ||
					placed[i-height].empty() || placed[i+height].empty() || placed[i-1].empty() ||
					placed[i+1].empty()) {
					stability_factor += 1.f;
				}
			}
			continue;
		}
		u8 l = 255, r = 255, u = 255, d = 255;
		if (i / height > 0 && placed[i-height].size() == 1) { l = placed[i-height][0] >> 4 & 3; }
		if (i / height < width-1 && placed[i+height].size() == 1) { r = placed[i+height][0] & 3; }
		if (i % height > 0 && placed[i-1].size() == 1) { u = placed[i-1][0] >> 2 & 3; }
		if (i % height < height-1 && placed[i+1].size() == 1) { d = placed[i+1][0] >> 6 & 3; }
		u8 c255 = (l==255?1:0)+(r==255?1:0)+(u==255?1:0)+(d==255?1:0);
		if (c255 == 0) {
			if (active_sites.size() < 4) {
				active_sites.push_back(binding_site{l, r, u, d, i});
			}
		} else if (c255 == 1) {
			catalysis_sites.push_back(binding_site{l, r, u, d, i});
		}
	}
	out.stability = 1.f - expf(-stability_factor*.25f) * .15f; // just feels nice
	out.stability = std::min(out.stability, 0.99f);
	// check edges for genome binding sites (which make the protein a transcription factor)
	usize max_genome_site_pos;
	std::vector<bool> max_genome_site;
	usize max_g_seq_len = 0;
	const auto check_edges = [this, &max_genome_site, &max_g_seq_len, &max_genome_site_pos](usize bound,
		usize add, usize mul, u8 shift) {
		std::vector<bool> genome_site;
		usize g_seq_len = 0;
		for (usize i = 0; i < bound; i++) {
			const std::vector<u8> place = placed[add + mul * i];
			if (place.size() != 1 || (place[0] >> shift & 3) != 1) {
				max_g_seq_len = std::max(g_seq_len, max_g_seq_len);
				g_seq_len = 0;
			} else {
				g_seq_len++;
			}
			if (place.size() != 1 || (0b101 >> (place[0] >> shift & 3) & 1) != 1) {
				if (max_genome_site.size() < genome_site.size()) {
					max_genome_site.swap(genome_site);
					max_genome_site_pos = add + mul * i;
				}
				genome_site.clear();
			} else {
				genome_site.push_back((place[0] >> shift & 3) == 2 ? 0 : 1);
			}
		}
		max_g_seq_len = std::max(g_seq_len, max_g_seq_len);
		if (max_genome_site.size() < genome_site.size()) {
			max_genome_site.swap(genome_site);
			max_genome_site_pos = add + mul * bound;
		}
	};
	check_edges(width, 0, height, 6);
	check_edges(width, height-1, height, 2);
	check_edges(height, 0, 1, 0);
	check_edges(height, (width-1)*height, 1, 4);
	if (max_g_seq_len > 3) {
		out.is_positive_factor = true;
	}
	// find center of active sites
	std::vector<glm::vec2> active_locs; active_locs.reserve(active_sites.size());
	glm::vec2 active_center = {0.f, 0.f};
	for (const binding_site &s : active_sites) {
		active_locs.push_back(s.pos(height));
		active_center += active_locs.back() / f32(active_sites.size());
	}
	// figure out which compounds fall into catalysis sites, their effect is sin(dist_to_closest_active_site)
	const auto find_catalyzers = [this, &active_locs, &catalysis_sites, &out, &comp]() {
		for (const binding_site &s : catalysis_sites) {
			glm::vec2 pos = s.pos(height);
			f32 min_sqdist = f32(width*width + height*height);
			for (const auto t : active_locs) { min_sqdist = std::min(min_sqdist, glm::dot(pos-t, pos-t)); }
			f32 effect = glm::sin(glm::sqrt(min_sqdist));
			const auto comp_set = s.comp_set4();
			for (const u8 a : comp_set) {
				const u8 ci = comp.atoms_to_id[a];
				auto i = std::ranges::find_if(out.catalyzers, [ci](const auto &c) { return c.compound == ci; });
				if (i == out.catalyzers.end())
					out.catalyzers.push_back({effect, ci});
				else
					i->effect += effect;
			}
		}
	};
	// is a transcription factor -> active site is the genome binding site
	if (max_genome_site.size() > 6) {
		active_locs.clear();
		active_locs.push_back(binding_site{0,0,0,0, max_genome_site_pos}.pos(height));
		find_catalyzers();
		out.genome_binder.swap(max_genome_site);
		return out;
	}
	find_catalyzers();
	// figure out which compounds fall into active sites
	std::vector<u8> comps;
	comps.reserve(active_sites.size());
	i32 reaction_energy_change = 0;
	for (const binding_site &s : active_sites) {
		comps.push_back(comp.atoms_to_id[s.comp_set1()]);
	}
	// check if there are both "nucleotid" compound sites close to each other for genome polymerase
	const u8 genome_0_id = comp.atoms_to_id[g0_comp];
	const u8 genome_1_id = comp.atoms_to_id[g1_comp];
	auto g0i = std::ranges::find(comps, genome_0_id);
	auto g1i = std::ranges::find(comps, genome_1_id);
	if (g0i != comps.end() && g1i != comps.end() &&
		std::find(g0i+1, comps.end(), genome_0_id) == comps.end() &&
		std::find(g1i+1, comps.end(), genome_1_id) == comps.end()) {
		glm::vec2 d = active_sites[usize(g0i - comps.begin())].pos(height) -
			active_sites[usize(g1i - comps.begin())].pos(height);
		if (glm::dot(d, d) <= 16.f) {
			out.is_genome_polymerase = true;
		}
		active_sites.erase(active_sites.begin() + (g0i - comps.begin()));
		comps.erase(g0i);
		g1i = std::ranges::find(comps, genome_1_id);
		active_sites.erase(active_sites.begin() + (g1i - comps.begin()));
		comps.erase(g1i);
	}
	// if there's 4+ long genome site and "nucleotid" active site (g0 goes first), then it's genome repair
	else if (max_genome_site.size() > 3 && (g0i != comps.end() || g1i != comps.end())) {
		out.is_genome_repair = true;
		if (g0i != comps.end()) {
			active_sites.erase(active_sites.begin() + (g0i - comps.begin()));
			comps.erase(g0i);
		} else {
			active_sites.erase(active_sites.begin() + (g1i - comps.begin()));
			comps.erase(g1i);
		}
	}
	// find reactions at each site
	std::vector<bool> used(active_sites.size(), false);
	for (usize i = 0; i < active_sites.size(); i++) {
		out.reaction_input.push_back(comps[i]);
		reaction_energy_change -= comp.infos[comps[i]].energy;
		if (used[i]) continue;
		for (usize j = i+1; j < active_sites.size(); j++) {
			if (used[j]) continue;
			glm::vec2 d = active_sites[j].pos(height) - active_sites[i].pos(height);
			if (glm::dot(d, d) <= 16.f) {
				// reaction between sites - exchange
				glm::vec2 rd = glm::mat2{{1.f, -1.f}, {1.f, 1.f}} * d;
				u8 exchanged_a;
				u8 exchanged_b;
				if (rd.x > 0 && rd.y > 0) { // A above B
					exchanged_a = u8((active_sites[i].comp_nr() & 0xf3) | (active_sites[j].comp_nr() & 0xc0) >> 4);
					exchanged_b = u8((active_sites[j].comp_nr() & 0x3f) | (active_sites[i].comp_nr() & 0xc) << 4);
				} else if (rd.x > 0) { // A on the left, B on the right
					exchanged_a = u8((active_sites[i].comp_nr() & 0xcf) | (active_sites[j].comp_nr() & 0x3) << 4);
					exchanged_b = u8((active_sites[j].comp_nr() & 0xfc) | (active_sites[i].comp_nr() & 0x30) >> 4);
				} else if (rd.y > 0) { // A on the right, B on the left
					exchanged_a = u8((active_sites[i].comp_nr() & 0xfc) | (active_sites[j].comp_nr() & 0x30) >> 4);
					exchanged_b = u8((active_sites[j].comp_nr() & 0xcf) | (active_sites[i].comp_nr() & 0x3) << 4);
				} else { // A below B
					exchanged_a = u8((active_sites[i].comp_nr() & 0x3f) | (active_sites[j].comp_nr() & 0xc) << 4);
					exchanged_b = u8((active_sites[j].comp_nr() & 0xf3) | (active_sites[i].comp_nr() & 0xc0) >> 4);
				}
				out.reaction_output.push_back(comp.atoms_to_id[compound(exchanged_a).atoms]);
				reaction_energy_change += comp.infos[out.reaction_output.back()].energy;
				out.reaction_output.push_back(comp.atoms_to_id[compound(exchanged_b).atoms]);
				reaction_energy_change += comp.infos[out.reaction_output.back()].energy;
				used[j] = used[i] = true;
				break;
			}
		}
		if (!used[i]) {
			// reaction in site - transmutation
			glm::vec2 d = active_sites[i].pos(height) - active_center;
			auto transmute = [](u8 x, i32 prerot) {
				x = std::rotr(x, prerot * 2);
				return compound(u8((x & 0xf0) | (x >> 2 & 3) | ((x & 3) << 2))).atoms;
			};
			if (d.x > 0 && d.y > 0) {
				out.reaction_output.push_back(comp.atoms_to_id[transmute(active_sites[i].comp_nr(), 1)]);
			} else if (d.x > 0) {
				out.reaction_output.push_back(comp.atoms_to_id[transmute(active_sites[i].comp_nr(), 0)]);
			} else if (d.y > 0) {
				out.reaction_output.push_back(comp.atoms_to_id[transmute(active_sites[i].comp_nr(), 2)]);
			} else {
				out.reaction_output.push_back(comp.atoms_to_id[transmute(active_sites[i].comp_nr(), 3)]);
			}
			reaction_energy_change += comp.infos[out.reaction_output.back()].energy;
		}
	}
	out.energy_balance = reaction_energy_change;
	return out;
}
void folder::draw() const {
	static constexpr f32 sz = 12.f; static constexpr f32 sp = 3; static constexpr f32 w = 3.f;
	static const ImU32 cols[] = {ImColor(1.f, .2f, .2f), ImColor(.1f, 1.f, .1f), ImColor(.1f, .1f, 1.f),
		ImColor(.6f, .6f, .6f), ImColor(1.f, 0.f, 1.f)};
	ImDrawList *draw_list = ImGui::GetWindowDrawList();
	ImVec2 pos = ImGui::GetCursorScreenPos();
	for (usize i = 0; i < placed.size(); i++) {
		if (placed[i].empty())
			continue;
		f32 x = f32(u32(i / height)) * sz + pos.x;
		f32 y = f32(i % height) * sz + pos.y;
		if (placed[i].size() > 1) {
			draw_list->AddLine({x + sz/2, y}, {x + sz/2, y + sz}, cols[4], w);
			draw_list->AddLine({x, y + sz/2}, {x + sz, y + sz/2}, cols[4], w);
			continue;
		}
		u8 p = placed[i][0];
		draw_list->AddLine({x + sz/2, y + sz/2}, {x + sz/2, y}, cols[p >> 6 & 3], w);
		draw_list->AddLine({x + sz/2, y + sz/2}, {x + sz, y + sz/2}, cols[p >> 4 & 3], w);
		draw_list->AddLine({x + sz/2, y + sz/2}, {x + sz/2, y + sz}, cols[p >> 2 & 3], w);
		draw_list->AddLine({x + sz/2, y + sz/2}, {x, y + sz/2}, cols[p & 3], w);
	}
	ImGui::Dummy({sz*f32(width)+sp, sz*f32(height)+sp});
}

