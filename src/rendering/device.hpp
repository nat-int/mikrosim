#pragma once

#include <algorithm>
#include <array>
#include <optional>
#include <set>
#include <stdexcept>
#include "../vulkan_glfw_pch.hpp"
#include "../cmake_defs.hpp"
#include "../defs.hpp"
#include "../log/log.hpp"

namespace rend {
	class physical_device_info {
	public:
		vk::raii::PhysicalDevice handle;
		vk::PhysicalDeviceProperties properties;
		vk::PhysicalDeviceFeatures features;

		physical_device_info();
		physical_device_info(vk::raii::PhysicalDevice &&handle);
	};
	namespace queue_families {
		struct graphics { bool operator()(const u32 id, vk::PhysicalDevice physical_device, const vk::QueueFamilyProperties &queue_family, vk::SurfaceKHR surface) const; };
		struct present { bool operator()(const u32 id, vk::PhysicalDevice physical_device, const vk::QueueFamilyProperties &queue_family, vk::SurfaceKHR surface) const; };
		struct graphics_compute { bool operator()(const u32 id, vk::PhysicalDevice physical_device, const vk::QueueFamilyProperties &queue_family, vk::SurfaceKHR surface) const; };
		struct compute { bool operator()(const u32 id, vk::PhysicalDevice physical_device, const vk::QueueFamilyProperties &queue_family, vk::SurfaceKHR surface) const; };
	}
	template<typename ...Ts>
	class queue_family_index_finder {
	public:
		std::array<std::optional<u32>, sizeof...(Ts)> indices;

		inline queue_family_index_finder(vk::PhysicalDevice physical_device, vk::SurfaceKHR surface) {
			u32 id = 0;
			for (const vk::QueueFamilyProperties &queue_family : physical_device.getQueueFamilyProperties()) {
				check_queue_family<0, Ts...>(physical_device, surface, queue_family, id++);
			}
		}
		inline bool all_found() const { return std::ranges::all_of(indices, [](const auto &i) { return i.has_value(); }); }
	private:
		template<usize I, typename T, typename ...Us>
		inline void check_queue_family(vk::PhysicalDevice physical_device, vk::SurfaceKHR surface, const vk::QueueFamilyProperties &queue_family, const u32 id) {
			if (T{}(id, physical_device, queue_family, surface)) { indices[I] = id; }
			if constexpr (sizeof...(Us) > 0) {
				check_queue_family<I+1, Us...>(physical_device, surface, queue_family, id);
			}
		}
	};

	namespace heuristics {
		std::optional<std::string> check_extensions(const physical_device_info &info, const std::vector<const char *> &extensions);
		std::optional<std::string> check_surface_format_and_present_mode(const physical_device_info &info, vk::SurfaceKHR surface);

		std::pair<u32, std::string> preference_by_type(const physical_device_info &info);
		template<typename F>
		class require_extensions {
		public:
			F prev_heuristic;
			const std::vector<const char *> &extensions;

			inline require_extensions(const std::vector<const char *> &extensions, F prev_heuristic) : prev_heuristic(prev_heuristic), extensions(extensions) { }
			inline std::pair<u32, std::string> operator()(const physical_device_info &info) {
				std::optional<std::string> res = check_extensions(info, extensions);
				return res.has_value() ? std::make_pair(0, *res) : prev_heuristic(info);
			}
		};
		template<typename F>
		class require_surface_format_and_present_mode {
		public:
			F prev_heuristic;
			vk::SurfaceKHR surface;

			inline require_surface_format_and_present_mode(vk::SurfaceKHR surface, F prev_heuristic) : prev_heuristic(prev_heuristic), surface(surface) { }
			inline std::pair<u32, std::string> operator()(const physical_device_info &info) {
				std::optional<std::string> res = check_surface_format_and_present_mode(info, surface);
				return res.has_value() ? std::make_pair(0, *res) : prev_heuristic(info);
			}
		};
	}

	template<typename F>
	inline physical_device_info select_physical_device(const vk::raii::Instance &instance, F heuristic) {
		physical_device_info best;
		u32 score = 0;
		std::vector<vk::raii::PhysicalDevice> physical_devices = instance.enumeratePhysicalDevices();
		for (usize i = 0; i < physical_devices.size(); i++) {
			physical_device_info info(std::move(physical_devices[i]));
			const std::pair<u32, std::string> curr_score = heuristic(info); // if heuristic returns 0, it means device isn't suitable
			if (curr_score.first > score) {
				logs::debugln("physical device selector", "New best candidate found: ", info.properties.deviceName, " {", curr_score.second, '}');
				best = info;
				score = curr_score.first;
			} else {
				logs::debugln("physical device selector", "Physical device not selected: ", info.properties.deviceName, " {", curr_score.second, '}');
			}
		}
		if (!*best.handle) {
			logs::errorln("physical device selector", "No physical device was found suitable - failed to select a physical device!");
			throw std::runtime_error("physical_devices failed to select any device!");
		}
		return best;
	}
	template<typename QueueFinder, typename F, typename FQE>
	inline physical_device_info select_physical_device_with_queues(const vk::raii::Instance &instance, F heuristic, vk::SurfaceKHR surface, FQE queue_extractor) {
		physical_device_info best;
		std::optional<QueueFinder> best_queues;
		u32 score = 0;
		std::vector<vk::raii::PhysicalDevice> physical_devices = instance.enumeratePhysicalDevices();
		for (usize i = 0; i < physical_devices.size(); i++) {
			physical_device_info info(std::move(physical_devices[i]));
			QueueFinder queues(*info.handle, surface);
			const std::pair<u32, std::string> curr_score = heuristic(info); // if heuristic returns 0, it means device isn't suitable
			if (curr_score.first > score) {
				logs::debugln("physical device selector", "New best candidate found: ", info.properties.deviceName, " {", curr_score.second, '}');
				best = info;
				best_queues = queues;
				score = curr_score.first;
			} else {
				logs::debugln("physical device selector", "Physical device not selected: ", info.properties.deviceName, " {", curr_score.second, '}');
			}
		}
		if (!*best.handle) {
			logs::errorln("physical device selector", "No physical device was found suitable - failed to select a physical device!");
			throw std::runtime_error("physical_devices failed to select any device!");
		}
		queue_extractor(*best_queues);
		return best;
	}

#define CHECK_FEATURE(feature) if (enabled_features.feature > info.features.feature) { return {0, "feature " #feature " was not supported"}; }
	template<bool RequireSwapchainSupport, typename ...Queues>
	class physical_device_manager {
	public:
		physical_device_info physical_device;
		vk::PhysicalDeviceFeatures enabled_features;
		std::array<u32, sizeof...(Queues)> queue_family_ids;

		inline physical_device_manager() {
			if constexpr (RequireSwapchainSupport) {
				require_extension(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
			}
		}
		inline void require_extension(const char *extension) { extensions.push_back(extension); }
		inline void select_physical(const vk::raii::Instance &instance, vk::SurfaceKHR surface=nullptr) {
			physical_device = select_physical_device_with_queues<
				queue_family_index_finder<Queues...>>(instance,
				[this, &surface](const physical_device_info &info) -> std::pair<u32, std::string> {
					std::optional<std::string> res = heuristics::check_extensions(info, extensions);
					if (res.has_value()) return {0, *res};
					if constexpr (RequireSwapchainSupport) {
						res = heuristics::check_surface_format_and_present_mode(info, surface);
						if (res.has_value()) return {0, *res};
					}
					// This assumes all features are "ordered"!
					CHECK_FEATURE(robustBufferAccess)
					CHECK_FEATURE(fullDrawIndexUint32)
					CHECK_FEATURE(imageCubeArray)
					CHECK_FEATURE(independentBlend)
					CHECK_FEATURE(geometryShader)
					CHECK_FEATURE(tessellationShader)
					CHECK_FEATURE(sampleRateShading)
					CHECK_FEATURE(dualSrcBlend)
					CHECK_FEATURE(logicOp)
					CHECK_FEATURE(multiDrawIndirect)
					CHECK_FEATURE(drawIndirectFirstInstance)
					CHECK_FEATURE(depthClamp)
					CHECK_FEATURE(depthBiasClamp)
					CHECK_FEATURE(fillModeNonSolid)
					CHECK_FEATURE(depthBounds)
					CHECK_FEATURE(wideLines)
					CHECK_FEATURE(largePoints)
					CHECK_FEATURE(alphaToOne)
					CHECK_FEATURE(multiViewport)
					CHECK_FEATURE(samplerAnisotropy)
					CHECK_FEATURE(textureCompressionETC2)
					CHECK_FEATURE(textureCompressionASTC_LDR)
					CHECK_FEATURE(textureCompressionBC)
					CHECK_FEATURE(occlusionQueryPrecise)
					CHECK_FEATURE(pipelineStatisticsQuery)
					CHECK_FEATURE(vertexPipelineStoresAndAtomics)
					CHECK_FEATURE(fragmentStoresAndAtomics)
					CHECK_FEATURE(shaderTessellationAndGeometryPointSize)
					CHECK_FEATURE(shaderImageGatherExtended)
					CHECK_FEATURE(shaderStorageImageExtendedFormats)
					CHECK_FEATURE(shaderStorageImageMultisample)
					CHECK_FEATURE(shaderStorageImageReadWithoutFormat)
					CHECK_FEATURE(shaderStorageImageWriteWithoutFormat)
					CHECK_FEATURE(shaderUniformBufferArrayDynamicIndexing)
					CHECK_FEATURE(shaderSampledImageArrayDynamicIndexing)
					CHECK_FEATURE(shaderStorageBufferArrayDynamicIndexing)
					CHECK_FEATURE(shaderStorageImageArrayDynamicIndexing)
					CHECK_FEATURE(shaderClipDistance)
					CHECK_FEATURE(shaderCullDistance)
					CHECK_FEATURE(shaderFloat64)
					CHECK_FEATURE(shaderInt64)
					CHECK_FEATURE(shaderInt16)
					CHECK_FEATURE(shaderResourceResidency)
					CHECK_FEATURE(shaderResourceMinLod)
					CHECK_FEATURE(sparseBinding)
					CHECK_FEATURE(sparseResidencyBuffer)
					CHECK_FEATURE(sparseResidencyImage2D)
					CHECK_FEATURE(sparseResidencyImage3D)
					CHECK_FEATURE(sparseResidency2Samples)
					CHECK_FEATURE(sparseResidency4Samples)
					CHECK_FEATURE(sparseResidency8Samples)
					CHECK_FEATURE(sparseResidency16Samples)
					CHECK_FEATURE(sparseResidencyAliased)
					CHECK_FEATURE(variableMultisampleRate)
					CHECK_FEATURE(inheritedQueries)
					return heuristics::preference_by_type(info);
				},
				surface,
				[this](const auto &q) {
					for (usize i = 0; i < sizeof...(Queues); i++) {
						queue_family_ids[i] = *q.indices[i];
					}
				});
		}
		vk::raii::Device logical_simple() const {
			std::vector<vk::DeviceQueueCreateInfo> queues;
			for (const u32 &queue_family_id : std::set<u32>(queue_family_ids.begin(), queue_family_ids.end())) {
				static const float one = 1.f;
				queues.push_back({{}, queue_family_id, 1, &one});
			}
			return vk::raii::Device(physical_device.handle, {{}, queues, {}, extensions, &enabled_features});
		}
	private:
		std::vector<const char *> extensions;
	};
#undef CHECK_FEATURE
	template<typename ...Queues> using physical_device_manager_base = physical_device_manager<false, Queues...>;
	template<typename ...Queues> using physical_device_manager_swap = physical_device_manager<true, Queues...>;

	class autocommand : public vk::raii::CommandBuffer {
	public:
		autocommand(const vk::raii::Device &device, vk::CommandPool command_pool, vk::Queue queue);
		~autocommand();
	private:
		const vk::raii::Device &device;
		vk::Queue queue;
		vk::raii::Fence fence;
	};
}
