#include "particles.hpp"
#include <imgui.h>
#include "rendering/pipeline.hpp"
#include "util.hpp"

static glm::vec2 randv() { return {randf(), randf()}; }
static glm::vec2 randuv() { f32 a = randf() * glm::pi<f32>() * 2; return {glm::cos(a), glm::sin(a)}; }

struct cwd_push { u32 pcount; };
struct sweep_push { u32 invocs; u32 off; };
struct update_push {
	glm::uvec4 mix_comps;
	u32 pcount; f32 global_density; f32 stiffness; f32 viscosity;
	
};
struct report_push { u32 off; u32 pcount; };

struct particle_id {
	alignas(16) glm::vec2 pos;
	alignas(4) u32 type;
	alignas(4) f32 density;
	alignas(16) glm::vec2 vel;
	alignas(4) u32 id;
	alignas(16) glm::vec4 concentration;
};

particles::particles(const rend::context &ctx, const vk::raii::Device &device,
	const rend::buffer_handler &bh, const vk::raii::DescriptorPool &dpool) : proc(device, bh),
	cell_stage(nullptr), reports(nullptr), concs_upload(nullptr), concs_download(nullptr),
	blocks_uniform(bh.get_alloc()) {
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
	proc.add_buffer(compile_options::cell_particle_count * sizeof(particle_report),
		vk::BufferUsageFlagBits::eTransferSrc);
	proc.add_buffer(compile_options::particle_count * sizeof(glm::vec4),
		vk::BufferUsageFlagBits::eTransferSrc | vk::BufferUsageFlagBits::eTransferDst);
	for (u32 i = 0; i < sim_frames; i++) {
		ctx.set_object_name(device, *proc.main_buffer[i], ("particle main buffer #"+std::to_string(i)).c_str());
	}
	ctx.set_object_name(*device, *proc.proc_buffers[counts_buff-1], "particle counts buffer");
	ctx.set_object_name(*device, *proc.proc_buffers[index_buff-1], "particle index/density buffer");
	ctx.set_object_name(*device, *proc.proc_buffers[cell_buff-1], "particle cell buffer");
	ctx.set_object_name(*device, *proc.proc_buffers[report_buff-1], "particle report buffer");
	ctx.set_object_name(*device, *proc.proc_buffers[concs_buff-1], "particle gpu concentrations buffer");

	cell_stage = bh.make_staging_buffer(sizeof(particle)*compile_options::cell_particle_count*sim_frames,
		vk::BufferUsageFlagBits::eTransferSrc);
	cell_stage_map = static_cast<particle *>(cell_stage.map());
	reports = bh.make_staging_buffer(sizeof(particle_report)*compile_options::cell_particle_count,
		vk::BufferUsageFlagBits::eTransferDst);
	reports_map = static_cast<particle_report *>(reports.map());
	concs_upload = bh.make_staging_buffer(sizeof(glm::vec4)*compile_options::particle_count*
		sim_frames, vk::BufferUsageFlagBits::eTransferSrc);
	concs_upload_map = static_cast<glm::vec4 *>(concs_upload.map());
	concs_download = bh.make_staging_buffer(sizeof(glm::vec4)*compile_options::particle_count*
		sim_frames, vk::BufferUsageFlagBits::eTransferDst);
	concs_download_map = static_cast<glm::vec4 *>(concs_download.map());
	ctx.set_object_name(*device, *cell_stage, "cell staging buffer");
	ctx.set_object_name(*device, *reports, "cell reports (staging) buffer");
	ctx.set_object_name(*device, *concs_upload, "concentrations staging buffer");
	ctx.set_object_name(*device, *concs_download, "concentrations unstaging buffer");

	logs::infoln("particles", compile_options::particle_count, " max particles, ", cell_count, " cells");
	logs::infoln("particles", "counts buffer has ", cell_count_1ceil, " entries");

	proc.add_main_buffer_pipeline<cwd_push>(dpool, "./shaders/count.comp.spv", {counts_buff, index_buff}, 1);
	proc.add_proc_buffer_pipeline<sweep_push>(dpool, "./shaders/upsweep.comp.spv", {counts_buff});
	proc.add_proc_buffer_pipeline<sweep_push>(dpool, "./shaders/downsweep.comp.spv", {counts_buff});
	proc.add_main_buffer_pipeline<cwd_push>(dpool, "./shaders/write.comp.spv", {counts_buff, index_buff, cell_buff, concs_buff}, 1);
	proc.add_main_buffer_pipeline<cwd_push>(dpool, "./shaders/density.comp.spv", {counts_buff, index_buff, cell_buff, concs_buff}, 1);
	//proc.add_main_buffer_pipeline<update_push>(dpool, "./shaders/update.comp.spv", {counts_buff, cell_buff, concs_buff}, 2);
	proc.pipelines.emplace_back();
	proc.pipelines.back().create<update_push>(device, "./shaders/update.comp.spv",
		{{vk::DescriptorType::eUniformBuffer, 1},
			{vk::DescriptorType::eStorageBuffer, 1 /*main*/},
			{vk::DescriptorType::eStorageBuffer, 1 /*next*/},
			{vk::DescriptorType::eStorageBuffer, 1 /*counts*/},
			{vk::DescriptorType::eStorageBuffer, 1 /*cell*/},
			{vk::DescriptorType::eStorageBuffer, 1 /*concs*/}});
	{
		std::vector<vk::DescriptorSetLayout> dsls(sim_frames, proc.pipelines.back().dsl);
		proc.pipelines.back().ds = device.allocateDescriptorSets({dpool, dsls});
		std::vector<vk::WriteDescriptorSet> writes;
		writes.reserve(sim_frames*6);
		std::vector<vk::DescriptorBufferInfo> dbis;
		dbis.reserve(sim_frames*6);
		for (u32 f = 0; f < sim_frames; f++) {
			dbis.push_back(blocks_uniform.descriptor_info());
			writes.push_back({proc.pipelines.back().ds[f], 0, 0,
				vk::DescriptorType::eUniformBuffer, {}, dbis.back(), {}});
			dbis.push_back({*proc.main_buffer[f], 0, proc.buffer_sizes[0]});
			writes.push_back({proc.pipelines.back().ds[f], 1, 0,
				vk::DescriptorType::eStorageBuffer, {}, dbis.back(), {}});
			dbis.push_back({*proc.main_buffer[(f+1)%sim_frames], 0, proc.buffer_sizes[0]});
			writes.push_back({proc.pipelines.back().ds[f], 2, 0,
				vk::DescriptorType::eStorageBuffer, {}, dbis.back(), {}});
			dbis.push_back({*proc.proc_buffers[counts_buff-1], 0, proc.buffer_sizes[counts_buff]});
			writes.push_back({proc.pipelines.back().ds[f], 3, 0,
				vk::DescriptorType::eStorageBuffer, {}, dbis.back(), {}});
			dbis.push_back({*proc.proc_buffers[cell_buff-1], 0, proc.buffer_sizes[cell_buff]});
			writes.push_back({proc.pipelines.back().ds[f], 4, 0,
				vk::DescriptorType::eStorageBuffer, {}, dbis.back(), {}});
			dbis.push_back({*proc.proc_buffers[concs_buff-1], 0, proc.buffer_sizes[concs_buff]});
			writes.push_back({proc.pipelines.back().ds[f], 5, 0,
				vk::DescriptorType::eStorageBuffer, {}, dbis.back(), {}});
		}
		device.updateDescriptorSets(writes, {});
	}
	proc.add_main_buffer_pipeline<report_push>(dpool, "./shaders/report.comp.spv", {report_buff}, 1);
	ctx.set_object_name(*device, *proc.pipelines[count_pl].pipeline, "particle count pipeline");
	ctx.set_object_name(*device, *proc.pipelines[upsweep_pl].pipeline, "particle upsweep pipeline");
	ctx.set_object_name(*device, *proc.pipelines[downsweep_pl].pipeline, "particle downsweep pipeline");
	ctx.set_object_name(*device, *proc.pipelines[write_pl].pipeline, "particle write pipeline");
	ctx.set_object_name(*device, *proc.pipelines[density_pl].pipeline, "particle density pipeline");
	ctx.set_object_name(*device, *proc.pipelines[update_pl].pipeline, "particle update pipeline");
	ctx.set_object_name(*device, *proc.pipelines[report_pl].pipeline, "particle report pipeline");

	next_cell_stage = 0;
	for (u32 i = compile_options::cell_particle_count-1; ; i--) {
		free_cells.push_back(i);
		if (i == 0)
			break;
	}
	next_mix_concs = 0;

	global_density = 2.f;
	stiffness = 8.f;
	viscosity = 1.f;
	const glm::vec2 csz = glm::vec2{f32(compile_options::cells_x), f32(compile_options::cells_y)};
	constexpr f32 i6 = 1.f / 6.f;
	force_blocks[0] = {csz * glm::vec2{.5f, i6}, csz * glm::vec2{i6, i6}, {0.f, 0.f}, {.1f, 0.f}};
	force_blocks[1] = {csz * glm::vec2{i6, .5f}, csz * glm::vec2{i6, i6}, {0.f, 0.f}, {.1f, 0.f}};
	force_blocks[2] = {csz * glm::vec2{.5f, 1.f-i6}, csz * glm::vec2{i6, i6}, {0.f, 0.f}, {.1f, 0.f}};
	force_blocks[3] = {csz * glm::vec2{1.f-i6, .5f}, csz * glm::vec2{i6, i6}, {0.f, 0.f}, {.1f, 0.f}};
	chem_blocks[0] = {csz * glm::vec2{.5f, i6}, 1.f, 0.f, csz * glm::vec2{i6, i6}, 0.f, 0};
	chem_blocks[1] = {csz * glm::vec2{i6, .5f}, 1.f, 0.f, csz * glm::vec2{i6, i6}, 0.f, 0};
	chem_blocks[2] = {csz * glm::vec2{.5f, 1.f-i6}, 1.f, 0.f, csz * glm::vec2{i6, i6}, 0.f, 0};
	chem_blocks[3] = {csz * glm::vec2{1.f-i6, .5f}, 1.f, 0.f, csz * glm::vec2{i6, i6}, 0.f, 0};
	block_opacity = .1f;

	comps = std::make_unique<compounds>();

	cpu_ts.times = {0};
}
particles::~particles() { cell_stage.unmap(); }

void particles::step_cpu() {
	cpu_ts.start();
	bool protein_creation = true;
	for (usize j = 0; j < block_count; j++) {
		if (comps->locked[comps->atoms_to_id[block_compounds[j]]]) {
			protein_creation = false;
			break;
		}
	}
	for (u32 i = 0; i < compile_options::cell_particle_count; i++) {
		if (cells[i].s != cell::state::none) {
			cells[i].pos = reports_map[i].pos;
			cells[i].vel = reports_map[i].vel;
			cells[i].update(*comps, protein_creation);
			if (randf() * cells[i].age > 100.f) {
				kill_cell(cells[i].gpu_id);
			} else if (cells[i].division_pos >= cells[i].genome.size()) {
				cells[i].division_pos = 0;
				usize ci = spawn_cell(cells[i].pos, cells[i].vel);
				if (ci == compile_options::cell_particle_count) {
					cells[i].division_genome.clear();
					continue;
				}
				cells[ci].genome.swap(cells[i].division_genome);
				cells[ci].analyze(*comps);
			}
		}
	}
	cpu_ts.stamp();
}
void particles::step_gpu(vk::CommandBuffer cmd, u32 frame, const rend::timestamps<timestamps> &ts) {
	static_cast<void>(frame);
	update_push upush{{0,0,0,0}, compile_options::particle_count, global_density * .1f,
		stiffness * .001f, viscosity * .01f};
	for (usize i = 0; i < 4; i++) {
		blocks_uniform.get_map()->fbs[i].pos = force_blocks[i].pos;
		blocks_uniform.get_map()->fbs[i].extent = force_blocks[i].extent;
		blocks_uniform.get_map()->fbs[i].cartesian_force = force_blocks[i].cartesian_force * .000003f;
		blocks_uniform.get_map()->fbs[i].polar_force = force_blocks[i].polar_force * .000003f;
		blocks_uniform.get_map()->chbs[i] = chem_blocks[i];
	}
	ts.reset(cmd);
	ts.stamp(cmd, vk::PipelineStageFlagBits::eTopOfPipe, 0);
	auto p = proc.start(cmd);
	for (u32 i = 0; i < 4; i++) {
		u32 compi = (next_mix_concs + compounds::count - 4*compile_options::frames_in_flight + i) %
			compounds::count;
		comps->locked[compi] = false;
		for (u32 j = 0; j < compile_options::particle_count; j++) {
			comps->at(compi, j) = concs_download_map[p.frame(sim_frames-compile_options::frames_in_flight)*
				compile_options::particle_count + j][i32(i)];
		}
	}
	for (u32 i = 0; i < 4; i++) {
		u32 compi = (next_mix_concs + i) % compounds::count;
		upush.mix_comps[i32(i)] = compi;
		comps->locked[compi] = true;
		for (u32 j = 0; j < compile_options::particle_count; j++) {
			concs_upload_map[p.frame() * compile_options::particle_count + j][i32(i)] = comps->at(compi, j);
		}
	}
	next_mix_concs = (next_mix_concs + 4) % compounds::count;
	glm::vec3 pkernel{(compile_options::particle_count+63)/64, 1, 1};
	if (!curr_staged.empty()) {
		cmd.copyBuffer(*cell_stage, *p.mbuff(), curr_staged);
	}
	cmd.copyBuffer(*concs_upload, *p.pbuff(concs_buff),
		{{sizeof(glm::vec4) * compile_options::particle_count * p.frame(), 0,
		sizeof(glm::vec4) * compile_options::particle_count}});
	p.fill_proc_buffer(counts_buff);
	rend::barrier::transfer_compute(cmd, [this, &p](rend::barrier &b) {
		p.barrier_proc_buffers(b, rend::barrier::trans_w, rend::barrier::shader_rw, {counts_buff});
		if (!curr_staged.empty()) {
			p.barrier_main_buffer(b, rend::barrier::trans_w, rend::barrier::shader_r);
		}
		p.barrier_proc_buffers(b, rend::barrier::trans_w, rend::barrier::shader_r, {concs_buff});
	});
	curr_staged.clear();
	p.pl(report_pl).bind(cmd);
	p.pl(report_pl).bind_ds(cmd, {*p.pl(report_pl).ds[p.frame(0)]});
	p.push_dispatch(report_pl, report_push{compile_options::env_particle_count,
		compile_options::particle_count}, {(compile_options::cell_particle_count+63)/64, 1, 1});
	rend::barrier::compute_transfer(cmd, [&p](rend::barrier &b) {
		p.barrier_proc_buffers(b, rend::barrier::shader_w, rend::barrier::trans_r, {report_buff});
	});
	cmd.copyBuffer(*p.pbuff(report_buff), *reports, {{0, 0, proc.buffer_sizes[report_buff]}});

	p.bind_dispatch(count_pl, cwd_push{compile_options::particle_count}, pkernel);
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
	ts.stamp(cmd, vk::PipelineStageFlagBits::eComputeShader, 2);
	p.bind_dispatch(write_pl, cwd_push{compile_options::particle_count}, pkernel);
	rend::barrier::compute_compute(cmd, [&p](rend::barrier &b) {
		p.barrier_main_buffer(b, rend::barrier::shader_r, rend::barrier::shader_rw, {0});
		p.barrier_proc_buffers(b, rend::barrier::shader_w, rend::barrier::shader_rw, {cell_buff});
	});
	ts.stamp(cmd, vk::PipelineStageFlagBits::eComputeShader, 3);
	p.bind_dispatch(density_pl, cwd_push{compile_options::particle_count}, pkernel);
	rend::barrier::compute_compute(cmd, [&p](rend::barrier &b) {
		p.barrier_main_buffer(b, rend::barrier::shader_rw, rend::barrier::shader_r, {0});
		p.barrier_proc_buffers(b, rend::barrier::shader_rw, rend::barrier::shader_r, {cell_buff});
	});
	ts.stamp(cmd, vk::PipelineStageFlagBits::eComputeShader, 4);
	p.bind_dispatch(update_pl, upush, pkernel);
	rend::barrier::compute_transfer(cmd, [&p](rend::barrier &b) {
		p.barrier_proc_buffers(b, rend::barrier::shader_rw, rend::barrier::trans_r, {concs_buff});
	});
	ts.stamp(cmd, vk::PipelineStageFlagBits::eTransfer, 5);
	cmd.copyBuffer(*p.pbuff(concs_buff), *concs_download,
			{{0, sizeof(glm::vec4) * compile_options::particle_count * p.frame(),
		sizeof(glm::vec4) * compile_options::particle_count}});
	ts.stamp(cmd, vk::PipelineStageFlagBits::eBottomOfPipe, 6);
}
void particles::imgui() {
	if (ImGui::Begin("cell list")) {
		if (ImGui::BeginTable("##cell_list_table", 5)) {
			ImGui::TableNextRow();
			ImGui::TableNextColumn(); ImGui::TableHeader("gpu id");
			ImGui::TableNextColumn(); ImGui::TableHeader("pos x");
			ImGui::TableNextColumn(); ImGui::TableHeader("pos y");
			ImGui::TableNextColumn(); ImGui::TableHeader("vel x");
			ImGui::TableNextColumn(); ImGui::TableHeader("vel y");
			for (const auto &c : cells) {
				if (c.s != cell::state::none) {
					ImGui::TableNextRow();
					ImGui::TableNextColumn(); ImGui::Text("%u", c.gpu_id);
					ImGui::TableNextColumn(); ImGui::Text("%.2f", f64(c.pos.x));
					ImGui::TableNextColumn(); ImGui::Text("%.2f", f64(c.pos.y));
					ImGui::TableNextColumn(); ImGui::Text("%.2f", f64(c.vel.x));
					ImGui::TableNextColumn(); ImGui::Text("%.2f", f64(c.vel.y));
				}
			}
			ImGui::EndTable();
		}
	}
	ImGui::End();
}
u32 particles::pframe() const { return u32(proc.frame); }
vk::Buffer particles::particle_buff() const { return *proc.main_buffer[proc.frame]; }

void particles::debug_dump() const {
	logs::debugln("dump", "------------ BEGIN PARTICLE DEBUG DUMP ------------------");
	logs::debugln("dump", "particle_buff[0]:");
	particle *particle_data = static_cast<particle *>(proc.main_buffer[0].map());
	for (usize i = 0; i < compile_options::particle_count; i++) {
		logs::debugln("dump", "  [", i, "] type ", particle_data[i].type, ", at [", particle_data[i].pos.x,
			", ", particle_data[i].pos.y, "] with vel [", particle_data[i].pos.x, ", ",
			particle_data[i].pos.y, "], density is ", particle_data[i].density);
	}
	proc.main_buffer[0].unmap();
	logs::debugln("dump", "------------- END PARTICLE DEBUG DUMP -------------------");
}

usize particles::spawn_cell(glm::vec2 pos, glm::vec2 vel) {
	if (free_cells.empty()) {
		logs::errorln("not enough space for more cells");
		return compile_options::cell_particle_count;
	}
	u32 ci = free_cells.back();
	u32 gi = ci + compile_options::env_particle_count;
	u32 si = next_cell_stage;
	free_cells.pop_back();
	next_cell_stage = (next_cell_stage + 1) % (sim_frames * compile_options::cell_particle_count);
	cell_stage_map[si] = {pos, 2, 0.f, vel};
	curr_staged.push_back({si * sizeof(particle), gi * sizeof(particle), sizeof(particle)});
	reports_map[ci].pos = pos;
	reports_map[ci].vel = vel;
	cells[ci] = cell(gi, pos, vel);
	for (usize i = 0; i < compounds::count; i++) {
		comps->at(i, gi) = 0.f;
	}
	return ci;
}
void particles::kill_cell(u32 gpu_id) {
	u32 si = next_cell_stage;
	u32 ci = gpu_id - compile_options::env_particle_count;
	next_cell_stage = (next_cell_stage + 1) % (sim_frames * compile_options::cell_particle_count);
	free_cells.push_back(ci);
	cell_stage_map[si].type = 0;
	curr_staged.push_back({si * sizeof(particle), gpu_id * sizeof(particle), sizeof(particle)});
	cells[ci].s = cell::state::none;
}

