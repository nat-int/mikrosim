#include "views.hpp"
#include <cinttypes>
#include <imgui.h>
#include <imgui/misc/cpp/imgui_stdlib.h>
#include <imgui_internal.h>
#include "log/log.hpp"
#include "parse.hpp"
#include "util.hpp"

template<class... Ts> struct overloaded : Ts... { using Ts::operator()...; };
template<class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

protein_view::protein_view() : random_len(42), genome_readonly(true), reqs(-1,-1,-1,-1),
	generating(false) { }
void protein_view::set_rand(const compounds &comps, usize len) {
	std::string s(len, quads_chr[stop_quad]);
	s[0] = quads_chr[start_quad];
	for (u32 i = 1; i < len-1; i++) {
		s[i] = quads_chr_nostop[rand() % 15];
	}
	set(comps, s);
}
void protein_view::set(const compounds &comps, const std::string &g) {
	genome = g;
	std::vector<block> bls;
	for (u32 i = 0; i < genome.size(); i++) {
		char c = genome[i];
		if ((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f')) {
			bls.push_back(blocks[chr_to_quad(c) % block_count]);
		}
	}
	set(comps, bls);
}
void protein_view::draw(const compounds &comps) {
	if (ImGui::Begin("protein view")) {
		if (ImGui::Button("randomize")) {
			generation.clear();
			set_rand(comps, random_len);
			generation.push_back(genome);
			generating = true;
		}
		if (generating) {
			generator_gen(comps);
		}
		ImGui::SameLine();
		ImGui::SliderInt("length", reinterpret_cast<int *>(&random_len), 3, 100);
		ImGui::InputInt4("require", &reqs.x);
		if (ImGui::IsItemHovered()) {
			ImGui::SetTooltip("-6 -> is transcription factor\n-5 -> is genome polymerase\n"
				"-4 -> has 3+ compound inputs\n-3 -> has 2+ compound inputs\n"
				"-2 -> has any compound input\n-1 -> no requirement\n"
				"0+ -> require specific compound as input");
		}
		ImGui::Separator();
		f.draw();
		ImGui::Separator();
		ImGui::Dummy({0.f, 0.f});
		for (u8 c : info.reaction_input) {
			ImGui::SameLine();
			comps.infos[c].imgui(16.f, 5.f, 3.f);
		}
		ImGui::SameLine();
		ImDrawList *draw_list = ImGui::GetWindowDrawList();
		ImVec2 pos = ImGui::GetCursorScreenPos();
		draw_list->AddLine({pos.x, pos.y + 6}, {pos.x + 16, pos.y + 6}, ImColor(1.f, 1.f, 1.f), 3.f);
		draw_list->AddLine({pos.x + 10, pos.y}, {pos.x + 16, pos.y + 6}, ImColor(1.f, 1.f, 1.f), 3.f);
		draw_list->AddLine({pos.x, pos.y + 10}, {pos.x + 16, pos.y + 10}, ImColor(1.f, 1.f, 1.f), 3.f);
		draw_list->AddLine({pos.x + 6, pos.y + 16}, {pos.x, pos.y + 10}, ImColor(1.f, 1.f, 1.f), 3.f);
		ImGui::Dummy({19.f, 19.f});
		for (u8 c : info.reaction_output) {
			ImGui::SameLine();
			comps.infos[c].imgui(16.f, 5.f, 3.f);
		}
		if (ImGui::BeginTable("##catalyzers", 2)) {
			ImGui::TableNextRow(); ImGui::TableNextColumn(); ImGui::TableHeader("compound");
			ImGui::TableNextColumn(); ImGui::TableHeader("effect");
			for (const auto &c : info.catalyzers) {
				ImGui::TableNextRow(); ImGui::TableNextColumn(); comps.infos[c.compound].imgui();
				ImGui::TableNextColumn(); ImGui::Text("%.4lf", f64(c.effect));
			}
			ImGui::EndTable();
		}
		if (info.genome_binder.empty()) {
			ImGui::Text("no genome binder");
		} else {
			std::string genome_bind; genome_bind.reserve(info.genome_binder.size());
			for (bool i : info.genome_binder) { genome_bind.push_back(i ? 'B' : 'R'); }
			ImGui::Text("binds to %s", genome_bind.c_str());
		}
		if (ImGui::BeginTable("##other_info", 2)) {
			ImGui::TableNextRow(); ImGui::TableNextColumn(); ImGui::TableHeader("is genome polymerase");
			ImGui::TableNextColumn();
			ImGui::Text("%s", (info.is_genome_polymerase ? "yes" : "no"));
			ImGui::TableNextRow(); ImGui::TableNextColumn(); ImGui::TableHeader("is positive factor");
			ImGui::TableNextColumn();
		 	ImGui::Text("%s", (info.is_positive_factor ? "yes" : "no"));
			ImGui::TableNextRow(); ImGui::TableNextColumn(); ImGui::TableHeader("reaction energy change");
			ImGui::TableNextColumn(); ImGui::Text("%" PRIi32, info.energy_balance);
			ImGui::EndTable();
		}
		ImGui::Checkbox("genome readonly", &genome_readonly);
		ImGui::SameLine();
		if (ImGui::Button("set")) {
			genome.erase(std::remove_if(genome.begin(), genome.end(), [](char c) {
				return (c < '0' || c > '9') && (c < 'a' || c > 'f'); }), genome.end());
			set(comps, genome);
		}
		ImGui::SameLine();
		if (ImGui::Button("log")) {
			logs::infoln("protein view", "genome: ", genome);
		}
		ImGui::InputTextMultiline("genome", &genome, {0, 0},
			genome_readonly ? ImGuiInputTextFlags_ReadOnly : ImGuiInputTextFlags_None);
	}
	ImGui::End();
}
void protein_view::set(const compounds &comps, const std::vector<block> &bls) {
	f.replace(bls);
	info = f.analyze(comps);
}
void protein_view::generator_gen(const compounds &comps) {
	std::vector<f32> fits; fits.reserve(generation.size());
	f32 fit_sum = 0.f;
	for (usize i = 0; i < generation.size(); i++) {
		set(comps, generation[i]);
		f32 fit = .1f;
		for (i32 i : {reqs.x, reqs.y, reqs.z, reqs.w}) {
			if (i >= 0 && usize(i) < compounds::count) {
				if (std::find(info.reaction_input.begin(), info.reaction_input.end(), u8(i)) !=
					info.reaction_input.end()) {
					fit += 1.f;
				} else if (info.reaction_input.size()) {
					fit += .5f;
				}
			} else if (i == -2) {
				fit += std::clamp(f32(info.reaction_input.size()), 0.f, 1.f);
			} else if (i == -3) {
				fit += std::clamp(f32(info.reaction_input.size()) * .5f, 0.f, 1.f);
			} else if (i == -4) {
				fit += std::clamp(f32(info.reaction_input.size()) * .333333334f, 0.f, 1.f);
			} else if (i == -5) {
				if (info.is_genome_polymerase) {
					fit += 1.f;
				}
				fit += std::clamp(f32(info.reaction_input.size()) * .25f, 0.f, .5f);
				if (std::find(info.reaction_input.begin(), info.reaction_input.end(),
					comps.atoms_to_id[g0_comp]) != info.reaction_input.end()) {
					fit += .1f;
				}
				if (std::find(info.reaction_input.begin(), info.reaction_input.end(),
					comps.atoms_to_id[g1_comp]) != info.reaction_input.end()) {
					fit += .1f;
				}
			} else if (i == -6) {
				// to lazy to make an actually good condition here as it's really easy to make it manually
				if (!info.genome_binder.empty()) {
					fit += 1.f;
				}
			} else {
				fit += 1.f;
			}
		}
		if (fit >= 4.099f) {
			generating = false;
			return;
		}
		fit_sum += fit * fit;
		fits.push_back(fit);
	}
	std::vector<std::string> next_gen;
	next_gen.reserve(100);
	for (usize i = 0; i < 100; i++) {
		f32 c = randf() * fit_sum;
		usize j = 0;
		if (generation.size() > 1) {
			for (; j < generation.size()-1; j++) {
				c -= fits[j] * fits[j];
				if (c < 0)
					break;
			}
		}
		std::string n = generation[j];
		if (rand() % 20 != 0)
			n[usize(rand()) % (n.size() - 2) + 1] = quads_chr_nostop[rand() % 15];
		if (rand() % 2 == 0)
			n[usize(rand()) % (n.size() - 2) + 1] = quads_chr_nostop[rand() % 15];
		next_gen.push_back(n);
	}
	generation.swap(next_gen);
}

cell_view::cell_view() : ext_cell(0, {}, {}), c(&ext_cell) {
	ext_cell.genome = {};
	file_path = "./basic.genome";
}
void cell_view::draw(const compounds &comps, protein_view &pv) {
	if (ext_cell.bound_factors.empty()) {
		ext_cell.analyze(comps);
	}
	if (ImGui::Begin("cell view")) {
		if (ImGui::BeginTable("##base_info", 2)) {
			ImGui::TableNextRow(); ImGui::TableNextColumn(); ImGui::TableHeader("state");
			ImGui::TableNextColumn(); ImGui::Text("%s", c->s == cell::state::none ? "none" : "active");
			ImGui::TableNextRow(); ImGui::TableNextColumn(); ImGui::TableHeader("gpu id");
			ImGui::TableNextColumn(); ImGui::Text("%" PRIu32, c->gpu_id);
			ImGui::TableNextRow(); ImGui::TableNextColumn(); ImGui::TableHeader("pos");
			ImGui::TableNextColumn(); ImGui::Text("%.3lf %.3lf", f64(c->pos.x), f64(c->pos.y));
			ImGui::TableNextRow(); ImGui::TableNextColumn(); ImGui::TableHeader("vel");
			ImGui::TableNextColumn(); ImGui::Text("%.3lf %.3lf", f64(c->vel.x), f64(c->vel.y));
			ImGui::TableNextRow(); ImGui::TableNextColumn(); ImGui::TableHeader("division progress");
			ImGui::TableNextColumn(); ImGui::Text("%zu / %zu", c->division_pos, c->genome.size());
			ImGui::EndTable();
		}
		ImGui::Checkbox("follow", &follow);
		usize hovered_prot = usize(-1);
		if (ImGui::BeginTable("##proteins", 4)) {
			ImGui::TableNextRow(); ImGui::TableNextColumn();
			ImGui::TableNextColumn(); ImGui::TableHeader("genome range");
			ImGui::TableNextColumn(); ImGui::TableHeader("concentration");
			ImGui::TableNextColumn(); ImGui::TableHeader("effect");
			usize i = 0;
			for (protein &p : c->proteins) {
				ImGui::PushID(("##id"+std::to_string(i++)).c_str());
				ImGui::TableNextRow(); ImGui::TableNextColumn();
				if (ImGui::Button("show")) {
					usize blocks_boundary;
					for (usize i = p.genome_start; ; i++) {
						if (c->genome_quad(i) == start_quad) {
							blocks_boundary = i;
							break;
						}
					}
					std::string gen;
					for (usize i = blocks_boundary; ; i += 4) {
						u8 q = c->genome_quad(i);
						gen.push_back(quads_chr[q]);
						if (q == stop_quad) {
							break;
						}
					}
					pv.set(comps, gen);
				}
				ImGui::TableNextColumn(); ImGui::Text("%zu %zu", p.genome_start, p.genome_end);
				ImGui::TableNextColumn(); ImGui::Text("%.4lf", f64(p.conc));
				ImGui::TableNextColumn();
				std::visit(overloaded{
					[](transcription_factor &tf) { ImGui::Text("TF %.3lf", f64(tf.curr_effect)); },
					[](chem_protein &cp) { ImGui::Text("ENZ, K=%.3lf", f64(cp.K)); },
					[](special_chem_protein &scp) {
						switch (scp.act) {
						case special_action::division: ImGui::Text("genome polymerase"); break;
						default: ImGui::Text("unknown special action!"); break;
						}
					},
				}, p.effect);
				ImGui::PopID();
			}
			hovered_prot = usize(ImGui::TableGetHoveredRow()-1);
			ImGui::EndTable();
		}
		ImDrawList *draw_list = ImGui::GetWindowDrawList();
		ImVec2 pos = ImGui::GetCursorScreenPos();
		constexpr static f32 spy = 7.f;
		const bool hover_tf = hovered_prot < c->proteins.size() &&
			std::holds_alternative<transcription_factor>(c->proteins[hovered_prot].effect);
		const std::optional<const transcription_factor *> hover_tf_extract = hover_tf ?
			std::optional<const transcription_factor *>(
				&std::get<transcription_factor>(c->proteins[hovered_prot].effect)) : std::nullopt;
		for (usize i = 0; i < (c->genome.size() + 149) / 150; i++) {
			for (usize j = 0; j < 150; j++) {
				const usize k = 150 * i + j;
				if (k < c->genome.size()) {
					draw_list->AddLine({pos.x + f32(j) * 3.f, pos.y + f32(i) * spy},
						{pos.x + f32(j + 1) * 3.f, pos.y + f32(i) * spy},
						c->genome[k] ? ImColor(.1f, .1f, 1.f) : ImColor(1.f, .1f, .1f), 3.f);
					if (hovered_prot < c->proteins.size() && c->proteins[hovered_prot].genome_start <= k &&
							k < c->proteins[hovered_prot].genome_end) {
						draw_list->AddLine({pos.x + f32(j) * 3.f, pos.y + f32(i) * spy + 3.f},
							{pos.x + f32(j + 1) * 3.f, pos.y + f32(i) * spy + 3.f},
							ImColor(1.f, 1.f, .1f), 3.f);
					}
					if (glm::abs(c->bound_factors[k]) > 0.01f) {
						const f32 f = c->bound_factors[k];
						draw_list->AddLine({pos.x + f32(j) * 3.f, pos.y + f32(i) * spy + 3.f},
							{pos.x + f32(j + 1) * 3.f, pos.y + f32(i) * spy + 3.f},
							f > 0.f ? ImColor(.7f, .7f, .7f, f) : ImColor(.1f, 1.f, .1f, -f), 3.f);
					}
					if (hover_tf && std::ranges::find((*hover_tf_extract)->bind_points, k) !=
						(*hover_tf_extract)->bind_points.end()) {
						draw_list->AddLine({pos.x + f32(j) * 3.f, pos.y + f32(i) * spy + 3.f},
							{pos.x + f32(j + 1) * 3.f, pos.y + f32(i) * spy + 3.f},
							ImColor(1.f, .1f, 1.f), 3.f);
					}
				}
			}
		}
		ImGui::Dummy({151.f, f32(u32((c->genome.size()+149) / 150)) * spy});
		ImGui::Separator();
		ImGui::InputText("file", &file_path);
		ImGui::SameLine();
		if (ImGui::Button("load")) {
			ext_cell.genome = load_genome(file_path);
			ext_cell.proteins.clear();
			ext_cell.bound_factors.clear();
			c = &ext_cell;
		}
	}
	ImGui::End();
}

