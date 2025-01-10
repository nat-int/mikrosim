#include "app.hpp"
#include <cinttypes>
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wold-style-cast"
#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_vulkan.h"
#pragma clang diagnostic pop

struct particle_draw_push {
	glm::mat4 proj;
	f32 psize;
};

mikrosim_window::mikrosim_window() : rend::preset::simple_window("mikrosim", version::combined, 1300, 720, "mikrosim",
	[](rend::context &ctx) { static_cast<void>(ctx); },
	[](const rend::window &win, phy_device_m_t &phy_device) {static_cast<void>(win); static_cast<void>(phy_device); }),
	first_frame(true), quad_vb(nullptr), quad_ib(nullptr), concs_stage(nullptr), concs_isb(nullptr),
	particle_draw_pll(nullptr), particle_draw_pl(nullptr) {
	win.set_user_pointer(this);
	std::vector<vk::DescriptorPoolSize> dpool_sizes = {
		{vk::DescriptorType::eStorageBuffer, compile_options::frames_in_flight * 128},
		{vk::DescriptorType::eCombinedImageSampler, compile_options::frames_in_flight * 64},
	};
	dpool = device.createDescriptorPool({vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet, compile_options::frames_in_flight * 256, dpool_sizes});
	setup_text();

	std::vector<glm::vec2> quad_vd = {{-1.f, -1.f}, {1.f, -1.f}, {1.f, 1.f}, {-1.f, 1.f}};
	std::vector<u16> quad_id = {0,1,2, 0,2,3};
	quad_vb = buffer_h->make_device_buffer_dedicated_stage(quad_vd, vk::BufferUsageFlagBits::eVertexBuffer);
	quad_ib = buffer_h->make_device_buffer_dedicated_stage(quad_id, vk::BufferUsageFlagBits::eIndexBuffer);

	concs_stage = buffer_h->make_staging_buffer(compile_options::particle_count*16,
		vk::BufferUsageFlagBits::eTransferSrc);
	concs_stage_map = static_cast<std140_f32 *>(concs_stage.map());
	std::vector<std140_f32> concs_isd(16*compile_options::particle_count);
	concs_isb = buffer_h->make_device_buffer_dedicated_stage(concs_isd,
		vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst);

	update_cmds = device.allocateCommandBuffers({*command_pool, vk::CommandBufferLevel::ePrimary,
		compile_options::frames_in_flight});
	for (usize i = 0; i < particles::sim_frames; i++) {
		update_semaphores.push_back(device.createSemaphore({}));
		update_render_semaphores.push_back(device.createSemaphore({}));
	}

	vk::PushConstantRange push_range{vk::ShaderStageFlagBits::eVertex, 0, sizeof(particle_draw_push)};
	particle_draw_pll = device.createPipelineLayout({{}, {}, push_range});
	shaders = rend::load_shaders_from_file(device, {"./shaders/particle.vert.spv", "./shaders/particle.frag.spv"});
	ctx.set_object_name(*device, *shaders[0], "particle vertex shader");
	ctx.set_object_name(*device, *shaders[1], "particle fragment shader");
	{
		rend::graphics_pipeline_create_maker pcm;
		pcm.set_shaders_gl({vk::ShaderStageFlagBits::eVertex, vk::ShaderStageFlagBits::eFragment}, { *shaders[0], *shaders[1] });
		pcm.add_vertex_input_attribute(0, rend::vglm<glm::vec2>::format).
			add_vertex_input_binding(sizeof(glm::vec2), vk::VertexInputRate::eVertex);
		pcm.add_vertex_input_attribute(1, rend::vglm<glm::vec2>::format).
			add_vertex_input_attribute(2, rend::vglm<u32>::format, offsetof(particle, type)).
			add_vertex_input_binding(sizeof(particle), vk::VertexInputRate::eInstance);
		pcm.add_vertex_input_attribute(3, rend::vglm<f32>::format).
			add_vertex_input_binding(sizeof(std140_f32), vk::VertexInputRate::eInstance);
		pcm.set_attachment_basic_blending();
		pcm.rasterization_state.cullMode = vk::CullModeFlagBits::eNone;
		particle_draw_pl = device.createGraphicsPipeline(nullptr, pcm.get(particle_draw_pll, swapchain_render_pass));
	}
	ctx.set_object_name(*device, *particle_draw_pl, "particle render pipeline");

	if (rend::supports_timestamps(phy_device.physical_device, phy_device.queue_family_ids[0])) {
		compute_ts.init(device);
		ctx.set_object_name(*device, *compute_ts.qpool, "compute timestamp query pool");
		render_ts.init(device);
		ctx.set_object_name(*device, *render_ts.qpool, "render timestamp query pool");
	}
	logs::debugln("mikrosim", "physical device timestamp period is ",
		phy_device.physical_device.properties.limits.timestampPeriod);

	p.emplace(ctx, device, *buffer_h, dpool);

	running = false;
	view_scale = 400.f / compile_options::cells_x;
	view_position = {f32(compile_options::cells_x) / 2.f, f32(compile_options::cells_y) / 2.f};
	update_view();
	win.bind_scroll_callback<mikrosim_window>();
	particle_draw_size = .6f;

	prev_frame = 0.f;
	dt = 0.f;
	sec_timer = 0.f;
	frame_counter = 0;
	fps = 0;

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
	ImGui_ImplGlfw_InitForVulkan(win.raw(), true);
	ImGui_ImplVulkan_InitInfo imgui_vk{};
	imgui_vk.Instance = ctx.vulkan_instance();
	imgui_vk.PhysicalDevice = *phy_device.physical_device.handle;
	imgui_vk.Device = *device;
	imgui_vk.QueueFamily = phy_device.queue_family_ids[0];
	imgui_vk.Queue = *graphics_compute_queue;
	imgui_vk.PipelineCache = nullptr;
	imgui_vk.DescriptorPool = *dpool;
	imgui_vk.RenderPass = *swapchain_render_pass;
	imgui_vk.Subpass = 0;
	imgui_vk.ImageCount = u32(win.vulkan_swapchain_image_count());
	imgui_vk.MinImageCount = u32(win.vulkan_swapchain_image_count());
	imgui_vk.MSAASamples = static_cast<VkSampleCountFlagBits>(vk::SampleCountFlagBits::e1);
	imgui_vk.Allocator = nullptr;
	imgui_vk.CheckVkResultFn = [](VkResult res) {
		if (res == 0) return;
		logs::errorln("imgui", "vulkan error - result ", i32(res));
		if (res < 0) std::exit(1);
	};
	ImGui_ImplVulkan_Init(&imgui_vk);
}
void mikrosim_window::terminate() {
	device.waitIdle();
	ImGui_ImplVulkan_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();
}
void mikrosim_window::loop() {
	std::vector<rend::vertex2d> debug_bg_vd;
	for (usize x = 0; x < compile_options::cells_x; x++) {
		for (usize y = 0; y < compile_options::cells_y; y++) {
			if ((x + y) % 2 == 0) {
				auto nq = rend::quad6v2d({f32(x), f32(y)}, {f32(x+1), f32(y+1)}, {.6f, .6f, .6f, 1.f});
				debug_bg_vd.insert(debug_bg_vd.end(), nq.begin(), nq.end());
			}
		}
	}
	rend::simple_mesh debug_bg_mesh{buffer_h->make_device_buffer_dedicated_stage(debug_bg_vd, vk::BufferUsageFlagBits::eVertexBuffer), static_cast<u32>(debug_bg_vd.size())};

	prev_frame = f32(glfwGetTime());
	mainloop<true>(
		[this]() {
			f32 this_frame = f32(glfwGetTime());
			dt = this_frame - prev_frame;
			prev_frame = this_frame;
			sec_timer += dt;
			while (sec_timer > 1) {
				sec_timer -= 1;
				fps = frame_counter;
				frame_counter = 0;
			}
			frame_counter++;
			inp.update(win);
			update();
		},
		[this, &debug_bg_mesh](u32 rframe, vk::CommandBuffer cmd) {
			compute_ts.update();
			render_ts.update();
			vk::Buffer particle_buff = p->particle_buff();
			if (running || inp.is_key_down(GLFW_KEY_SEMICOLON)) {
				advance_sim(rframe);
			}

			cmd.reset();
			static_cast<void>(cmd.begin(vk::CommandBufferBeginInfo{}));
			if (rframe == 0) {
				for (usize i = 0; i < compile_options::particle_count; i++) {
					concs_stage_map[i].v = p->comps->at(0, i);
				}
				cmd.copyBuffer(*concs_stage, *concs_isb, {{0, 0, sizeof(std140_f32) *
					compile_options::particle_count}});
				{
					auto b = rend::barrier(cmd, vk::PipelineStageFlagBits::eTransfer,
						vk::PipelineStageFlagBits::eVertexInput);
					b.buff(rend::barrier::trans_w, vk::AccessFlagBits::eVertexAttributeRead,
						*concs_isb, sizeof(std140_f32) * compile_options::particle_count);
				}
			}
			render(cmd, debug_bg_mesh, particle_buff);
			cmd.end();
		},
		[this]() {
			update_view();
		}
	);
	terminate();
}
void mikrosim_window::update() {
	bool pos_upd = false;
	if (inp.is_mouse_held(GLFW_MOUSE_BUTTON_MIDDLE) &&
		!inp.is_mouse_down(GLFW_MOUSE_BUTTON_MIDDLE)) {
		view_position -= inp.get_cursor_delta() / view_scale;
		pos_upd = true;
	}
	if (inp.is_key_held(GLFW_KEY_A)) { view_position.x -= 100.f*dt/view_scale; pos_upd = true; }
	if (inp.is_key_held(GLFW_KEY_D)) { view_position.x += 100.f*dt/view_scale; pos_upd = true; }
	if (inp.is_key_held(GLFW_KEY_W)) { view_position.y -= 100.f*dt/view_scale; pos_upd = true; }
	if (inp.is_key_held(GLFW_KEY_S)) { view_position.y += 100.f*dt/view_scale; pos_upd = true; }
	if (pos_upd) { update_view(); }
	if (inp.is_key_down(GLFW_KEY_SPACE)) {
		running = !running;
	}
	if (inp.is_key_down(GLFW_KEY_N)) {
		// adding that thing to position fixes cells spawning at -nan when in starting
		// view for some reason, probably glm being weird with types??
		p->spawn_cell(view_position + glm::vec2{.0001f, .0001f}, {0.f, 0.f});
	}
	if (inp.is_key_down(GLFW_KEY_F12)) {
		running = false;
		p->debug_dump();
	}
}
void mikrosim_window::advance_sim(u32 rframe) {
	u32 pframe = p->pframe();
	vk::CommandBuffer ucmd = update_cmds[rframe];
	ucmd.reset();
	static_cast<void>(ucmd.begin(vk::CommandBufferBeginInfo{}));
	p->step_gpu(ucmd, rframe, compute_ts);
	compute_ts.end();
	ucmd.end();
	u32 npframe = p->pframe();
	auto signal_semaphores = {*update_semaphores[npframe], *update_render_semaphores[npframe]};
	if (first_frame) { [[unlikely]];
		first_frame = false;
		graphics_compute_queue.submit({{{}, {}, ucmd, signal_semaphores}});
	} else {
		vk::PipelineStageFlags psf = vk::PipelineStageFlagBits::eComputeShader;
		graphics_compute_queue.submit({{*update_semaphores[pframe], psf, ucmd, signal_semaphores}});
		render_submit_semaphores.push_back(*update_render_semaphores[pframe]);
		render_submit_semaphore_stages.push_back(vk::PipelineStageFlagBits::eComputeShader);
	}
	p->step_cpu();
}
void mikrosim_window::render(vk::CommandBuffer cmd, const rend::simple_mesh &bg_mesh,
	vk::Buffer particle_buff) {
	constexpr f32 imgui_width = 300.f;
	vk::Extent2D inner_ext = {win.vulkan_swapchain_extent().width - u32(imgui_width),
		win.vulkan_swapchain_extent().height};

	render_ts.reset(cmd);
	render_ts.stamp(cmd, vk::PipelineStageFlagBits::eTopOfPipe, 0);
	begin_swapchain_render_pass(cmd, {.9f, .9f, .9f, 1.f});
	win.set_viewport(cmd);
	cmd.setScissor(0, vk::Rect2D{{u32(imgui_width), 0}, inner_ext});
	rend2d->bind_solid_pipeline(cmd);
	rend2d->push_projection(cmd, vp);
	bg_mesh.bind_draw(cmd);

	cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, particle_draw_pl);
	particle_draw_push push{vp, particle_draw_size};
	cmd.pushConstants(*particle_draw_pll, vk::ShaderStageFlagBits::eVertex, 0, sizeof(particle_draw_push), &push);
	cmd.bindIndexBuffer(*quad_ib, 0, vk::IndexType::eUint16);
	cmd.bindVertexBuffers(0, {*quad_vb, particle_buff, *concs_isb}, {0, 0, 0});
	cmd.drawIndexed(6, compile_options::particle_count, 0, 0, 0);

	win.set_viewport_and_scissor(cmd);
	ImGui_ImplVulkan_NewFrame();
	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();
	ImGui::SetNextWindowPos({0.0f, 0.0f});
	ImGui::SetNextWindowSize({imgui_width, f32(win.height())});
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
	if (ImGui::Begin("mikrosim", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoDecoration)) {
		ImGui::Text("%u FPS", fps); ImGui::SameLine();
		if (running) {
			ImGui::TextColored({.5f, 1.f, .5f, 1.f}, "running ([space] - stop)");
		} else {
			ImGui::TextColored({1.f, .5f, .06f, 1.f}, "stopped ([space] - start)");
		}
		ImGui::SliderFloat("fluid density", &p->global_density, .1f, 20.f, "%.3f");
		ImGui::SliderFloat("fluid stiffness", &p->stiffness, 0.f, 16.f, "%.3f");
		ImGui::SliderFloat("fluid viscosity", &p->viscosity, 0.f, 8.f, "%.3f");
		ImGui::SliderFloat("particle draw size", &particle_draw_size, 0.f, 2.f, "%.3f");
		const f64 tsp = f64(phy_device.physical_device.properties.limits.timestampPeriod);
		std::vector<std::string> timestamp_names = {"count","prefix","write","density","update","transfer"};
		if (ImGui::BeginTable("##gpu_timestamps", 3)) {
			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::TableHeader("event");
			ImGui::TableNextColumn();
			ImGui::TableHeader("timestamp");
			ImGui::TableNextColumn();
			ImGui::TableHeader("delta");
			for (u32 i = 1; i < particles::timestamps; i++) {
				ImGui::TableNextRow();
				ImGui::TableNextColumn();
				ImGui::Text("%s", i-1 < timestamp_names.size() ? timestamp_names[i-1].c_str() : "unknown");
				ImGui::TableNextColumn();
				ImGui::Text("%s", format_time(compute_ts.delta(i, 0, tsp)).c_str());
				ImGui::TableNextColumn();
				ImGui::Text("%s", format_time(compute_ts.delta(i, i-1, tsp)).c_str());
			}
		}
		ImGui::EndTable();
		if (ImGui::BeginTable("##gpu_timestamps2", 3)) {
			static constexpr u64 sns = 1'000'000'000ull;
			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::TableNextColumn();
			ImGui::TableHeader("time");
			ImGui::TableNextColumn();
			ImGui::TableHeader("rate (Hz)");
			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::TableHeader("simulation");
			ImGui::TableNextColumn();
			u64 cd = compute_ts.delta(particles::timestamps-1, 0, tsp);
			ImGui::Text("%s", format_time(cd).c_str());
			ImGui::TableNextColumn();
			ImGui::Text("%" PRIu64, cd > 0 ? sns / cd : 0);
			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::TableHeader("render (gpu)");
			ImGui::TableNextColumn();
			u64 rd = render_ts.delta(render_timestamps-1, 0, tsp);
			ImGui::Text("%s", format_time(rd).c_str());
			ImGui::TableNextColumn();
			ImGui::Text("%" PRIu64, rd > 0 ? sns / rd : 0);
		}
		ImGui::EndTable();
	}
	ImGui::End();
	ImGui::PopStyleVar();
	p->imgui();
	ImGui::Render();
	ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);

	cmd.endRenderPass();
	render_ts.stamp(cmd, vk::PipelineStageFlagBits::eBottomOfPipe, 1);
	render_ts.end();
}
void mikrosim_window::on_scroll(f64 dx, f64 dy) {
	static_cast<void>(dx);
	if (ImGui::GetIO().WantCaptureMouse)
		return;
	view_scale *= glm::pow(inp.is_key_held(GLFW_KEY_LEFT_SHIFT) ? 1.1f : 1.5f, f32(dy));
	update_view();
}
void mikrosim_window::update_view() {
	vp = rend2d->proj(rend::anchor2d::center) *
		glm::translate(glm::scale(glm::mat4(1.f), {view_scale, view_scale, 1.f}), {-view_position, 0.f});
}

