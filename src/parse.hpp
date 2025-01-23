#pragma once
#include <string>
#include <utility>
#include <vector>
#include "particles.hpp"

std::vector<bool> load_genome(const std::string &file);
std::pair<std::array<force_block, 4>, std::array<chem_block, 4>> load_blocks(const std::string &file);
void save_blocks(const std::string &file, const std::pair<std::array<force_block, 4>,
	std::array<chem_block, 4>> &blocks);

