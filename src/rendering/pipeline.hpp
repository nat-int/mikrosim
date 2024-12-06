#pragma once

#include <initializer_list>
#include <optional>
#include <string>
#include <vector>
#include "../vulkan_glfw_pch.hpp"
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include "../defs.hpp"

namespace rend {
	std::vector<u32> load_file(const std::string &path);
	vk::raii::ShaderModule load_shader_from_file(const vk::raii::Device &device, const std::string &path);
	std::vector<vk::raii::ShaderModule> load_shaders_from_file(const vk::raii::Device &device, const std::initializer_list<const std::string> &paths);

	class graphics_pipeline_create_maker {
	public:
		std::vector<vk::PipelineShaderStageCreateInfo> shader_stages;
		std::vector<vk::VertexInputBindingDescription> vertex_input_bindings;
		std::vector<vk::VertexInputAttributeDescription> vertex_input_attributes;
		vk::PipelineInputAssemblyStateCreateInfo input_assembly_state;
		std::optional<vk::PipelineTessellationStateCreateInfo> tessellation_state;
		vk::PipelineViewportStateCreateInfo viewport_state;
		vk::PipelineRasterizationStateCreateInfo rasterization_state;
		vk::PipelineMultisampleStateCreateInfo multisample_state;
		std::optional<vk::PipelineDepthStencilStateCreateInfo> depth_stencil_state;
		std::vector<vk::PipelineColorBlendAttachmentState> blend_attachments;
		vk::PipelineColorBlendStateCreateInfo blend_state;
		std::vector<vk::DynamicState> dynamic_states;

		graphics_pipeline_create_maker();
		graphics_pipeline_create_maker &set_shaders_gl(const std::initializer_list<const vk::ShaderStageFlagBits> &stages, const std::initializer_list<vk::ShaderModule> &modules);
		graphics_pipeline_create_maker &add_vertex_input_binding(const u32 stride, const vk::VertexInputRate input_rate);
		/// adds vertex input attribute for the *next* binding
		graphics_pipeline_create_maker &add_vertex_input_attribute(const u32 location, const vk::Format format, const u32 offset=0);
		graphics_pipeline_create_maker &set_attachment_basic_blending();
		constexpr graphics_pipeline_create_maker &with_depth_test(const vk::CompareOp compare_op);
		vk::GraphicsPipelineCreateInfo get(vk::PipelineLayout layout, vk::RenderPass render_pass, const u32 subpass=0, vk::Pipeline parent=nullptr, const i32 parent_index=0);
	private:
		vk::PipelineVertexInputStateCreateInfo vertex_input_state;
		vk::PipelineDynamicStateCreateInfo dynamic_state;
	};
	template<typename T> struct vglm {};
	template<> struct vglm<f32> { constexpr static vk::Format format = vk::Format::eR32Sfloat; };
	template<> struct vglm<glm::vec2> { constexpr static vk::Format format = vk::Format::eR32G32Sfloat; };
	template<> struct vglm<glm::vec3> { constexpr static vk::Format format = vk::Format::eR32G32B32Sfloat; };
	template<> struct vglm<glm::vec4> { constexpr static vk::Format format = vk::Format::eR32G32B32A32Sfloat; };
}
