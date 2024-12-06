#pragma once
#include "rendering/preset.hpp"

class mikrosim_window : public rend::preset::simple_window {
public:
	mikrosim_window();
	void loop();
};

