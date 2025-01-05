#pragma once
#include <bit>
#include "cell.hpp"
#include "rendering/compute.hpp"
#include "rendering/timestamp.hpp"

struct particle {
	alignas(16) glm::vec2 pos;
	alignas(4) u32 type;
	alignas(4) f32 density;
	alignas(16) glm::vec2 vel;
};
struct particle_report {
	alignas(8) glm::vec2 pos;
	alignas(8) glm::vec2 vel;
};

class particles {
public:
	constexpr static u32 sim_frames = compile_options::frames_in_flight + 1;
	constexpr static u32 cell_count = compile_options::cells_x * compile_options::cells_y;
	constexpr static u32 timestamps = 7;
private:
	constexpr static u32 cell_count_1ceil = std::bit_ceil(cell_count + 1);

	constexpr static u32 counts_buff = 1;
	constexpr static u32 index_buff = 2;
	constexpr static u32 cell_buff = 3;
	constexpr static u32 report_buff = 4;

	constexpr static u32 count_pl = 0;
	constexpr static u32 upsweep_pl = 1;
	constexpr static u32 downsweep_pl = 2;
	constexpr static u32 write_pl = 3;
	constexpr static u32 density_pl = 4;
	constexpr static u32 update_pl = 5;
	constexpr static u32 report_pl = 6;

	rend::basic_compute_process<sim_frames> proc;
	vma::buffer cell_stage;
	particle *cell_stage_map;
	// consistency is not important, approximate values are enough so the data races (gpu-cpu) here are fine
	vma::buffer reports;
	particle_report *reports_map;
	std::vector<vk::BufferCopy> curr_staged;
	u32 next_cell_stage;
	std::vector<u32> free_cells;
public:
	std::array<cell, compile_options::cell_particle_count> cells;
	f32 global_density;
	f32 stiffness;
	f32 viscosity;

	particles(const rend::context &ctx, const vk::raii::Device &device, const rend::buffer_handler &bh,
		const vk::raii::DescriptorPool &dpool);
	~particles();
	void step_cpu();
	void step_gpu(vk::CommandBuffer cmd, u32 frame, const rend::timestamps<timestamps> &ts);
	void imgui();
	u32 pframe() const;
	vk::Buffer particle_buff() const;

	void spawn_cell(glm::vec2 pos, glm::vec2 vel);
	void kill_cell(u32 gpu_id);
};

