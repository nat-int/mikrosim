#pragma once

#include <vector>
#include "../vulkan_glfw_pch.hpp"
#include "../defs.hpp"
#include "device.hpp"
#include "window.hpp"

namespace rend {
	template<typename T> void set_object_name(const vk::DispatchLoaderDynamic &dispatcher, vk::Device device, T object, const char *name) {
		if constexpr (compile_options::vulkan_validation) {
			device.setDebugUtilsObjectNameEXT({T::objectType,reinterpret_cast<uint64_t>(static_cast<T::CType>(object)), name}, dispatcher);
		}
	}
	class context;
	class namer {
	public:
		namer(const context &ctx, vk::Device device);
		template<typename T> void operator()(T object, const char *name) const;
		template<typename T> void operator()(T object, const char *name, u32 index) const;
		template<typename TI> void operator()(TI begin, TI end, const char *name) const;
		template<typename TI> inline void raii_range(TI begin, TI end, const char *name) const;
		void set_device(vk::Device device);
	private:
		const context &ctx;
		vk::Device device;
	};
	class context {
		friend class namer;
	public:
		static constexpr u32 vk_api_version = VK_API_VERSION_1_3;

		context();
		context(const context &) = delete;
		~context();

		void require_vulkan_extension(const char *extension);
		void require_vulkan_layer(const char *layer);
		void init_vulkan_instance(const char *app_name, u32 app_ver);
		window make_window(u32 width, u32 height, const char *title);
		inline const vk::Instance &vulkan_instance() const { return *vk_instance; }
		inline const vk::raii::Instance &vulkan_instance_raii() const { return vk_instance; }
		inline const vk::DispatchLoaderDynamic &vulkan_dynamic_dispatcher() const { return dynamic_dispatcher; }
		template<typename F> inline physical_device_info select_physical_device(F heuristic) { return rend::select_physical_device(vk_instance, heuristic); }
		template<typename QueueFinder, typename F, typename FQE> inline physical_device_info select_physical_device_with_queues(F heuristic, vk::SurfaceKHR surface, FQE queue_extractor) const {
			return rend::select_physical_device_with_queues<QueueFinder>(vk_instance, heuristic, surface, queue_extractor);
		}
		template<typename T> inline void set_object_name(vk::Device device, T object, const char *name) const {
			rend::set_object_name(dynamic_dispatcher, device, object, name);
		}
		namer get_namer(vk::Device) const;
	private:
		static u32 glfw_context_counter;

		vk::raii::Context vk_ctx;
		std::vector<const char *> enabled_vulkan_extensions;
		std::vector<const char *> enabled_vulkan_layers;
		vk::raii::Instance vk_instance;
		vk::raii::DebugUtilsMessengerEXT vk_debug_utils_messenger;
		vk::DispatchLoaderDynamic dynamic_dispatcher;
	};
	template<typename T> inline void namer::operator()(T object, const char *name) const {
		rend::set_object_name(ctx.dynamic_dispatcher, device, object, name);
	}
	template<typename T> inline void namer::operator()(T object, const char *name, u32 index) const {
		rend::set_object_name(ctx.dynamic_dispatcher, device, object, (std::string(name)+" #"+std::to_string(index)).c_str());
	}
	template<typename TI> inline void namer::operator()(TI begin, TI end, const char *name) const {
		for (u32 counter = 0; begin != end; begin++, counter++) {
			(*this)(*begin, name, counter);
		}
	}
	template<typename TI> inline void namer::raii_range(TI begin, TI end, const char *name) const {
		for (u32 counter = 0; begin != end; begin++, counter++) {
			(*this)(**begin, name, counter);
		}
	}
}
