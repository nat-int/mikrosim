#pragma once

#include <array>
#include <optional>
#include "../util.hpp"
#include "../vulkan_glfw_pch.hpp"
#include "context.hpp"

namespace rend {
	template<u32 frames_in_flight>
	class main_render_driver {
	public:
		std::vector<vk::Semaphore> wait_semaphores;

		inline main_render_driver(const context &ctx, window &win, const vk::raii::Device &device, vk::Queue present_queue) : ctx(ctx), win(win), device(device), present_queue(present_queue),
			frame_in_flight(0),
			image_acquired_semaphores(array_init<vk::raii::Semaphore, frames_in_flight>(nullptr)), frame_in_flight_fences(array_init<vk::raii::Fence, frames_in_flight>(nullptr)) {
			for (usize i = 0; i < frames_in_flight; i++) {
				image_acquired_semaphores[i] = device.createSemaphore({});
				ctx.set_object_name(*device, *image_acquired_semaphores[i], ("render driver image acquired semaphore #"+std::to_string(i)).c_str());
				frame_in_flight_fences[i] = device.createFence({ vk::FenceCreateFlagBits::eSignaled });
				ctx.set_object_name(*device, *frame_in_flight_fences[i], ("render driver frame fence #"+std::to_string(i)).c_str());
			}
		}
		inline std::optional<bool> begin_frame() {
			vk::Result res = device.waitForFences(*frame_in_flight_fences[frame_in_flight], true, std::numeric_limits<u64>::max());
			if (res != vk::Result::eSuccess) {
				logs::errorln("render driver", "wait for fences returned non-success - ", vk::to_string(res));
			}
			auto acquire_result = win.vulkan_swapchain_raii().acquireNextImage(std::numeric_limits<u64>::max(), *image_acquired_semaphores[frame_in_flight], {});
			if (acquire_result.first == vk::Result::eErrorOutOfDateKHR) {
				device.waitIdle();
				win.recreate_swapchain();
				return true;
			} else if (acquire_result.first != vk::Result::eSuccess && acquire_result.first != vk::Result::eSuboptimalKHR) {
				logs::errorln("vulkan swapchain", "Failed to acquire swapchain image - ", vk::to_string(acquire_result.first));
				return false;
			}
			device.resetFences(*frame_in_flight_fences[frame_in_flight]);
			current_image = acquire_result.second;
			return std::nullopt;
		}
		inline bool end_frame() {
			vk::Result res = present_queue.presentKHR(vk::PresentInfoKHR{wait_semaphores, win.vulkan_swapchain(), current_image, nullptr});
			wait_semaphores.clear();
			if (res == vk::Result::eErrorOutOfDateKHR || res == vk::Result::eSuboptimalKHR || win.is_swapchain_outdated()) {
				device.waitIdle();
				win.recreate_swapchain();
				return true;
			} else if (res != vk::Result::eSuccess) {
				logs::errorln("render driver", "presentKHR returned non-success - ", vk::to_string(res));
			}
			frame_in_flight = (frame_in_flight + 1) % frames_in_flight;
			return false;
		}
		inline u32 frame() const { return frame_in_flight; }
		inline u32 image() const { return current_image; }
		inline const vk::Semaphore image_acquired_semaphore() const { return *image_acquired_semaphores[frame_in_flight]; }
		inline vk::Fence frame_in_flight_fence() const { return *frame_in_flight_fences[frame_in_flight]; }
	private:
		const context &ctx;
		window &win;
		const vk::raii::Device &device;
		vk::Queue present_queue;
		u32 frame_in_flight, current_image;
		std::array<vk::raii::Semaphore, frames_in_flight> image_acquired_semaphores;
		std::array<vk::raii::Fence, frames_in_flight> frame_in_flight_fences;
	};
}
