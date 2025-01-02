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
struct update_push { u32 pcount; f32 global_density; f32 stiffness; };

struct particle_id {
	alignas(16) glm::vec2 pos;
	alignas(4) u32 id;
	alignas(4) f32 density;
};

particles::particles(const rend::context &ctx, const vk::raii::Device &device,
	const rend::buffer_handler &bh, const vk::raii::DescriptorPool &dpool) : proc(device, bh) {
	std::vector<particle> particle_data(compile_options::particle_count);
	for (u32 i = 0; i < compile_options::particle_count; i++) {
		particle_data[i].pos = randv() * glm::fvec2{compile_options::cells_x, compile_options::cells_y};
		particle_data[i].vel = randuv() / 1000.f;
	}
	static constexpr u32 cell_buff_size = cell_count_ceil+(std::popcount(cell_count)==1?1:0);
	proc.set_main_buffer(particle_data, vk::BufferUsageFlagBits::eVertexBuffer);
	proc.add_buffer(cell_buff_size * 16, vk::BufferUsageFlagBits::eTransferDst);
	proc.add_buffer(compile_options::particle_count * 16);
	proc.add_buffer(compile_options::particle_count * sizeof(particle_id));
	for (u32 i = 0; i < sim_frames; i++) {
		ctx.set_object_name(device, *proc.main_buffer[i], ("particle main buffer #"+std::to_string(i)).c_str());
	}
	ctx.set_object_name(*device, *proc.proc_buffers[counts_buff-1], "particle counts buffer");
	ctx.set_object_name(*device, *proc.proc_buffers[index_buff-1], "particle index/density buffer");
	ctx.set_object_name(*device, *proc.proc_buffers[cell_buff-1], "particle cell buffer");

	logs::infoln("particles", compile_options::particle_count, " max particles, ", cell_count, " cells");
	logs::infoln("particles", "counts buffer has ", cell_buff_size, " entries");

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
}

void particles::step(vk::CommandBuffer cmd, u32 frame) {
	static_cast<void>(frame);
	auto p = proc.start(cmd);
	glm::vec3 pkernel{(compile_options::particle_count+63)/64, 1, 1};
	p.fill_proc_buffer(counts_buff);
	rend::barrier::transfer_compute(cmd, [&p](rend::barrier &b) {
		p.barrier_proc_buffers(b, rend::barrier::trans_w, rend::barrier::shader_rw, {counts_buff});
	});
	p.bind_dispatch(count_pl, cw_push{compile_options::particle_count}, pkernel);
	constexpr static usize scan_steps = std::bit_width(cell_count_ceil-1);
	for (usize i = 0; i < scan_steps-1; i++) {
		rend::barrier::compute_compute(cmd, [&p](rend::barrier &b) {
			p.barrier_proc_buffers(b, rend::barrier::shader_rw, rend::barrier::shader_rw, {counts_buff});
		});
		u32 off = 1 << i;
		u32 invocs = cell_count_ceil >> (i + 1);
		p.bind_dispatch(upsweep_pl, sweep_push{invocs, off}, {(invocs+63)/64, 1, 1});
	}
	rend::barrier::compute_transfer(cmd, [&p](rend::barrier &b) {
		p.barrier_proc_buffers(b, rend::barrier::shader_rw, rend::barrier::trans_w, {counts_buff});
	});
	cmd.fillBuffer(*p.pbuff(counts_buff), 16*(cell_count_ceil-1), sizeof(u32), 0);
	rend::barrier::transfer_compute(cmd, [&p](rend::barrier &b) {
		p.barrier_proc_buffers(b, rend::barrier::trans_w, rend::barrier::shader_rw, {counts_buff});
	});
	for (usize i = scan_steps-1; i < scan_steps; i--) { // <- intended underflow
		u32 off = 1 << i;
		u32 invocs = cell_count_ceil >> (i + 1);
		p.bind_dispatch(downsweep_pl, sweep_push{invocs, off}, {(invocs+63)/64, 1, 1});
		rend::barrier::compute_compute(cmd, [&p, i](rend::barrier &b) {
			if (i == 0) {
				p.barrier_proc_buffers(b, rend::barrier::shader_rw, rend::barrier::shader_r, {counts_buff});
				p.barrier_proc_buffers(b, rend::barrier::shader_w, rend::barrier::shader_r, {index_buff});
			} else {
				p.barrier_proc_buffers(b, rend::barrier::shader_rw, rend::barrier::shader_rw, {counts_buff});
			}
		});
	}
	rend::barrier::compute_transfer(cmd, [&p](rend::barrier &b) {
		p.barrier_proc_buffers(b, rend::barrier::shader_rw, rend::barrier::trans_w, {counts_buff});
	});
	cmd.fillBuffer(*p.pbuff(counts_buff), 16*(cell_count), sizeof(u32),
		compile_options::particle_count);
	rend::barrier::transfer_compute(cmd, [&p](rend::barrier &b) {
		p.barrier_proc_buffers(b, rend::barrier::trans_w, rend::barrier::shader_r, {counts_buff});
	});
	p.bind_dispatch(write_pl, cw_push{compile_options::particle_count}, pkernel);
	rend::barrier::compute_compute(cmd, [&p](rend::barrier &b) {
		p.barrier_proc_buffers(b, rend::barrier::shader_w, rend::barrier::shader_r, {cell_buff});
	});
	rend::barrier::compute_compute(cmd, [&p](rend::barrier &b) {
		p.barrier_main_buffer(b, rend::barrier::shader_r, rend::barrier::shader_rw, {0});
		p.barrier_proc_buffers(b, rend::barrier::shader_w, rend::barrier::shader_rw, {cell_buff});
	});
	p.bind_dispatch(density_pl, cw_push{compile_options::particle_count}, pkernel);
	rend::barrier::compute_compute(cmd, [&p](rend::barrier &b) {
		p.barrier_main_buffer(b, rend::barrier::shader_rw, rend::barrier::shader_r, {0});
		p.barrier_proc_buffers(b, rend::barrier::shader_rw, rend::barrier::shader_r, {cell_buff});
	});
	p.bind_dispatch(update_pl, update_push{compile_options::particle_count, .5f, .0005f}, pkernel);
}
u32 particles::pframe() const { return u32(proc.frame); }
vk::Buffer particles::particle_buff() const { return *proc.main_buffer[proc.frame]; }

