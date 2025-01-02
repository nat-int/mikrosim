#pragma once

#include "../vulkan_glfw_pch.hpp"
#include "../defs.hpp"
#include "device.hpp"

struct GLFWwindow;
namespace input { class input_handler; }
namespace rend {
	class context;
	class window {
		friend class input::input_handler;
	public:
		window(context &rendctx, u32 width, u32 height, const char *title);
		window(context &rendctx);
		window(const window &) = delete;
		~window();

		void late_init(u32 width, u32 height, const char *title);

		bool should_close() const;
		void set_device(const vk::raii::PhysicalDevice *physical_device, const vk::raii::Device *device);
		void init_swapchain(bool vsync=false);
		void recreate_swapchain();
		void set_swapchain_outdated();
		bool is_swapchain_outdated() const;
		void make_framebuffers(std::vector<vk::raii::Framebuffer> &out, const vk::RenderPass render_pass) const;
		void set_viewport_and_scissor(vk::CommandBuffer command_buffer) const;

		static void poll_events();
		static void wait_events();
		static void wait_events_timeout(double timeout);

		template<typename F> inline auto require_surface_format_and_present_mode(F prev_heuristic) { return heuristics::require_surface_format_and_present_mode(*vk_surface, prev_heuristic); }
		const vk::SurfaceKHR &vulkan_surface() const;
		const vk::raii::SurfaceKHR &vulkan_surface_raii() const;
		const vk::SwapchainKHR &vulkan_swapchain() const;
		const vk::raii::SwapchainKHR &vulkan_swapchain_raii() const;
		vk::Format vulkan_swapchain_format() const;
		vk::ColorSpaceKHR vulkan_swapchain_color_space() const;
		vk::PresentModeKHR vulkan_swapchain_present_mode() const;
		vk::Extent2D vulkan_swapchain_extent() const;
		size_t vulkan_swapchain_image_count() const;
		vk::Image vulkan_swapchain_image(u32 i) const;
		vk::ImageView vulkan_swapchain_image_view(u32 i) const;
		u32 width() const;
		u32 height() const;

#define WINDOW_FORWARD_CALLBACK(callback, callback_cc) \
			template<typename T> inline void bind_##callback##_callback() const { glfwSet##callback_cc##Callback(window_handle, [](GLFWwindow *w, auto... args) {\
				window *this_ = static_cast<window *>(glfwGetWindowUserPointer(w));\
				static_cast<T *>(this_->user_ptr)->WINDOW_CALLBACK_GENERATOR(callback)(args...);\
			}); }
		WINDOW_FORWARD_CALLBACK(windowpos, WindowPos)
		WINDOW_FORWARD_CALLBACK(windowsize, WindowSize)
		WINDOW_FORWARD_CALLBACK(windowclose, WindowClose)
		WINDOW_FORWARD_CALLBACK(windowrefresh, WindowRefresh)
		WINDOW_FORWARD_CALLBACK(windowfocus, WindowFocus)
		WINDOW_FORWARD_CALLBACK(windowiconify, WindowIconify)
		WINDOW_FORWARD_CALLBACK(windowmaximize, WindowMaximize)
		WINDOW_FORWARD_CALLBACK(windowcontentscale, WindowContentScale)
		// framebuffer size callback bind isn't allowed
		WINDOW_FORWARD_CALLBACK(key, Key)
		WINDOW_FORWARD_CALLBACK(char, Char)
		WINDOW_FORWARD_CALLBACK(charmods, CharMods)
		WINDOW_FORWARD_CALLBACK(mousebutton, MouseButton)
		WINDOW_FORWARD_CALLBACK(cursorpos, CursorPos)
		WINDOW_FORWARD_CALLBACK(cursorenter, CursorEnter)
		WINDOW_FORWARD_CALLBACK(scroll, Scroll)
		WINDOW_FORWARD_CALLBACK(drop, Drop)
		WINDOW_FORWARD_CALLBACK(joystick, Joystick)
#undef WINDOW_FORWARD_CALLBACK

		void set_user_pointer(void *up);
		glm::dvec2 get_cursor_pos() const;
		GLFWwindow *raw() const;
	private:
		void *user_ptr;
		GLFWwindow *window_handle;
		u32 wwidth, wheight;
		const context &rendctx;
		vk::raii::SurfaceKHR vk_surface;
		const vk::raii::PhysicalDevice *vk_physical_device;
		const vk::raii::Device *vk_device;
		vk::SurfaceFormatKHR vk_swapchain_format;
		vk::PresentModeKHR vk_swapchain_present_mode;
		vk::Extent2D vk_swapchain_extent;
		vk::raii::SwapchainKHR vk_swapchain;
		std::vector<vk::Image> vk_swapchain_images;
		std::vector<vk::raii::ImageView> vk_swapchain_image_views;
		bool swapchain_outdated;

		void create(const char *title);
	};
}
