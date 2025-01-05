#pragma once
#include "defs.hpp"
#include <glm/vec2.hpp>

class cell {
public:
	enum class state { none, active };

	state s;
	u32 gpu_id;
	glm::vec2 pos;
	glm::vec2 vel;
	u32 div_timer;
};

