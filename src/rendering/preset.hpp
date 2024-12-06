#pragma once
#include <optional>
#include <vector>
#include "../input/input.hpp"
#include "2d.hpp"
#include "buffers.hpp"
#include "context.hpp"
#include "renderer.hpp"
#include "text.hpp"
#include "vma_wrap.hpp"

namespace rend::preset {
	class simple_window {
	public:
		context ctx;
		vk::raii::Device device;
		window win;
		physical_device_manager_swap<queue_families::graphics_compute, queue_families::present> phy_device;
		rend::namer namer;
		vk::raii::Queue graphics_compute_queue;
		vk::raii::Queue present_queue;
		vk::raii::RenderPass swapchain_render_pass;
		std::vector<vk::raii::Framebuffer> swapchain_framebuffers;
		vma::allocator allocator;
		vk::raii::CommandPool command_pool;
		std::vector<vk::raii::CommandBuffer> main_command_buffers;
		std::array<vk::raii::Semaphore, compile_options::frames_in_flight> render_finished_semaphores;
		std::optional<renderer2d> rend2d;
		std::optional<main_render_driver<compile_options::frames_in_flight>> driver;
		std::optional<buffer_handler> buffer_h;
		vk::raii::DescriptorPool dpool;
		rend::text text;
		std::vector<vk::Semaphore> render_submit_semaphores;
		std::vector<vk::PipelineStageFlags> render_submit_semaphore_stages;
		input::input_handler input;

		template<typename F1, typename F2>
		inline simple_window(const char *name, u32 version_id, u32 width, u32 height, const char *title, F1 setup_instance_extensions, F2 setup_physical_device_requirements) :
			device(nullptr), win(ctx), namer(ctx, nullptr), graphics_compute_queue(nullptr), present_queue(nullptr), swapchain_render_pass(nullptr), allocator(nullptr),
			command_pool(nullptr), render_finished_semaphores(array_init<vk::raii::Semaphore, compile_options::frames_in_flight>(nullptr)), dpool(nullptr) {
			setup_instance_extensions(ctx);
			ctx.init_vulkan_instance(name, version_id);
			win.late_init(width, height, title);
			setup_physical_device_requirements(win, phy_device);
			phy_device.select_physical(ctx.vulkan_instance_raii(), win.vulkan_surface());
			device = phy_device.logical_simple();
			namer.set_device(*device); namer(*device, "main device");
			graphics_compute_queue = device.getQueue(phy_device.queue_family_ids[0], 0); namer(*graphics_compute_queue, "main graphics/compute queue");
			present_queue = device.getQueue(phy_device.queue_family_ids[1], 0); namer(*present_queue, "main present queue");
			win.set_device(&phy_device.physical_device.handle, &device);
			win.init_swapchain(true);

			vk::AttachmentDescription att_desc{{}, win.vulkan_swapchain_format(), vk::SampleCountFlagBits::e1, vk::AttachmentLoadOp::eClear, vk::AttachmentStoreOp::eStore,
				vk::AttachmentLoadOp::eDontCare, vk::AttachmentStoreOp::eDontCare, vk::ImageLayout::eUndefined, vk::ImageLayout::ePresentSrcKHR};
			vk::AttachmentReference att_ref{0, vk::ImageLayout::eColorAttachmentOptimal};
			vk::SubpassDescription sub_desc{{}, vk::PipelineBindPoint::eGraphics, {}, att_ref, {}, {}, {}};
			vk::SubpassDependency sub_dep{VK_SUBPASS_EXTERNAL, 0, vk::PipelineStageFlagBits::eColorAttachmentOutput, vk::PipelineStageFlagBits::eColorAttachmentOutput,
				{}, vk::AccessFlagBits::eColorAttachmentWrite, {}};
			swapchain_render_pass = device.createRenderPass(vk::RenderPassCreateInfo{{}, att_desc, sub_desc, sub_dep});
			namer(*swapchain_render_pass, "swapchain render pass");
			win.make_framebuffers(swapchain_framebuffers, *swapchain_render_pass);

			allocator.late_init(VmaAllocatorCreateInfo{{}, *phy_device.physical_device.handle, *device, {}, nullptr, {}, {}, {}, ctx.vulkan_instance(), context::vk_api_version, nullptr});

			command_pool = device.createCommandPool({vk::CommandPoolCreateFlagBits::eResetCommandBuffer, phy_device.queue_family_ids[0]});
			namer(*command_pool, "main command pool");
			main_command_buffers = device.allocateCommandBuffers({*command_pool, vk::CommandBufferLevel::ePrimary, compile_options::frames_in_flight});
			namer.raii_range(main_command_buffers.begin(), main_command_buffers.end(), "main command buffer");

			for (usize i = 0; i < compile_options::frames_in_flight; i++) {
				render_finished_semaphores[i] = device.createSemaphore({}); namer(*render_finished_semaphores[i], "render finished semaphore", static_cast<u32>(i));
			}

			rend2d.emplace(renderer2d(ctx, device, *swapchain_render_pass, width, height));
			driver.emplace(main_render_driver<compile_options::frames_in_flight>(ctx, win, device, *present_queue));
			buffer_h.emplace(buffer_handler(*phy_device.physical_device.handle, device, allocator, *command_pool, *graphics_compute_queue));
			text.load_ascii_atlas(*buffer_h);
		}
		~simple_window();

		void setup_text();
		template<bool poll, typename F1, typename F2, typename F3=void (*)()> inline void mainloop(F1 update, F2 render, F3 resize_callback=[]() {}) {
			while (!win.should_close()) {
				tick<poll>(update,render,resize_callback);
			}
		}
		template<bool poll, typename F1, typename F2, typename F3=void (*)()> inline void tick(F1 update, F2 render, F3 resize_callback=[]() {}) {
			if constexpr (poll) {
				window::poll_events();
			}
			input.update(win);
			update();
			const auto swap_recreated = driver->begin_frame();
			if (swap_recreated.has_value()) {
				if (*swap_recreated) {
					win.make_framebuffers(swapchain_framebuffers, *swapchain_render_pass);
					rend2d->resize(static_cast<f32>(win.width()), static_cast<f32>(win.height()));
					resize_callback();
				}
			} else {
				vk::CommandBuffer command_buffer = *main_command_buffers[driver->frame()];
				render_submit_semaphores.clear();
				render_submit_semaphore_stages.clear();
				render(driver->frame(), command_buffer);
				render_submit_semaphores.push_back(driver->image_acquired_semaphore());
				render_submit_semaphore_stages.push_back(vk::PipelineStageFlagBits::eColorAttachmentOutput);
				graphics_compute_queue.submit(vk::SubmitInfo{render_submit_semaphores, render_submit_semaphore_stages, command_buffer, *render_finished_semaphores[driver->frame()]}, driver->frame_in_flight_fence());
				driver->wait_semaphores.push_back(*render_finished_semaphores[driver->frame()]);
				if (driver->end_frame()) {
					win.make_framebuffers(swapchain_framebuffers, *swapchain_render_pass);
					rend2d->resize(static_cast<f32>(win.width()), static_cast<f32>(win.height()));
					resize_callback();
				}
			}
		}
		void begin_swapchain_render_pass(vk::CommandBuffer command_buffer, const glm::vec4 &clear_color) const;
	};
}
