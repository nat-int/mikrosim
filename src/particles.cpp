#include "particles.hpp"
#include "rendering/pipeline.hpp"

static f32 randf() {
	if constexpr (RAND_MAX > 0xffffu) { return (u32(rand()) % 0xffffu) / f32(0xffffu); }
	else { return f32(rand()) / f32(RAND_MAX); }
}
static glm::vec2 randv() { return {randf(), randf()}; }
static glm::vec2 randuv() { f32 a = randf() * glm::pi<f32>() * 2; return {glm::cos(a), glm::sin(a)}; }

struct cw_push { u32 pcount; };
struct sweep_push { u32 invocs; u32 off; };
struct density_push { u32 pcount; };
struct update_push { u32 pcount; f32 global_density; f32 stiffness; f32 viscosity; };

struct particle_id {
	alignas(16) glm::vec2 pos;
	alignas(4) u32 type;
	alignas(4) f32 density;
	alignas(16) glm::vec2 vel;
	alignas(4) u32 id;
};

particles::particles(const rend::context &ctx, const vk::raii::Device &device,
	const rend::buffer_handler &bh, const vk::raii::DescriptorPool &dpool) : proc(device, bh),
	cell_stage(nullptr) {
	std::vector<particle> particle_data(compile_options::particle_count);
	for (u32 i = 0; i < compile_options::particle_count; i++) {
		particle_data[i].pos = randv() * glm::fvec2{compile_options::cells_x, compile_options::cells_y};
		particle_data[i].vel = randuv() / 1000.f;
		particle_data[i].type = i < compile_options::env_particle_count ? 1 : 0;
	}
	proc.set_main_buffer(particle_data, vk::BufferUsageFlagBits::eVertexBuffer |
		vk::BufferUsageFlagBits::eTransferDst);
	proc.add_buffer(cell_count_1ceil * 16, vk::BufferUsageFlagBits::eTransferDst);
	proc.add_buffer(compile_options::particle_count * 16);
	proc.add_buffer(compile_options::particle_count * sizeof(particle_id));
	for (u32 i = 0; i < sim_frames; i++) {
		ctx.set_object_name(device, *proc.main_buffer[i], ("particle main buffer #"+std::to_string(i)).c_str());
	}
	ctx.set_object_name(*device, *proc.proc_buffers[counts_buff-1], "particle counts buffer");
	ctx.set_object_name(*device, *proc.proc_buffers[index_buff-1], "particle index/density buffer");
	ctx.set_object_name(*device, *proc.proc_buffers[cell_buff-1], "particle cell buffer");

	cell_stage = bh.get_alloc().create_buffer({{}, sizeof(particle)*compile_options::cell_particle_count*
		sim_frames, vk::BufferUsageFlagBits::eTransferSrc},
		{vma::allocation_create_flag_bits::eHostAccessRandom, vma::memory_usage::eAuto});
	cell_stage_map = static_cast<particle *>(cell_stage.map());

	logs::infoln("particles", compile_options::particle_count, " max particles, ", cell_count, " cells");
	logs::infoln("particles", "counts buffer has ", cell_count_1ceil, " entries");

	proc.add_main_buffer_pipeline<cw_push>(dpool, "./shaders/count.comp.spv", {counts_buff, index_buff}, 1);
	proc.add_proc_buffer_pipeline<sweep_push>(dpool, "./shaders/upsweep.comp.spv", {counts_buff});
	proc.add_proc_buffer_pipeline<sweep_push>(dpool, "./shaders/downsweep.comp.spv", {counts_buff});
	proc.add_main_buffer_pipeline<cw_push>(dpool, "./shaders/write.comp.spv", {counts_buff, index_buff, cell_buff}, 1);
	proc.add_main_buffer_pipeline<density_push>(dpool, "./shaders/density.comp.spv", {counts_buff, index_buff, cell_buff}, 1);
	proc.add_main_buffer_pipeline<update_push>(dpool, "./shaders/update.comp.spv", {counts_buff, cell_buff}, 2);
	ctx.set_object_name(*device, *proc.pipelines[count_pl].pipeline, "particle count pipeline");
	ctx.set_object_name(*device, *proc.pipelines[upsweep_pl].pipeline, "particle upsweep pipeline");
	ctx.set_object_name(*device, *proc.pipelines[downsweep_pl].pipeline, "particle downsweep pipeline");
	ctx.set_object_name(*device, *proc.pipelines[write_pl].pipeline, "particle write pipeline");
	ctx.set_object_name(*device, *proc.pipelines[density_pl].pipeline, "particle density pipeline");
	ctx.set_object_name(*device, *proc.pipelines[update_pl].pipeline, "particle update pipeline");

	next_cell_stage = 0;
	for (u32 i = compile_options::particle_count-1; i >= compile_options::env_particle_count; i--) {
		free_cells.push_back(i);
	}

	global_density = 2.f;
	stiffness = 5.f;
	viscosity = 1.f;
}
particles::~particles() { cell_stage.unmap(); }

void particles::step(vk::CommandBuffer cmd, u32 frame, const rend::timestamps<timestamps> &ts) {
	static_cast<void>(frame);
	ts.reset(cmd);
	ts.stamp(cmd, vk::PipelineStageFlagBits::eTopOfPipe, 0);
	auto p = proc.start(cmd);
	glm::vec3 pkernel{(compile_options::particle_count+63)/64, 1, 1};
	if (!curr_staged.empty()) {
		cmd.copyBuffer(*cell_stage, *p.mbuff(), curr_staged);
	}
	p.fill_proc_buffer(counts_buff);
	rend::barrier::transfer_compute(cmd, [this, &p](rend::barrier &b) {
		p.barrier_proc_buffers(b, rend::barrier::trans_w, rend::barrier::shader_rw, {counts_buff});
		if (!curr_staged.empty()) {
			p.barrier_main_buffer(b, rend::barrier::trans_w, rend::barrier::shader_r);
		}
	});
	curr_staged.clear();
	p.bind_dispatch(count_pl, cw_push{compile_options::particle_count}, pkernel);
	constexpr static usize scan_steps = std::bit_width(cell_count_1ceil)-1;
	for (usize i = 0; i < scan_steps-1; i++) {
		rend::barrier::compute_compute(cmd, [&p](rend::barrier &b) {
			p.barrier_proc_buffers(b, rend::barrier::shader_rw, rend::barrier::shader_rw, {counts_buff});
		});
		if (i == 0) {
			ts.stamp(cmd, vk::PipelineStageFlagBits::eComputeShader, 1);
		}
		u32 off = 1 << i;
		u32 invocs = cell_count_1ceil >> (i + 1);
		p.bind_dispatch(upsweep_pl, sweep_push{invocs, off}, {(invocs+63)/64, 1, 1});
	}
	rend::barrier::compute_transfer(cmd, [&p](rend::barrier &b) {
		p.barrier_proc_buffers(b, rend::barrier::shader_rw, rend::barrier::trans_w, {counts_buff});
	});
	ts.stamp(cmd, vk::PipelineStageFlagBits::eComputeShader, 2);
	cmd.fillBuffer(*p.pbuff(counts_buff), 16*(cell_count_1ceil-1), sizeof(u32), 0);
	rend::barrier::transfer_compute(cmd, [&p](rend::barrier &b) {
		p.barrier_proc_buffers(b, rend::barrier::trans_w, rend::barrier::shader_rw, {counts_buff});
	});
	for (usize i = scan_steps-1; ; i--) {
		u32 off = 1 << i;
		u32 invocs = cell_count_1ceil >> (i + 1);
		p.bind_dispatch(downsweep_pl, sweep_push{invocs, off}, {(invocs+63)/64, 1, 1});
		if (i == 0) break;
		rend::barrier::compute_compute(cmd, [&p](rend::barrier &b) {
			p.barrier_proc_buffers(b, rend::barrier::shader_rw, rend::barrier::shader_rw, {counts_buff});
		});
	}
	rend::barrier::compute_compute(cmd, [&p](rend::barrier &b) {
		p.barrier_proc_buffers(b, rend::barrier::shader_rw, rend::barrier::shader_r, {counts_buff});
		p.barrier_proc_buffers(b, rend::barrier::shader_w, rend::barrier::shader_r, {index_buff}); // from count_pl
	});
	ts.stamp(cmd, vk::PipelineStageFlagBits::eComputeShader, 3);
	p.bind_dispatch(write_pl, cw_push{compile_options::particle_count}, pkernel);
	rend::barrier::compute_compute(cmd, [&p](rend::barrier &b) {
		p.barrier_main_buffer(b, rend::barrier::shader_r, rend::barrier::shader_rw, {0});
		p.barrier_proc_buffers(b, rend::barrier::shader_w, rend::barrier::shader_rw, {cell_buff});
	});
	ts.stamp(cmd, vk::PipelineStageFlagBits::eComputeShader, 4);
	p.bind_dispatch(density_pl, density_push{compile_options::particle_count}, pkernel);
	rend::barrier::compute_compute(cmd, [&p](rend::barrier &b) {
		p.barrier_main_buffer(b, rend::barrier::shader_rw, rend::barrier::shader_r, {0});
		p.barrier_proc_buffers(b, rend::barrier::shader_rw, rend::barrier::shader_r, {cell_buff});
	});
	ts.stamp(cmd, vk::PipelineStageFlagBits::eComputeShader, 5);
	p.bind_dispatch(update_pl, update_push{compile_options::particle_count, global_density * .1f,
		stiffness * .001f, viscosity * .01f}, pkernel);
	ts.stamp(cmd, vk::PipelineStageFlagBits::eBottomOfPipe, 6);
}
u32 particles::pframe() const { return u32(proc.frame); }
vk::Buffer particles::particle_buff() const { return *proc.main_buffer[proc.frame]; }
void particles::spawn_cell(glm::vec2 pos, glm::vec2 vel) {
	if (free_cells.empty()) {
		logs::errorln("not enough space for more cells");
		return;
	}
	u32 gi = free_cells.back();
	u32 si = next_cell_stage;
	free_cells.pop_back();
	next_cell_stage = (next_cell_stage + 1) % (sim_frames * compile_options::cell_particle_count);
	cell_stage_map[si] = {pos, 2, 0.f, vel};
	curr_staged.push_back({si * sizeof(particle), gi * sizeof(particle), sizeof(particle)});
	cells.push_back({cell::gpu_state::staged, gi});
}
void particles::kill_cell(u32 gpu_id) {
	u32 si = next_cell_stage;
	next_cell_stage = (next_cell_stage + 1) % (sim_frames * compile_options::cell_particle_count);
	free_cells.push_back(gpu_id);
	cell_stage_map[si].type = 0;
	curr_staged.push_back({si * sizeof(particle), gpu_id * sizeof(particle), sizeof(particle)});
}

