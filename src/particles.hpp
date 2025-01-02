#pragma once
#include <bit>
#include "rendering/compute.hpp"

struct particle {
	alignas(16) glm::vec2 pos;
	alignas(4) u32 debug_tags;
	alignas(4) f32 density;
	alignas(16) glm::vec2 vel;
};

class particles {
public:
	constexpr static u32 sim_frames = compile_options::frames_in_flight + 1;
	constexpr static u32 cell_count = compile_options::cells_x * compile_options::cells_y;
private:
	constexpr static u32 cell_count_ceil = std::bit_ceil(cell_count);

	constexpr static u32 counts_buff = 1;
	constexpr static u32 index_buff = 2;
	constexpr static u32 cell_buff = 3;

	constexpr static u32 count_pl = 0;
	constexpr static u32 upsweep_pl = 1;
	constexpr static u32 downsweep_pl = 2;
	constexpr static u32 write_pl = 3;
	constexpr static u32 density_pl = 4;
	constexpr static u32 update_pl = 5;

	rend::basic_compute_process<sim_frames> proc;
public:
	f32 global_density;
	f32 stiffness;
	f32 viscosity;

	particles(const rend::context &ctx, const vk::raii::Device &device, const rend::buffer_handler &bh,
		const vk::raii::DescriptorPool &dpool);
	void step(vk::CommandBuffer cmd, u32 frame);
	u32 pframe() const;
	vk::Buffer particle_buff() const;
};

