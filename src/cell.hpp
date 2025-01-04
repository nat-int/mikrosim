#pragma once
#include "defs.hpp"
#include <glm/vec2.hpp>

class cell {
public:
	enum class gpu_state { staged, active };

	gpu_state gs;
	u32 gpu_id;
};

