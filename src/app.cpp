#include "app.hpp"

mikrosim_window::mikrosim_window() : rend::preset::simple_window("mikrosim", version::combined, 800, 600, "mikrosim",
	[](rend::context &ctx) {},
	[](const rend::window &win, auto &phy_device) {}) {
	win.set_user_pointer(this);
	std::vector<vk::DescriptorPoolSize> dpool_sizes = {
		{vk::DescriptorType::eCombinedImageSampler, compile_options::frames_in_flight * 64},
	};
	dpool = device.createDescriptorPool({vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet, compile_options::frames_in_flight * 256, dpool_sizes});
	setup_text();
}
void mikrosim_window::loop() {
	float tx = 0, ty = 0;
	auto text_vd = text.vb_only_text2d("this is a window", tx, ty, 16.f, 0, 18.f, {0,0,0,1});
	rend::simple_mesh text_mesh{buffer_h->make_device_buffer_dedicated_stage(text_vd, vk::BufferUsageFlagBits::eVertexBuffer), static_cast<u32>(text_vd.size())};
	mainloop<true>(
		[]() { },
		[this, &text_mesh](u32 rframe, vk::CommandBuffer cmd) {
			cmd.reset();
			static_cast<void>(cmd.begin(vk::CommandBufferBeginInfo{}));
			begin_swapchain_render_pass(cmd, {.9f, .9f, .9f, 1.f});
			rend2d->bind_text_pipeline(cmd);
			win.set_viewport_and_scissor(cmd);
			rend::renderer2d::text_push tp{text.scr_px_range2d(16.f)};
			rend2d->push_text_constants(cmd, tp);
			rend2d->bind_texture_text(cmd, text.get_ds2d(rframe));
			rend2d->push_projection(cmd, rend2d->proj(rend::anchor2d::topleft));
			text_mesh.bind_draw(cmd);
			cmd.endRenderPass();
			cmd.end();
		}
	);
	device.waitIdle();
}

