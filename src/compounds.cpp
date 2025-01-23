#include "compounds.hpp"
#include <algorithm>
#include <imgui.h>
#include "log/log.hpp"

compound::compound(u8 a) : atoms(a) {
	atoms = std::min({atoms, rot(1), rot(2), rot(3)});
}
compound::compound(u8 a, u8 b, u8 c, u8 d) {
	atoms = u8(a << u8(6) | b << u8(4) | c << u8(2) | d);
	atoms = std::min({atoms, rot(1), rot(2), rot(3)});
}
u8 compound::rot(u8 amount) const {
	return std::rotr<u8>(atoms, amount * 2);
}

void compound_info::imgui(f32 sz, f32 w, f32 sp) const {
	ImDrawList *draw_list = ImGui::GetWindowDrawList();
	static const ImU32 cols[] = {ImColor(1.f, .2f, .2f), ImColor(.1f, 1.f, .1f),
		ImColor(.1f, .1f, 1.f), ImColor(.6f, .6f, .6f)};
	ImVec2 pos = ImGui::GetCursorScreenPos();
	draw_list->AddLine({pos.x + sz/2, pos.y + sz/2}, {pos.x + sz/2, pos.y}, cols[parts[0]], w);
	draw_list->AddLine({pos.x + sz/2, pos.y + sz/2}, {pos.x + sz, pos.y + sz/2}, cols[parts[1]], w);
	draw_list->AddLine({pos.x + sz/2, pos.y + sz/2}, {pos.x + sz/2, pos.y + sz}, cols[parts[2]], w);
	draw_list->AddLine({pos.x + sz/2, pos.y + sz/2}, {pos.x, pos.y + sz/2}, cols[parts[3]], w);
	ImGui::Dummy({sz+sp, sz+sp});
}

compounds::compounds() {
	u8 id = 0;
	for (u32 i = 0; i < 256; i++) {
		u8 a = compound(u8(i)).atoms;
		if (a == i) {
			auto &inf = infos[id];
			inf = compound_info{{u8(a>>6&3),u8(a>>4&3),u8(a>>2&3),u8(a&3)},0};
			// arbitary energy function, inspired by simple aromatic compounds
			static constexpr u16 base_energy[4] = { 51, 38, 46, 16 };
			static constexpr i16 draw_coef[4] = { 7, 1, -2, 0 };
			static constexpr i16 draw[4] = { 5, 2, -3, 0 };
			static constexpr i16 draw_op[4] = { 3, -1, -1, 0 };
			for (u32 j = 0; j < 4; j++) {
				i16 d = draw[inf.parts[(j+1)%4]] + draw_op[inf.parts[(j+2)%4]] + draw[inf.parts[(j+3)%4]];
				inf.energy += base_energy[inf.parts[j]] + d * draw_coef[inf.parts[j]];
			}
			atoms_to_id[i] = id++;
		}
	}
	if (id != count)
	logs::debugln("compounds", "there are ", u32(id), " different compounds");
}
f32 &compounds::at(usize compound, usize particle) {
	return concentrations[compound][particle];
}

