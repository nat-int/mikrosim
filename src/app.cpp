#include "app.hpp"
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
	first_frame(true), quad_vb(nullptr), quad_ib(nullptr), particle_draw_pll(nullptr),
	particle_draw_pl(nullptr), timestamp_qpool(nullptr) {
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

	update_cmds = device.allocateCommandBuffers({*command_pool, vk::CommandBufferLevel::ePrimary,
		compile_options::frames_in_flight});
	for (usize i = 0; i < particles::sim_frames; i++) {
		update_semaphores.push_back(device.createSemaphore({}));
		update_render_semaphores.push_back(device.createSemaphore({}));
	}

	vk::PushConstantRange push_range{vk::ShaderStageFlagBits::eVertex, 0, sizeof(particle_draw_push)};
	particle_draw_pll = device.createPipelineLayout({{}, {}, push_range});
	shaders = rend::load_shaders_from_file(device, {"./shaders/particle.vert.spv", "./shaders/particle.frag.spv"});
	{
		rend::graphics_pipeline_create_maker pcm;
		pcm.set_shaders_gl({vk::ShaderStageFlagBits::eVertex, vk::ShaderStageFlagBits::eFragment}, { *shaders[0], *shaders[1] });
		pcm.add_vertex_input_attribute(0, rend::vglm<glm::vec2>::format).
			add_vertex_input_binding(sizeof(glm::vec2), vk::VertexInputRate::eVertex);
		pcm.add_vertex_input_attribute(1, rend::vglm<glm::vec2>::format).
			add_vertex_input_attribute(2, rend::vglm<u32>::format, offsetof(particle, debug_tags)).
			add_vertex_input_binding(sizeof(particle), vk::VertexInputRate::eInstance);
		pcm.set_attachment_basic_blending();
		pcm.rasterization_state.cullMode = vk::CullModeFlagBits::eNone;
		particle_draw_pl = device.createGraphicsPipeline(nullptr, pcm.get(particle_draw_pll, swapchain_render_pass));
	}

	timestamps.resize(particles::timestamps);
	if (phy_device.physical_device.properties.limits.timestampPeriod > 0 &&
		(phy_device.physical_device.properties.limits.timestampComputeAndGraphics ||
		phy_device.physical_device.handle.getQueueFamilyProperties()[phy_device.queue_family_ids[0]].timestampValidBits != 0)) {
		timestamp_qpool = device.createQueryPool({{}, vk::QueryType::eTimestamp, particles::timestamps});
	}
	logs::debugln("mikrosim", "physical device timestamp period is ",
		phy_device.physical_device.properties.limits.timestampPeriod);

	p.emplace(ctx, device, *buffer_h, dpool);

	running = false;
	view_scale = 400.f / compile_options::cells_x;
	view_position = {f32(compile_options::cells_x) / -2.f, f32(compile_options::cells_y) / -2.f};
	update_view();
	win.bind_scroll_callback<mikrosim_window>();
	particle_draw_size = .2f;

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
			bool resubmit_timestamps = first_frame;
			vk::Buffer particle_buff = p->particle_buff();
			if (*timestamp_qpool && !first_frame) {
				u64 tms[particles::timestamps * 2];
				auto res = (*device).getQueryPoolResults(*timestamp_qpool, 0, particles::timestamps,
					sizeof(u64)*particles::timestamps*2, tms, sizeof(u64),
					vk::QueryResultFlagBits::e64 | vk::QueryResultFlagBits::eWithAvailability);
				if (res == vk::Result::eSuccess) {
					resubmit_timestamps = true;
					for (usize i = 0; i < particles::timestamps; i++) {
						if (!tms[i+particles::timestamps]) { resubmit_timestamps = false; }
					}
					if (resubmit_timestamps) {
						for (usize i = 0; i < particles::timestamps; i++) {
							timestamps[i] = tms[i];
						}
					}
				} else if (res == vk::Result::eNotReady) {
					resubmit_timestamps = false;
				} else {
					logs::warnln("mikrosim", "getQueryPoolResults returned result code ", u32(res));
				}
			}
			if (running || inp.is_key_down(GLFW_KEY_SEMICOLON)) {
				advance_sim(rframe, resubmit_timestamps);
			}

			cmd.reset();
			static_cast<void>(cmd.begin(vk::CommandBufferBeginInfo{}));
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
		view_position += inp.get_cursor_delta() / view_scale;
		pos_upd = true;
	}
	if (inp.is_key_held(GLFW_KEY_A)) { view_position.x += 100.f*dt/view_scale; pos_upd = true; }
	if (inp.is_key_held(GLFW_KEY_D)) { view_position.x -= 100.f*dt/view_scale; pos_upd = true; }
	if (inp.is_key_held(GLFW_KEY_W)) { view_position.y += 100.f*dt/view_scale; pos_upd = true; }
	if (inp.is_key_held(GLFW_KEY_S)) { view_position.y -= 100.f*dt/view_scale; pos_upd = true; }
	if (pos_upd) { update_view(); }
	if (inp.is_key_down(GLFW_KEY_SPACE)) {
		running = !running;
	}
}
void mikrosim_window::advance_sim(u32 rframe, bool submit_timestamps) {
	u32 pframe = p->pframe();
	vk::CommandBuffer ucmd = update_cmds[rframe];
	ucmd.reset();
	static_cast<void>(ucmd.begin(vk::CommandBufferBeginInfo{}));
	p->step(ucmd, rframe, submit_timestamps ? *timestamp_qpool : VK_NULL_HANDLE);
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
}
void mikrosim_window::render(vk::CommandBuffer cmd, const rend::simple_mesh &bg_mesh,
	vk::Buffer particle_buff) {
	begin_swapchain_render_pass(cmd, {.9f, .9f, .9f, 1.f});
	rend2d->bind_solid_pipeline(cmd);
	win.set_viewport_and_scissor(cmd);
	rend2d->push_projection(cmd, vp);
	bg_mesh.bind_draw(cmd);

	cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, particle_draw_pl);
	win.set_viewport_and_scissor(cmd);
	particle_draw_push push{vp, particle_draw_size};
	cmd.pushConstants(*particle_draw_pll, vk::ShaderStageFlagBits::eVertex, 0, sizeof(particle_draw_push), &push);
	cmd.bindIndexBuffer(*quad_ib, 0, vk::IndexType::eUint16);
	cmd.bindVertexBuffers(0, {*quad_vb, particle_buff}, {0, 0});
	cmd.drawIndexed(6, compile_options::particle_count, 0, 0, 0);

	ImGui_ImplVulkan_NewFrame();
	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();
	if (ImGui::Begin("mikrosim")) {
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
		if (timestamps.empty()) {
			ImGui::TextColored({1.f, .5f, .06f, 1.f}, "no timestamp information avaliable");
		} else {
			f64 tsp = f64(phy_device.physical_device.properties.limits.timestampPeriod);
			std::vector<std::string> timestamp_names = {"count", "upsweep", "downsweep",
				"write", "calc density", "update" };
			if (ImGui::BeginTable("##gpu_timestamps", 3)) {
				ImGui::TableNextRow();
				ImGui::TableNextColumn();
				ImGui::TableHeader("event");
				ImGui::TableNextColumn();
				ImGui::TableHeader("timestamp");
				ImGui::TableNextColumn();
				ImGui::TableHeader("delta");
				for (usize i = 1; i < timestamps.size(); i++) {
					ImGui::TableNextRow();
					ImGui::TableNextColumn();
					ImGui::Text("%s", i-1 < timestamp_names.size() ? timestamp_names[i-1].c_str() : "unknown");
					ImGui::TableNextColumn();
					ImGui::Text("%s", format_time(u64(f64(timestamps[i] - timestamps[0]) * tsp)).c_str());
					ImGui::TableNextColumn();
					ImGui::Text("%s", format_time(u64(f64(timestamps[i] - timestamps[i-1]) * tsp)).c_str());
				}
				ImGui::EndTable();
			}
		}
		ImGui::End();
	}
	ImGui::Render();
	ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);

	cmd.endRenderPass();
}
void mikrosim_window::on_scroll(f64 dx, f64 dy) {
	static_cast<void>(dx);
	view_scale *= glm::pow(inp.is_key_held(GLFW_KEY_LEFT_SHIFT) ? 1.1f : 1.5f, f32(dy));
	update_view();
}
void mikrosim_window::update_view() {
	vp = rend2d->proj(rend::anchor2d::center) *
		glm::translate(glm::scale(glm::mat4(1.f), {view_scale, view_scale, 1.f}), {view_position, 0.f});
}

