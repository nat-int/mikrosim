#include "parse.hpp"
#include "log/log.hpp"
#include <fstream>

std::vector<bool> load_genome(const std::string &file) {
	std::ifstream f(file);
	if (!f.good()) {
		logs::errorln("mikrosim", "failed to open genome file ", file);
		return {};
	}
	std::vector<bool> out;
	char c;
	while (f >> c) {
		switch (c) {
		case 'T': out.push_back(true); break;
		case 'F': out.push_back(false); break;
		case '0': out.insert(out.end(), {false, false, false, false}); break;
		case '1': out.insert(out.end(), {true, false, false, false}); break;
		case '2': out.insert(out.end(), {false, true, false, false}); break;
		case '3': out.insert(out.end(), {true, true, false, false}); break;
		case '4': out.insert(out.end(), {false, false, true, false}); break;
		case '5': out.insert(out.end(), {true, false, true, false}); break;
		case '6': out.insert(out.end(), {false, true, true, false}); break;
		case '7': out.insert(out.end(), {true, true, true, false}); break;
		case '8': out.insert(out.end(), {false, false, false, true}); break;
		case '9': out.insert(out.end(), {true, false, false, true}); break;
		case 'a': out.insert(out.end(), {false, true, false, true}); break;
		case 'b': out.insert(out.end(), {true, true, false, true}); break;
		case 'c': out.insert(out.end(), {false, false, true, true}); break;
		case 'd': out.insert(out.end(), {true, false, true, true}); break;
		case 'e': out.insert(out.end(), {false, true, true, true}); break;
		case 'f': out.insert(out.end(), {true, true, true, true}); break;
		case ';': while (f >> c) { if (c == '\n') break; } break;
		default: break;
		}
	}
	return out;
}

std::pair<std::array<force_block, 4>, std::array<chem_block, 4>> load_blocks(const std::string &file) {
	std::ifstream f(file);
	if (!f.good()) {
		logs::errorln("mikrosim", "failed to open blocks file ", file);
		return {};
	}
	std::pair<std::array<force_block, 4>, std::array<chem_block, 4>> out{};
	for (usize i = 0; i < 4; i++) {
		f >> out.first[i].pos.x;
		f >> out.first[i].pos.y;
		f >> out.first[i].extent.x;
		f >> out.first[i].extent.y;
		f >> out.first[i].cartesian_force.x;
		f >> out.first[i].cartesian_force.y;
		f >> out.first[i].polar_force.x;
		f >> out.first[i].polar_force.y;
	}
	for (usize i = 0; i < 4; i++) {
		f >> out.second[i].pos.x;
		f >> out.second[i].pos.y;
		f >> out.second[i].extent.x;
		f >> out.second[i].extent.y;
		f >> out.second[i].target_conc;
		f >> out.second[i].lerp_strength;
		f >> out.second[i].hard_delta;
		f >> out.second[i].comp;
	}
	return out;
}

void save_blocks(const std::string &file, const std::pair<std::array<force_block, 4>,
	std::array<chem_block, 4>> &blocks) {
	std::ofstream f(file);
	if (!f.good()) {
		logs::errorln("mikrosim", "failed to open blocks file (for writing) ", file);
		return;
	}
	for (usize i = 0; i < 4; i++) {
		f << blocks.first[i].pos.x << ' ';
		f << blocks.first[i].pos.y << ' ';
		f << blocks.first[i].extent.x << ' ';
		f << blocks.first[i].extent.y << "\n\t";
		f << blocks.first[i].cartesian_force.x << ' ';
		f << blocks.first[i].cartesian_force.y << "\n\t";
		f << blocks.first[i].polar_force.x << ' ';
		f << blocks.first[i].polar_force.y << '\n';
	}
	for (usize i = 0; i < 4; i++) {
		f << blocks.second[i].pos.x << ' ';
		f << blocks.second[i].pos.y << ' ';
		f << blocks.second[i].extent.x << ' ';
		f << blocks.second[i].extent.y << "\n\t";
		f << blocks.second[i].target_conc << ' ';
		f << blocks.second[i].lerp_strength << "\n\t";
		f << blocks.second[i].hard_delta << "\n\t";
		f << blocks.second[i].comp << '\n';
	}
}

