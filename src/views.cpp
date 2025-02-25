#include "views.hpp"
#include <cinttypes>
#include <imgui.h>
#include <imgui/misc/cpp/imgui_stdlib.h>
#include <imgui_internal.h>
#include "input/input.hpp"
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
			ImGui::TableNextColumn(); ImGui::Text("%s", (info.is_genome_polymerase ? "yes" : "no"));
			ImGui::TableNextRow(); ImGui::TableNextColumn(); ImGui::TableHeader("is genome repair");
			ImGui::TableNextColumn(); ImGui::Text("%s", (info.is_genome_repair ? "yes" : "no"));
			ImGui::TableNextRow(); ImGui::TableNextColumn(); ImGui::TableHeader("is positive factor");
			ImGui::TableNextColumn(); ImGui::Text("%s", (info.is_positive_factor ? "yes" : "no"));
			ImGui::TableNextRow(); ImGui::TableNextColumn(); ImGui::TableHeader("is small struct");
			ImGui::TableNextColumn(); ImGui::Text("%s", (info.is_small_struct ? "yes" : "no"));
			ImGui::TableNextRow(); ImGui::TableNextColumn(); ImGui::TableHeader("is big struct");
			ImGui::TableNextColumn(); ImGui::Text("%s", (info.is_big_struct ? "yes" : "no"));
			ImGui::TableNextRow(); ImGui::TableNextColumn(); ImGui::TableHeader("is struct synthesizer");
			ImGui::TableNextColumn(); ImGui::Text("%s", (info.is_struct_synthesizer ? "yes" : "no"));
			ImGui::TableNextRow(); ImGui::TableNextColumn(); ImGui::TableHeader("reaction energy change");
			ImGui::TableNextColumn(); ImGui::Text("%" PRIi32, info.energy_balance);
			ImGui::TableNextRow(); ImGui::TableNextColumn(); ImGui::TableHeader("stability");
			ImGui::TableNextColumn(); ImGui::Text("%.3lf", f64(info.stability));
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

void metabolism_subview::set(const cell &c) {
	nodes.clear();
	edges.clear();
	for (usize i = 0; i < c.proteins.size(); i++) {
		const protein &p = c.proteins[i];
		std::visit(overloaded{
			[](const empty_protein &) {},
			[](const transcription_factor &) {},
			[this, i](const chem_protein &cp) {
				if (cp.K > 1.f)
					add_prot(i, cp.inputs, cp.outputs, false);
				else
					add_prot(i, cp.outputs, cp.inputs, false);
			},
			[this, i](const special_chem_protein &scp) {
				if (scp.K > 1.f)
					add_prot(i, scp.inputs, scp.outputs, true);
				else
					add_prot(i, scp.outputs, scp.inputs, true);
			},
			[](const struct_protein &) {},
			}, p.effect);
	}
	view_pos = {0.f, 0.f};
	view_scale = 1.f;
	lock = false;
}
void metabolism_subview::update() {
	if (lock)
		return;
	for (const edge &e : edges) {
		glm::vec2 d = nodes[e.b].pos - nodes[e.a].pos;
		f32 l = std::max(glm::length(d), .5f);
		f32 fix = .01f - .6f / l;
		nodes[e.a].pos += d * fix;
		nodes[e.b].pos -= d * fix;
	}
	for (auto i = nodes.begin(); i != nodes.end(); i++) {
		for (auto j = i+1; j != nodes.end(); j++) {
			glm::vec2 d = j->pos - i->pos;
			f32 l2 = glm::dot(d, d);
			if (l2 < 2500.f) {
				f32 fix = -10.f / std::max(l2, 0.1f);
				i->pos += d * fix;
				j->pos -= d * fix;
			}
		}
		i->pos *= 0.999f;
	}
}

static bool clip_line(glm::vec2 &a, glm::vec2 &b, const glm::vec2 sz) {
	f32 ta = 0.f, tb = 1.f;
	const glm::vec2 d = b - a;
	auto clip = [&ta, &tb](f32 d, f32 sp) {
		if (abs(d) < 0.001f) {
			if (sp < 0.f) ta = 1.f;
			return;
		}
		float r = sp / d;
		if (d < 0) {
			ta = std::max(ta, r);
		} else {
			tb = std::min(tb, r);
		}
	};
	clip(-d.x, a.x);
	clip(d.x,  sz.x - a.x);
	clip(-d.y, a.y);
	clip(d.y,  sz.y - a.y);
	if (tb <= ta) return false;
	b = a + tb * d;
	a = a + ta * d;
	return true;
}

void metabolism_subview::draw(const input::input_handler &inp, const compounds &comps) {
	static constexpr f32 w = 300.f;
	static constexpr f32 h = 150.f;
	static constexpr f32 padding = 5.f;
	static constexpr glm::vec2 pad = {padding, padding};
	static constexpr glm::vec2 sz = {w, h};
	ImDrawList *draw_list = ImGui::GetWindowDrawList();
	ImVec2 pos = ImGui::GetCursorScreenPos();
	glm::vec2 pv = glm::vec2{pos.x, pos.y};
	f32 lw = view_scale * 3.f;
	for (const node &n : nodes) {
		glm::vec2 p = (n.pos - view_pos) * view_scale + sz * .5f;
		if (glm::all(glm::greaterThanEqual(p, pad) && glm::lessThanEqual(p, sz - pad))) {
			p += pv;
			f32 r = view_scale * 8.f;
			if (n.k == node::kind::protein) {
				draw_list->AddCircle({p.x, p.y}, r, ImColor(1.f, 1.f, 1.f), 6, lw);
			} else {
				draw_list->AddLine({p.x, p.y-r}, {p.x, p.y}, comp_cols[comps.infos[n.id].parts[0]], lw);
				draw_list->AddLine({p.x+r, p.y}, {p.x, p.y}, comp_cols[comps.infos[n.id].parts[1]], lw);
				draw_list->AddLine({p.x, p.y+r}, {p.x, p.y}, comp_cols[comps.infos[n.id].parts[2]], lw);
				draw_list->AddLine({p.x-r, p.y}, {p.x, p.y}, comp_cols[comps.infos[n.id].parts[3]], lw);
			}
		}
	}
	for (const edge &e : edges) {
		glm::vec2 pa = (nodes[e.a].pos - view_pos) * view_scale + sz * .5f;
		glm::vec2 pb = (nodes[e.b].pos - view_pos) * view_scale + sz * .5f;
		const glm::vec2 d = pb - pa;
		const glm::vec2 nd = glm::normalize(d);
		pa += nd * 10.f * view_scale;
		pb -= nd * 10.f * view_scale;
		if (clip_line(pa, pb, sz)) {
			const glm::vec2 ond = {nd.y, -nd.x};
			pa += pv;
			pb += pv;
			draw_list->AddLine({pa.x, pa.y}, {pb.x, pb.y}, e.col, lw);
			const glm::vec2 pba = pb + (ond - nd) * 5.f * view_scale;
			draw_list->AddLine({pba.x, pba.y}, {pb.x, pb.y}, e.col, lw);
			const glm::vec2 pbb = pb + (-ond - nd) * 5.f * view_scale;
			draw_list->AddLine({pbb.x, pbb.y}, {pb.x, pb.y}, e.col, lw);
		}
	}
	ImGui::InvisibleButton("##msv", {w, h});
	if (ImGui::IsItemHovered()) {
		if (ImGui::IsMouseDragging(ImGuiMouseButton_Middle)) {
			view_pos -= inp.get_cursor_delta() / view_scale;
		}
		if (ImGui::IsMouseDragging(ImGuiMouseButton_Right)) {
			view_scale *= powf(1.05f, inp.get_cursor_delta().y);
		}
		if (inp.is_mouse_held(GLFW_MOUSE_BUTTON_LEFT)) {
			const glm::vec2 cp = (inp.get_cursor_pos() - pv - sz * .5f) / view_scale + view_pos;
			if (inp.is_mouse_down(GLFW_MOUSE_BUTTON_LEFT)) {
				drag = usize(-1);
				f32 drag_d2 = 2500.f;
				for (usize i = 0; i < nodes.size(); i++) {
					const glm::vec2 d = nodes[i].pos - cp;
					f32 d2 = glm::dot(d, d);
					if (d2 < drag_d2) {
						drag_d2 = d2;
						drag = i;
					}
				}
			}
			if (drag < nodes.size()) {
				nodes[drag].pos = cp;
			}
		}
	}
}
void metabolism_subview::add_prot(usize id, const std::vector<u8> &inp, const std::vector<u8> &out, bool spec) {
	usize ni = nodes.size();
	nodes.push_back({{randf() * 1000.f - 500.f, randf() * 1000.f - 500.f}, id, node::kind::protein});
	ImU32 col = spec ? ImColor(1.f, 1.f, 0.f, .7f) : ImColor(1.f, 1.f, 1.f, .7f);
	for (u8 i : inp) {
		add_comp_edge(ni, true, i, col);
	}
	for (u8 i : out) {
		add_comp_edge(ni, false, i, col);
	}
}
void metabolism_subview::add_comp_edge(usize from, bool rev, u8 comp, ImU32 col) {
	for (usize i = 0; i < nodes.size(); i++) {
		if (nodes[i].k == node::kind::compound && nodes[i].id == comp) {
			edges.push_back({rev ? i : from, rev ? from : i, col});
			return;
		}
	}
	usize j = nodes.size();
	nodes.push_back({{randf() * 1000.f - 500.f, randf() * 1000.f - 500.f}, comp, node::kind::compound});
	edges.push_back({rev ? j : from, rev ? from : j, col});
}

cell_view::cell_view() : ext_cell(0, {}, {}), c(&ext_cell) {
	ext_cell.genome = {};
	file_path = "./basic.genome";
	save_file_path = "./qs.genome";
	follow = false;
	msv.set(ext_cell);
}
void cell_view::draw(const input::input_handler &inp, const compounds &comps, protein_view &pv) {
	if (ext_cell.bound_factors.empty() && !ext_cell.genome.empty()) {
		ext_cell.analyze(comps);
		if (c == &ext_cell)
			msv.set(ext_cell);
	}
	if (ImGui::Begin("cell view")) {
		ImGui::InputText("load file", &file_path);
		ImGui::SameLine();
		if (ImGui::Button("load")) {
			ext_cell.genome = load_genome(file_path);
			ext_cell.proteins.clear();
			ext_cell.bound_factors.clear();
			set(&ext_cell);
		}
		ImGui::InputText("save file", &save_file_path);
		ImGui::SameLine();
		if (ImGui::Button("save")) {
			save_genome(save_file_path, *c);
		}
		ImGui::Separator();
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
			ImGui::TableNextRow(); ImGui::TableNextColumn(); ImGui::TableHeader("health");
			ImGui::TableNextColumn(); ImGui::Text("%" PRIu32, c->health);
			ImGui::TableNextRow(); ImGui::TableNextColumn(); ImGui::TableHeader("membrane states");
			ImGui::TableNextColumn(); ImGui::Text("s %" PRIu32 "/%" PRIu32 " b %" PRIu32 "/%" PRIu32
					" + %" PRIu32, c->small_struct, c->small_struct_effective, c->big_struct,
					c->big_struct_effective, c->membrane_add);
			ImGui::EndTable();
		}
		ImGui::Checkbox("follow", &follow);
		ImGui::SameLine();
		ImGui::Checkbox("lock graph", &msv.lock);

		constexpr static f32 spy = 7.f;
		constexpr static usize llen = 100;
		const ImVec2 gpos = ImGui::GetCursorScreenPos();
		ImGui::Dummy({f32(llen), f32(u32((c->genome.size()+llen-1) / llen)) * spy});
		ImGui::Separator();
		const ImVec2 cpos = ImGui::GetCursorScreenPos();
		constexpr static f32 csp = 4.f; // spacing of bars
		constexpr static f32 cbh = 25.f; // bar height
		ImGui::Dummy({csp * compounds::count, cbh + 28.f});
		msv.update();
		msv.draw(inp, comps);

		usize hovered_prot = usize(-1);
		usize empty_count = 0;
		if (ImGui::BeginTable("##proteins", 4)) {
			ImGui::TableNextRow(); ImGui::TableNextColumn();
			ImGui::TableNextColumn(); ImGui::TableHeader("genome range");
			ImGui::TableNextColumn(); ImGui::TableHeader("concentration");
			ImGui::TableNextColumn(); ImGui::TableHeader("effect");
			usize i = 0;
			std::vector<usize> real_ids;
			for (protein &p : c->proteins) {
				if (std::holds_alternative<empty_protein>(p.effect)) {
					empty_count++;
					i++;
					continue;
				}
				real_ids.push_back(i);
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
					[](const empty_protein &) { ImGui::Text("-"); },
					[](const transcription_factor &tf) { ImGui::Text("TF %.3lf", f64(tf.curr_effect)); },
					[](const chem_protein &cp) { ImGui::Text("ENZ, K=%.3lf", f64(cp.K)); },
					[](const special_chem_protein &scp) {
						switch (scp.act) {
						case special_action::division: ImGui::Text("genome polymerase"); break;
						case special_action::repair: ImGui::Text("genome repair"); break;
						case special_action::struct_synthesize: ImGui::Text("struct synthesizer"); break;
						default: ImGui::Text("unknown special action!"); break;
						}
					},
					[](const struct_protein &sp) { ImGui::Text(sp.big ? "big struct" : "small struct"); },
				}, p.effect);
				ImGui::PopID();
			}
			hovered_prot = usize(ImGui::TableGetHoveredRow()-1);
			hovered_prot = hovered_prot < real_ids.size() ? real_ids[hovered_prot] : hovered_prot;
			ImGui::EndTable();
		}
		ImGui::Text("+%zu empty proteins", empty_count);

		ImDrawList *draw_list = ImGui::GetWindowDrawList();
		const bool hover_tf = hovered_prot < c->proteins.size() &&
			std::holds_alternative<transcription_factor>(c->proteins[hovered_prot].effect);
		const std::optional<const transcription_factor *> hover_tf_extract = hover_tf ?
			std::optional<const transcription_factor *>(
				&std::get<transcription_factor>(c->proteins[hovered_prot].effect)) : std::nullopt;
		for (usize i = 0; i < (c->genome.size() + llen-1) / llen; i++) {
			for (usize j = 0; j < llen; j++) {
				const usize k = llen * i + j;
				if (k < c->genome.size()) {
					draw_list->AddLine({gpos.x + f32(j) * 3.f, gpos.y + f32(i) * spy},
						{gpos.x + f32(j + 1) * 3.f, gpos.y + f32(i) * spy},
						c->genome[k] ? ImColor(.1f, .1f, 1.f) : ImColor(1.f, .1f, .1f), 3.f);
					if (hovered_prot < c->proteins.size() && c->proteins[hovered_prot].genome_start <= k &&
							k < c->proteins[hovered_prot].genome_end) {
						draw_list->AddLine({gpos.x + f32(j) * 3.f, gpos.y + f32(i) * spy + 3.f},
							{gpos.x + f32(j + 1) * 3.f, gpos.y + f32(i) * spy + 3.f},
							ImColor(1.f, 1.f, .1f), 3.f);
					}
					if (k < c->bound_factors.size() && glm::abs(c->bound_factors[k]) > 0.01f) {
						const f32 f = c->bound_factors[k];
						draw_list->AddLine({gpos.x + f32(j) * 3.f, gpos.y + f32(i) * spy + 3.f},
							{gpos.x + f32(j + 1) * 3.f, gpos.y + f32(i) * spy + 3.f},
							f > 0.f ? ImColor(.7f, .7f, .7f, f) : ImColor(.1f, 1.f, .1f, -f), 3.f);
					}
					if (hover_tf && std::ranges::find((*hover_tf_extract)->bind_points, k) !=
						(*hover_tf_extract)->bind_points.end()) {
						draw_list->AddLine({gpos.x + f32(j) * 3.f, gpos.y + f32(i) * spy + 3.f},
							{gpos.x + f32(j + 1) * 3.f, gpos.y + f32(i) * spy + 3.f},
							ImColor(1.f, .1f, 1.f), 3.f);
					}
				}
			}
		}

		const bool hover_cp = hovered_prot < c->proteins.size() &&
			std::holds_alternative<chem_protein>(c->proteins[hovered_prot].effect);
		const bool hover_scp = hovered_prot < c->proteins.size() &&
			std::holds_alternative<special_chem_protein>(c->proteins[hovered_prot].effect);
		for (usize i = 0; i < compounds::count; i++) {
			glm::vec3 bg = {0.f, 0.f, 0.f};
			glm::vec3 fg = {.7f, .7f, .7f};
			if (hover_cp || hover_scp) {
				const auto &eff = c->proteins[hovered_prot].effect;
				const std::vector<u8> *inp = hover_cp ? &std::get<chem_protein>(eff).inputs : &std::get<special_chem_protein>(eff).inputs;
				const std::vector<u8> *out = hover_cp ? &std::get<chem_protein>(eff).outputs : &std::get<special_chem_protein>(eff).outputs;
				for (u8 j : *inp) { if (j == i) bg.r += 1.f; }
				for (u8 j : *out) { if (j == i) bg.g += 1.f; }
			}
			if (hovered_prot < c->proteins.size()) {
				for (catalyzer j : c->proteins[hovered_prot].catalyzers) {
					if (j.compound == i) bg.b += 1.f;
				}
			}
			for (u8 j : block_compounds) { if (comps.atoms_to_id[j] == i) bg *= 1.5f; }
			bg = glm::clamp(bg, 0.f, .5f);
			fg = bg.x+bg.y+bg.z < 0.01f ? glm::vec3{.7f, .7f, .7f} : bg * 2.f;
			draw_list->AddLine({cpos.x + f32(i) * csp, cpos.y},
				{cpos.x + f32(i) * csp, cpos.y + cbh + 1.f}, ImColor(bg.r, bg.g, bg.b), 3.f);
			draw_list->AddLine({cpos.x + f32(i) * csp, cpos.y + cbh * (1.f - .5f * comps.at(i, c->gpu_id))},
				{cpos.x + f32(i) * csp, cpos.y + cbh + 1.f}, ImColor(fg.r, fg.g, fg.b), 3.f);
			const f32 center_y = cpos.y + cbh + f32(i % 3) * 10.f + 5.f;
			draw_list->AddLine({cpos.x + f32(i) * csp, center_y - 4.f},
				{cpos.x + f32(i) * csp, center_y}, comp_cols[comps.infos[i].parts[0]], 2.f);
			draw_list->AddLine({cpos.x + f32(i) * csp + 4.f, center_y},
				{cpos.x + f32(i) * csp, center_y}, comp_cols[comps.infos[i].parts[1]], 2.f);
			draw_list->AddLine({cpos.x + f32(i) * csp, center_y + 4.f},
				{cpos.x + f32(i) * csp, center_y}, comp_cols[comps.infos[i].parts[2]], 2.f);
			draw_list->AddLine({cpos.x + f32(i) * csp - 4.f, center_y},
				{cpos.x + f32(i) * csp, center_y}, comp_cols[comps.infos[i].parts[3]], 2.f);
		}
	}
	ImGui::Separator();
	if (ImGui::Button("test")) {
		ext_cell.test();
	}
	ImGui::End();
}
void cell_view::set(cell *_c) {
	c = _c;
	msv.set(*c);
}

