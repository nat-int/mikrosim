#include "app.hpp"

struct particle_draw_push {
	glm::mat4 proj;
	f32 psize;
};

mikrosim_window::mikrosim_window() : rend::preset::simple_window("mikrosim", version::combined, 800, 600, "mikrosim",
	[](rend::context &ctx) { static_cast<void>(ctx); },
	[](const rend::window &win, auto &phy_device) { static_cast<void>(win);
		static_cast<void>(phy_device); }), first_frame(true), quad_vb(nullptr), quad_ib(nullptr),
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

	p.emplace(ctx, device, *buffer_h, dpool);

	running = false;
	view_scale = 400.f / compile_options::cells_x;
	view_position = {f32(compile_options::cells_x) / -2.f, f32(compile_options::cells_y) / -2.f};
	update_view();

	win.bind_scroll_callback<mikrosim_window>();
}
void mikrosim_window::loop() {
	float tx = 0, ty = 0;
	auto text_vd = text.vb_only_text2d("this is a window", tx, ty, 16.f, 0, 18.f, {0,0,0,1});
	rend::simple_mesh text_mesh{buffer_h->make_device_buffer_dedicated_stage(text_vd, vk::BufferUsageFlagBits::eVertexBuffer), static_cast<u32>(text_vd.size())};

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

	mainloop<true>(
		[this]() {
			inp.update(win);
			if (inp.is_mouse_held(GLFW_MOUSE_BUTTON_MIDDLE) &&
				!inp.is_mouse_down(GLFW_MOUSE_BUTTON_MIDDLE)) {
				view_position += inp.get_cursor_delta() / view_scale;
				update_view();
			}
			if (inp.is_key_down(GLFW_KEY_SPACE)) {
				running = !running;
			}
		},
		[this, &text_mesh, &debug_bg_mesh](u32 rframe, vk::CommandBuffer cmd) {
			vk::Buffer particle_buff = p->particle_buff();
			if (running || inp.is_key_down(GLFW_KEY_SEMICOLON)) {
				u32 pframe = p->pframe();
				vk::CommandBuffer ucmd = update_cmds[rframe];
				ucmd.reset();
				static_cast<void>(ucmd.begin(vk::CommandBufferBeginInfo{}));
				p->step(ucmd, rframe);
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

			cmd.reset();
			static_cast<void>(cmd.begin(vk::CommandBufferBeginInfo{}));
			begin_swapchain_render_pass(cmd, {.9f, .9f, .9f, 1.f});
			rend2d->bind_solid_pipeline(cmd);
			win.set_viewport_and_scissor(cmd);
			rend2d->push_projection(cmd, vp);
			debug_bg_mesh.bind_draw(cmd);

			cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, particle_draw_pl);
			win.set_viewport_and_scissor(cmd);
			particle_draw_push push{vp, 0.1f};
			cmd.pushConstants(*particle_draw_pll, vk::ShaderStageFlagBits::eVertex, 0, sizeof(particle_draw_push), &push);
			cmd.bindIndexBuffer(*quad_ib, 0, vk::IndexType::eUint16);
			cmd.bindVertexBuffers(0, {*quad_vb, particle_buff}, {0, 0});
			cmd.drawIndexed(6, compile_options::particle_count, 0, 0, 0);

			rend2d->bind_text_pipeline(cmd);
			win.set_viewport_and_scissor(cmd);
			rend::renderer2d::text_push tp{text.scr_px_range2d(16.f)};
			rend2d->push_text_constants(cmd, tp);
			rend2d->bind_texture_text(cmd, text.get_ds2d(rframe));
			rend2d->push_text_projection(cmd, rend2d->proj(rend::anchor2d::topleft));
			text_mesh.bind_draw(cmd);
			cmd.endRenderPass();
			cmd.end();
		},
		[this]() {
			update_view();
		}
	);
	device.waitIdle();
}
void mikrosim_window::on_scroll(f64 dx, f64 dy) {
	static_cast<void>(dx);
	view_scale *= glm::pow(1.5f, f32(dy));
	update_view();
}
void mikrosim_window::update_view() {
	vp = rend2d->proj(rend::anchor2d::center) *
		glm::translate(glm::scale(glm::mat4(1.f), {view_scale, view_scale, 1.f}), {view_position, 0.f});
}

