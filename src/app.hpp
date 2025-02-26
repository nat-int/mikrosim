#pragma once
#include <optional>
#include "input/input.hpp"
#include "particles.hpp"
#include "rendering/preset.hpp"
#include "rendering/timestamp.hpp"
#include "views.hpp"

struct std140_f32 {
	alignas(16) f32 v;
};

class mikrosim_window : public rend::preset::simple_window {
	static constexpr u32 render_timestamps = 2;
	bool first_frame;
	vma::buffer quad_vb, quad_ib;
	vma::buffer concs_stage;
	std140_f32 *concs_stage_map;
	vma::buffer concs_isb;
	vma::buffer force_quad;
	vma::buffer chem_quad;
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
	u32 disp_compound;
	input::input_handler inp;
	f32 prev_frame;
	f32 dt;
	f32 sec_timer;
	u32 frame_counter;
	u32 fps;
	protein_view pv;
	cell_view cv;
	std::string blocks_load_path;
	std::string blocks_save_path;
	u32 last_struct_spawn;
	float set_conc_target;
	vk::raii::Fence multistep_fence;
	u32 steps_per_frame;
	u32 curr_sync_semaphore;
public:
	mikrosim_window();
	void terminate();
	void loop();
	void update();
	void advance_sim(bool signal_render_semaphore, bool fence);
	void render(vk::CommandBuffer cmd, const rend::simple_mesh &bg_mesh,
		const rend::simple_mesh &crosshair_mesh, vk::Buffer particle_buff);
	void on_scroll(f64 dx, f64 dy);
private:
	void update_view();
};

