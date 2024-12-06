#include "preset.hpp"

namespace rend::preset {
	simple_window::~simple_window() { device.waitIdle(); }
	void simple_window::setup_text() {
		text.init_ascii_atlas_ds2d(*rend2d, *dpool);
	}
	void simple_window::begin_swapchain_render_pass(vk::CommandBuffer command_buffer, const glm::vec4 &clear_color) const {
		vk::ClearValue clear_value = {{clear_color.r, clear_color.g, clear_color.b, clear_color.a}};
		command_buffer.beginRenderPass({*swapchain_render_pass, *swapchain_framebuffers[driver->image()], { { 0,0 }, win.vulkan_swapchain_extent() }, clear_value }, vk::SubpassContents::eInline);
	}
}
