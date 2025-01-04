#pragma once
#include <optional>
#include "input/input.hpp"
#include "particles.hpp"
#include "rendering/preset.hpp"
#include "rendering/timestamp.hpp"

class mikrosim_window : public rend::preset::simple_window {
	static constexpr u32 render_timestamps = 2;
	bool first_frame;
	vma::buffer quad_vb, quad_ib;
	std::vector<vk::raii::CommandBuffer> update_cmds;
	std::vector<vk::raii::Semaphore> update_semaphores;
	std::vector<vk::raii::Semaphore> update_render_semaphores;
	std::vector<vk::raii::ShaderModule> shaders;
	vk::raii::PipelineLayout particle_draw_pll;
	vk::raii::Pipeline particle_draw_pl;
	rend::timestamps<particles::timestamps> compute_ts;
	rend::timestamps<render_timestamps> render_ts;
	std::optional<particles> p;
	bool running;
	f32 view_scale;
	glm::vec2 view_position;
	f32 particle_draw_size;
	glm::mat4 vp;
	input::input_handler inp;
	f32 prev_frame;
	f32 dt;
	f32 sec_timer;
	u32 frame_counter;
	u32 fps;
public:
	mikrosim_window();
	void terminate();
	void loop();
	void update();
	void advance_sim(u32 rframe);
	void render(vk::CommandBuffer cmd, const rend::simple_mesh &bg_mesh, vk::Buffer particle_buff);
	void on_scroll(f64 dx, f64 dy);
private:
	void update_view();
};

