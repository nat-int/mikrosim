#include "window.hpp"

#include <algorithm>
#include <limits>
#include <optional>
#include <stdexcept>
#include "../log/log.hpp"
#include "context.hpp"

namespace rend {
	window::window(context &rendctx, u32 width, u32 height, const char *title) : wwidth(width), wheight(height), rendctx(rendctx), vk_surface(nullptr), vk_physical_device(nullptr),
		vk_device(nullptr), vk_swapchain(nullptr), swapchain_outdated(true) {
		create(title);
	}
	window::window(context &rendctx) : wwidth(0), wheight(0), rendctx(rendctx), vk_surface(nullptr), vk_physical_device(nullptr), vk_device(nullptr), vk_swapchain(nullptr),
		swapchain_outdated(true) { }
	window::~window() {
		vk_swapchain_image_views.clear();
		vk_swapchain.clear();
		vk_surface.clear();
		glfwDestroyWindow(window_handle);
	}
	void window::late_init(u32 width, u32 height, const char *title) {
		wwidth = width;
		wheight = height;
		create(title);
	}
	bool window::should_close() const { return glfwWindowShouldClose(window_handle); }
	void window::set_device(const vk::raii::PhysicalDevice *physical_device, const vk::raii::Device *device) {
		vk_physical_device = physical_device;
		vk_device = device;
	}
	void window::init_swapchain(bool vsync) {
		std::vector<vk::SurfaceFormatKHR> formats = vk_physical_device->getSurfaceFormatsKHR(*vk_surface);
		std::optional<vk::SurfaceFormatKHR> selected_format;
		for (const vk::SurfaceFormatKHR &format : formats) {
			if (format.format == vk::Format::eB8G8R8A8Srgb && format.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear) {
				selected_format = format;
				break;
			} else if (format.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear) {
				selected_format = format;
			}
		}
		if (!selected_format) {
			selected_format = formats[0];
		}
		vk_swapchain_format = *selected_format;
		vk_swapchain_present_mode = vk::PresentModeKHR::eFifo;
		if (!vsync) {
			if (std::ranges::any_of(vk_physical_device->getSurfacePresentModesKHR(*vk_surface), [](const vk::PresentModeKHR pm) { return pm == vk::PresentModeKHR::eMailbox; })) {
				vk_swapchain_present_mode = vk::PresentModeKHR::eMailbox;
				logs::infoln("window", "using mailbox present mode");
			} else {
				if (std::ranges::any_of(vk_physical_device->getSurfacePresentModesKHR(*vk_surface), [](const vk::PresentModeKHR pm) { return pm == vk::PresentModeKHR::eImmediate; })) {
					vk_swapchain_present_mode = vk::PresentModeKHR::eImmediate;
					logs::infoln("window", "using immediate present mode");
				} else {
					logs::infoln("window", "fallback to vsync");
				}
			}
		}
		recreate_swapchain();
	}
	void window::recreate_swapchain() {
		swapchain_outdated = false;
		vk::SurfaceCapabilitiesKHR surface_capabilities = vk_physical_device->getSurfaceCapabilitiesKHR(*vk_surface);
		vk_swapchain_extent = surface_capabilities.currentExtent;
		if (vk_swapchain_extent.width == std::numeric_limits<u32>::max()) {
			vk_swapchain_extent.width = std::clamp<u32>(wwidth, surface_capabilities.minImageExtent.width, surface_capabilities.maxImageExtent.width);
			vk_swapchain_extent.height = std::clamp<u32>(wheight, surface_capabilities.minImageExtent.height, surface_capabilities.maxImageExtent.height);
		}
		vk::raii::SwapchainKHR old(std::move(vk_swapchain));
		const u32 image_count = surface_capabilities.maxImageCount ? std::min(surface_capabilities.minImageCount + 1, surface_capabilities.maxImageCount) : surface_capabilities.minImageCount + 1;
		vk_swapchain = vk_device->createSwapchainKHR({{}, *vk_surface, image_count, vk_swapchain_format.format, vk_swapchain_format.colorSpace, vk_swapchain_extent, 1,
			vk::ImageUsageFlagBits::eColorAttachment, vk::SharingMode::eExclusive, {}, surface_capabilities.currentTransform, vk::CompositeAlphaFlagBitsKHR::eOpaque,
			vk_swapchain_present_mode, false, *old});
		rendctx.set_object_name(**vk_device, *vk_swapchain, "constantan window swapchain");
		vk_swapchain_images = vk_swapchain.getImages();
		vk_swapchain_image_views.clear();
		vk_swapchain_image_views.reserve(vk_swapchain_images.size());
		for (usize i = 0; i < vk_swapchain_images.size(); i++) {
			rendctx.set_object_name(**vk_device, vk_swapchain_images[i], ("constantan window swapchain image #"+std::to_string(i)).c_str());
			vk_swapchain_image_views.push_back(vk_device->createImageView({{}, vk_swapchain_images[i], vk::ImageViewType::e2D, vk_swapchain_format.format,
				vk::ComponentMapping{}, { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 }}));
			rendctx.set_object_name(**vk_device, *vk_swapchain_image_views[i], ("constantan window swapchain image view #"+std::to_string(i)).c_str());
		}
	}
	void window::set_swapchain_outdated() { swapchain_outdated = true; }
	bool window::is_swapchain_outdated() const { return swapchain_outdated; }
	void window::make_framebuffers(std::vector<vk::raii::Framebuffer> &out, const vk::RenderPass render_pass) const {
		out.clear();
		out.reserve(vk_swapchain_images.size());
		for (usize i = 0; i < vk_swapchain_images.size(); i++) {
			const vk::ImageView image_view = *vk_swapchain_image_views[i];
			out.push_back(vk_device->createFramebuffer(vk::FramebufferCreateInfo{{}, render_pass, image_view, vk_swapchain_extent.width, vk_swapchain_extent.height, 1 }));
			rendctx.set_object_name(**vk_device, *out.back(), ("constantan window swapchain framebuffer #"+std::to_string(i)).c_str());
		}
	}
	void window::set_viewport_and_scissor(vk::CommandBuffer command_buffer) const {
		command_buffer.setViewport(0, vk::Viewport{0.f, 0.f, float(vk_swapchain_extent.width), float(vk_swapchain_extent.height), 0.f, 1.f});
		command_buffer.setScissor(0, vk::Rect2D{{ 0,0 }, vk_swapchain_extent});
	}

	void window::poll_events() { glfwPollEvents(); }
	void window::wait_events() { glfwWaitEvents(); }
	void window::wait_events_timeout(double timeout) { glfwWaitEventsTimeout(timeout); }

	const vk::SurfaceKHR &window::vulkan_surface() const { return *vk_surface; }
	const vk::raii::SurfaceKHR &window::vulkan_surface_raii() const { return vk_surface; }
	const vk::SwapchainKHR &window::vulkan_swapchain() const { return *vk_swapchain; }
	const vk::raii::SwapchainKHR &window::vulkan_swapchain_raii() const { return vk_swapchain; }
	vk::Format window::vulkan_swapchain_format() const { return vk_swapchain_format.format; }
	vk::ColorSpaceKHR window::vulkan_swapchain_color_space() const { return vk_swapchain_format.colorSpace; }
	vk::PresentModeKHR window::vulkan_swapchain_present_mode() const { return vk_swapchain_present_mode; }
	vk::Extent2D window::vulkan_swapchain_extent() const { return vk_swapchain_extent; }
	size_t window::vulkan_swapchain_image_count() const { return vk_swapchain_images.size(); }
	vk::Image window::vulkan_swapchain_image(u32 i) const { return vk_swapchain_images[i]; }
	vk::ImageView window::vulkan_swapchain_image_view(u32 i) const { return *vk_swapchain_image_views[i]; }
	u32 window::width() const { return wwidth; }
	u32 window::height() const { return wheight; }
	void window::set_user_pointer(void *up) { user_ptr = up; }
	glm::dvec2 window::get_cursor_pos() const { glm::dvec2 out; glfwGetCursorPos(window_handle, &out.x, &out.y); return out; }

	void window::create(const char *title) {
		window_handle = glfwCreateWindow(i32(wwidth), i32(wheight), title, NULL, NULL);
		if (!window_handle) {
			logs::errorln("window", "Couldn't create window!");
			throw std::runtime_error("window creation failed");
		}
		glfwSetFramebufferSizeCallback(window_handle, [](GLFWwindow *w, int width, int height) {
			window *this_ = reinterpret_cast<window *>(glfwGetWindowUserPointer(w));
			this_->wwidth = u32(width);
			this_->wheight = u32(height);
			this_->swapchain_outdated = true;
		});
		glfwSetWindowUserPointer(window_handle, this);
		VkSurfaceKHR c_surface;
		VkResult result_create_surface = glfwCreateWindowSurface(rendctx.vulkan_instance(), window_handle, nullptr, &c_surface);
		if (result_create_surface != VK_SUCCESS) {
			logs::errorln("window", "Couldn't create vulkan surface!");
			throw std::runtime_error("vulkan surface creation failed");
		}
		vk_surface = vk::raii::SurfaceKHR(rendctx.vulkan_instance_raii(), c_surface);
	}
}
