#include "context.hpp"

#include <algorithm>
#include <stdexcept>
#include "../cmake_defs.hpp"
#include "../log/log.hpp"

namespace rend {
	u32 context::glfw_context_counter = 0;

	namer::namer(const context &ctx, vk::Device device) : ctx(ctx), device(device) { }
	void namer::set_device(vk::Device device) { this->device = device; }

	context::context() : vk_instance(nullptr), vk_debug_utils_messenger(nullptr) {
		if (!glfw_context_counter++) {
			glfwSetErrorCallback([](int code, const char *msg) {
				logs::errorln("glfw3", "\x1b[100m#", code, "\x1b[0m ", msg);
			});
			glfwInit();
			glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
			u32 glfw_vulkan_extension_count;
			const char **glfw_vulkan_extensions = glfwGetRequiredInstanceExtensions(&glfw_vulkan_extension_count);
			enabled_vulkan_extensions.insert(enabled_vulkan_extensions.end(), glfw_vulkan_extensions, glfw_vulkan_extensions + glfw_vulkan_extension_count);
		}
	}
	context::~context() {
		if (!--glfw_context_counter) {
			glfwTerminate();
		}
	}
	void context::require_vulkan_extension(const char *extension) { enabled_vulkan_extensions.push_back(extension); }
	void context::require_vulkan_layer(const char *layer) { enabled_vulkan_layers.push_back(layer); }
	void context::init_vulkan_instance(const char *app_name, u32 app_ver) {
		if constexpr (compile_options::vulkan_validation) {
			logs::infoln("rendering context", "Using vulkan validation");
			require_vulkan_layer("VK_LAYER_KHRONOS_validation");
			require_vulkan_extension(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
		}

		logs::infoln("rendering context", "Initializing vulkan v", VK_API_VERSION_MAJOR(vk_api_version), '.', VK_API_VERSION_MINOR(vk_api_version), '.',
			VK_API_VERSION_PATCH(vk_api_version), '.', VK_API_VERSION_VARIANT(vk_api_version));
		const vk::ApplicationInfo app_info(app_name, app_ver, "Constantan", version::combined, vk_api_version);
		const std::vector<vk::ExtensionProperties> avaliable_extensions = vk_ctx.enumerateInstanceExtensionProperties();
		std::vector<const char *> missing_extensions;
		std::ranges::copy_if(enabled_vulkan_extensions, std::back_inserter(missing_extensions), [&avaliable_extensions](const char *extension) {
			return !std::ranges::any_of(avaliable_extensions, [&extension](const vk::ExtensionProperties i) { return std::string(i.extensionName) == std::string(extension); }); });
		const std::vector<vk::LayerProperties> avaliable_layers = vk_ctx.enumerateInstanceLayerProperties();
		std::vector<const char *> missing_layers;
		std::ranges::copy_if(enabled_vulkan_layers, std::back_inserter(missing_layers), [&avaliable_layers](const char *layer) {
			return !std::ranges::any_of(avaliable_layers, [&layer](const vk::LayerProperties i) { return std::string(i.layerName) == std::string(layer); }); });
		if (!missing_extensions.empty()) {
			logs::errorln("rendering context", "Couldn't find required vulkan instance extensions: ", missing_extensions);
		}
		if (!missing_layers.empty()) {
			logs::errorln("rendering context", "Couldn't find required vulkan instance layers: ", missing_layers);
		}
		if (!missing_extensions.empty() || !missing_layers.empty()) {
			throw std::runtime_error("required vulkan instance extensions or layers not found");
		}
		vk_instance = vk_ctx.createInstance({ {}, &app_info, enabled_vulkan_layers, enabled_vulkan_extensions });

		dynamic_dispatcher.init(vkGetInstanceProcAddr);
		dynamic_dispatcher.init(*vk_instance);

		if constexpr (compile_options::vulkan_validation) {
			constexpr auto severity_all = vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose | vk::DebugUtilsMessageSeverityFlagBitsEXT::eInfo |
				vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning | vk::DebugUtilsMessageSeverityFlagBitsEXT::eError;
			constexpr auto type_all = vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral | vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance |
				vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation | vk::DebugUtilsMessageTypeFlagBitsEXT::eDeviceAddressBinding;
			vk_debug_utils_messenger = vk_instance.createDebugUtilsMessengerEXT({ {}, severity_all, type_all, [](VkDebugUtilsMessageSeverityFlagBitsEXT severity,
				VkDebugUtilsMessageTypeFlagsEXT type, const VkDebugUtilsMessengerCallbackDataEXT *data, void *user_data) -> VkBool32 { (void)user_data;
				std::ostringstream msg;
				bool isfirsttype = true;
				if (type & VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT) { msg << (isfirsttype ? "general" : "-general"); isfirsttype = false; }
				if (type & VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT) { msg << (isfirsttype ? "validation" : "-validation"); isfirsttype = false; }
				if (type & VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT) { msg << (isfirsttype ? "performance" : "-performance"); isfirsttype = false; }
				if constexpr (compile_options::vulkan_validation_supress_loader_info) {
					if (data->messageIdNumber == 0 && severity < VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
						return VK_FALSE;
				}
				msg << " 0x" << std::hex << data->messageIdNumber << std::dec << ' ' <<
					data->pMessageIdName << "\n";
				const std::string pmsgs = data->pMessage;
				auto fbp = pmsgs.find('|');
				auto sbp = fbp == std::string::npos ? std::string::npos : pmsgs.find('|', fbp);
				if (sbp == std::string::npos) { msg << pmsgs; }
				else { msg << pmsgs.substr(sbp); }
				if (data->queueLabelCount) {
					msg << "\nqueue labels:";
					for (size_t i = 0; i < data->queueLabelCount; i++) {
						msg << "\n\t[" << data->pQueueLabels[i].pLabelName << "]";
					}
				}
				if (data->cmdBufLabelCount) {
					msg << "\ncommand buffer labels:";
					for (size_t i = 0; i < data->cmdBufLabelCount; i++) {
						msg << "\n\t[" << data->pCmdBufLabels[i].pLabelName << "]";
					}
				}
				if (data->objectCount) {
					for (size_t i = 0; i < data->objectCount; i++) {
						msg << "\nobj " << i << " at 0x" << std::hex << data->pObjects[i].objectHandle << std::dec <<
							": " << vk::to_string(static_cast<vk::ObjectType>(data->pObjects[i].objectType));
						if (data->pObjects[i].pObjectName) {
							msg << " [" << data->pObjects[i].pObjectName << "]";
						}
					}
				}//*/
				if (severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) { logs::errorln("vulkan vaidation", msg.str()); }
				else if (severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) { logs::warnln("vulkan vaidation", msg.str()); }
				else if (severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT) { logs::infoln("vulkan vaidation", msg.str()); }
				else { logs::debugln("vulkan vaidation", msg.str()); }
				if constexpr (compile_options::vulkan_validation_abort) {
					if (severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
						throw std::runtime_error("validation error");
					}
				}
				return VK_FALSE;
			} }, nullptr);
		}
	}
	window context::make_window(u32 width, u32 height, const char *title) { return window(*this, width, height, title); }
	namer context::get_namer(vk::Device device) const { return namer(*this, device); }
}
