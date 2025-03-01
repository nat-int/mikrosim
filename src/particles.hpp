#pragma once
#include <bit>
#include <memory>
#include "cell.hpp"
#include "compounds.hpp"
#include "rendering/compute.hpp"
#include "rendering/timestamp.hpp"
#include "util.hpp"

struct particle {
	alignas(16) glm::vec2 pos;
	alignas(4) u32 type;
	alignas(4) f32 density;
	alignas(16) glm::vec2 vel;

	alignas(4) u32 bond_a;
	alignas(4) u32 bond_b;

	void set(glm::vec2 _pos, glm::vec2 _vel, u32 _type);
};
struct particle_report {
	alignas(8) glm::vec2 pos;
	alignas(8) glm::vec2 vel;
	alignas(16) glm::vec2 spos;
};

struct chem_block {
	glm::vec2 pos;
	f32 target_conc;
	f32 lerp_strength;
	glm::vec2 extent;
	f32 hard_delta;
	uint comp;
};
struct force_block {
	glm::vec2 pos;
	glm::vec2 extent;
	glm::vec2 cartesian_force;
	glm::vec2 polar_force;
};
struct update_uniform {
	force_block fbs[4];
	chem_block chbs[4];
};
struct struct_particle {
	bool active;
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
	constexpr static u32 concs_buff = 5;
	constexpr static u32 forces_buff = 6;

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
	vma::buffer forces_stage;
	glm::vec4 *forces_stage_map;
	std::vector<vk::BufferCopy> curr_staged;
	u32 next_cell_stage;
	u32 next_bond_stage;
	// consistency is not important, approximate values are enough so the data races (gpu-cpu) here are fine
	vma::buffer reports;
	particle_report *reports_map;
	vma::buffer concs_upload;
	glm::vec4 *concs_upload_map;
	vma::buffer concs_download;
	glm::vec4 *concs_download_map;
	u32 next_mix_concs;
	std::vector<u32> free_cells;
	std::vector<u32> free_structs;
public:
	std::array<cell, compile_options::cell_particle_count> cells;
	std::array<struct_particle, compile_options::struct_particle_count> structs;
	std::array<glm::uvec2, compile_options::membrane_particle_count+compile_options::struct_particle_count> bonds;
	std::unique_ptr<compounds> comps;
	cpu_timestamps cpu_ts;
	f32 global_density;
	f32 stiffness;
	f32 viscosity;
	std::array<force_block, 4> force_blocks;
	std::array<chem_block, 4> chem_blocks;
	rend::mapped_dedicated_uniform_buffer<update_uniform> blocks_uniform;
	f32 block_opacity;

	particles(const rend::context &ctx, const vk::raii::Device &device, const rend::buffer_handler &bh,
		const vk::raii::DescriptorPool &dpool);
	~particles();
	void step_cpu();
	void tick_cell(bool protein_creation, u32 cell_id);
	void step_gpu(vk::CommandBuffer cmd, const rend::timestamps<timestamps> &ts);
	void imgui();
	u32 pframe() const;
	vk::Buffer particle_buff() const;
	void debug_dump() const;

	particle *get_cell_stage(u32 target_gpu_id);
	usize spawn_cell(glm::vec2 pos, glm::vec2 vel);
	void kill_cell(u32 gpu_id);
	usize spawn_membrane(glm::vec2 pos, glm::vec2 vel, u32 cell_id, u32 type=4);
	void kill_membrane(u32 gpu_id);
	bool kill_branch(u32 at, bool dir, u32 start);
	usize spawn_struct(glm::vec2 pos, glm::vec2 vel);
	void kill_struct(u32 gpu_id);
	void bond(u32 gi_a, u32 gi_b);
	void unbond(u32 gi_a, u32 gi_b);
};

