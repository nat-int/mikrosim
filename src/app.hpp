#pragma once
#include <optional>
#include "input/input.hpp"
#include "particles.hpp"
#include "rendering/preset.hpp"

class mikrosim_window : public rend::preset::simple_window {
	bool first_frame;
	vma::buffer quad_vb, quad_ib;
	std::vector<vk::raii::CommandBuffer> update_cmds;
	std::vector<vk::raii::Semaphore> update_semaphores;
	std::vector<vk::raii::Semaphore> update_render_semaphores;
	std::vector<vk::raii::ShaderModule> shaders;
	vk::raii::PipelineLayout particle_draw_pll;
	vk::raii::Pipeline particle_draw_pl;
	std::optional<particles> p;
	bool running;
	f32 view_scale;
	glm::vec2 view_position;
	f32 particle_draw_size;
	glm::mat4 vp;
	input::input_handler inp;
public:
	mikrosim_window();
	void terminate();
	void loop();
	void on_scroll(f64 dx, f64 dy);
private:
	void update_view();
};

