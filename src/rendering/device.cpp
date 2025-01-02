#include "device.hpp"

namespace rend {
	physical_device_info::physical_device_info() : handle(nullptr) { }
	physical_device_info::physical_device_info(vk::raii::PhysicalDevice &&handle) : handle(handle), properties(this->handle.getProperties()), features(this->handle.getFeatures()) { }
	namespace queue_families {
		bool graphics::operator()(const u32 id, vk::PhysicalDevice physical_device, const vk::QueueFamilyProperties &queue_family, vk::SurfaceKHR surface) const {
			(void)id; (void)physical_device; (void)surface;
			return bool(queue_family.queueFlags & vk::QueueFlagBits::eGraphics);
		}
		bool present::operator()(const u32 id, vk::PhysicalDevice physical_device, const vk::QueueFamilyProperties &queue_family, vk::SurfaceKHR surface) const {
			(void)queue_family;
			return physical_device.getSurfaceSupportKHR(id, surface);
		}
		bool graphics_compute::operator()(const u32 id, vk::PhysicalDevice physical_device, const vk::QueueFamilyProperties &queue_family, vk::SurfaceKHR surface) const {
			(void)id; (void)physical_device; (void)surface;
			return (queue_family.queueFlags & vk::QueueFlagBits::eGraphics) && (queue_family.queueFlags & vk::QueueFlagBits::eCompute);
		}
		bool compute::operator()(const u32 id, vk::PhysicalDevice physical_device, const vk::QueueFamilyProperties &queue_family, vk::SurfaceKHR surface) const {
			(void)id; (void)physical_device; (void)surface;
			return bool(queue_family.queueFlags & vk::QueueFlagBits::eCompute);
		}
	}
	namespace heuristics {
		std::optional<std::string> check_extensions(const physical_device_info &info, const std::vector<const char *> &extensions) {
				const std::vector<vk::ExtensionProperties> avaliable_extensions = info.handle.enumerateDeviceExtensionProperties();
				std::vector<const char *> missing_extensions;
				std::ranges::copy_if(extensions, std::back_inserter(missing_extensions), [&avaliable_extensions](const char *extension) {
					return !std::ranges::any_of(avaliable_extensions, [&extension](const vk::ExtensionProperties i) { return std::string(i.extensionName) == std::string(extension); }); });
				if (missing_extensions.empty()) {
					return std::nullopt;
				} else {
					std::string missing_str;
					for (const char *extension : missing_extensions) {
						if (!missing_str.empty()) {
							missing_str.append(", ");
						}
						missing_str.append(extension);
					}
					return "extensions required but not supported by the device: [ " + missing_str + " ]";
				}
		}
		std::optional<std::string> check_surface_format_and_present_mode(const physical_device_info &info, vk::SurfaceKHR surface) {
			const std::vector<vk::SurfaceFormatKHR> surface_formats = info.handle.getSurfaceFormatsKHR(surface);
			const std::vector<vk::PresentModeKHR> present_modes = info.handle.getSurfacePresentModesKHR(surface);
			if (surface_formats.empty()) {
				return "no surface format found";
			} else if (present_modes.empty()) {
				return "no present mode found";
			} else {
				return std::nullopt;
			}
		}
		std::pair<u32, std::string> preference_by_type(const physical_device_info &info) {
			switch (info.properties.deviceType) {
			case vk::PhysicalDeviceType::eDiscreteGpu: return {5,"discrete gpu"};
			case vk::PhysicalDeviceType::eIntegratedGpu: return {4,"integrated gpu"};
			case vk::PhysicalDeviceType::eVirtualGpu: return {3,"virtual gpu"};
			case vk::PhysicalDeviceType::eCpu: return {2,"cpu"};
			case vk::PhysicalDeviceType::eOther:
			default: return {1,"unknown device type"};
			}
		}
	}
	autocommand::autocommand(const vk::raii::Device &device, vk::CommandPool command_pool, vk::Queue queue) : CommandBuffer(std::move(device.allocateCommandBuffers({ command_pool, vk::CommandBufferLevel::ePrimary, 1 })[0])),
		device(device), queue(queue), fence(device.createFence({})) {
		begin(vk::CommandBufferBeginInfo{});
	}
	autocommand::~autocommand() {
		end();
		queue.submit({ vk::SubmitInfo{{}, {}, **this, {}} }, *fence);
		(void)device.waitForFences(*fence, true, std::numeric_limits<u64>::max());
	}
}
