#include "pipeline.hpp"

#include <algorithm>
#include <fstream>
#include <stdexcept>
#include "../log/log.hpp"

namespace rend {
	std::vector<u32> load_file(const std::string &path) {
		std::ifstream f(path);
		if (!f.good()) {
			logs::errorln("Unable to open file `", path, '`');
			throw std::runtime_error("failed to open file");
		}
		std::vector<u32> out;
		f.seekg(0, std::ios::end);   
		out.resize((usize(f.tellg())+3)/4);
		f.seekg(0, std::ios::beg);
		std::copy(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>(), reinterpret_cast<char *>(out.data()));
		return out;
	}
	vk::raii::ShaderModule load_shader_from_file(const vk::raii::Device &device, const std::string &path) {
		const std::vector<u32> code = load_file(path);
		return device.createShaderModule({{}, code});
	}
	std::vector<vk::raii::ShaderModule> load_shaders_from_file(const vk::raii::Device &device, const std::initializer_list<const std::string> &paths) {
		std::vector<vk::raii::ShaderModule> out;
		out.reserve(paths.size());
		for (const std::string &p : paths) {
			out.push_back(load_shader_from_file(device, p));
		}
		return out;
	}

	graphics_pipeline_create_maker::graphics_pipeline_create_maker() : input_assembly_state({}, vk::PrimitiveTopology::eTriangleList, false), viewport_state({}, 1, {}, 1, {}),
		rasterization_state({}, false, false, vk::PolygonMode::eFill, vk::CullModeFlagBits::eBack, vk::FrontFace::eCounterClockwise, false, 0.f, 0.f, 0.f, 1.f),
		multisample_state({}, vk::SampleCountFlagBits::e1), blend_state({}, false, {}, {}, {}), dynamic_states({ vk::DynamicState::eViewport, vk::DynamicState::eScissor }) {
		blend_attachments.push_back({false, {}, {}, {}, {}, {}, {}, vk::ColorComponentFlagBits::eA | vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB});
	}
	graphics_pipeline_create_maker &graphics_pipeline_create_maker::set_shaders_gl(const std::initializer_list<const vk::ShaderStageFlagBits> &stages, const std::initializer_list<vk::ShaderModule> &modules) {
		shader_stages.clear();
		for (usize i = 0; i < stages.size(); i++) {
			shader_stages.push_back(vk::PipelineShaderStageCreateInfo{{}, *(stages.begin()+i), *(modules.begin()+i), "main"});
		}
		return *this;
	}
	graphics_pipeline_create_maker &graphics_pipeline_create_maker::add_vertex_input_binding(const u32 stride, const vk::VertexInputRate input_rate) {
		vertex_input_bindings.push_back({u32(vertex_input_bindings.size()), stride, input_rate});
		return *this;
	}
	graphics_pipeline_create_maker &graphics_pipeline_create_maker::add_vertex_input_attribute(const u32 location, const vk::Format format, const u32 offset) {
		vertex_input_attributes.push_back({location, u32(vertex_input_bindings.size()), format, offset});
		return *this;
	}
	graphics_pipeline_create_maker &graphics_pipeline_create_maker::set_attachment_basic_blending() {
		blend_attachments.back().setBlendEnable(true).setColorBlendOp(vk::BlendOp::eAdd).setAlphaBlendOp(vk::BlendOp::eAdd).
			setSrcColorBlendFactor(vk::BlendFactor::eSrcAlpha).setDstColorBlendFactor(vk::BlendFactor::eOneMinusSrcAlpha).
			setSrcAlphaBlendFactor(vk::BlendFactor::eOne).setDstAlphaBlendFactor(vk::BlendFactor::eZero);
		return *this;
	}
	constexpr graphics_pipeline_create_maker &graphics_pipeline_create_maker::with_depth_test(const vk::CompareOp compare_op) {
		depth_stencil_state->setDepthTestEnable(true).setDepthWriteEnable(true).setDepthCompareOp(compare_op);
		return *this;
	}
	vk::GraphicsPipelineCreateInfo graphics_pipeline_create_maker::get(vk::PipelineLayout layout, vk::RenderPass render_pass, u32 subpass, vk::Pipeline base, i32 base_index) {
		vertex_input_state = {{}, vertex_input_bindings, vertex_input_attributes};
		blend_state.setAttachments(blend_attachments);
		dynamic_state = {{}, dynamic_states};
		return vk::GraphicsPipelineCreateInfo{ {}, shader_stages, &vertex_input_state, &input_assembly_state, tessellation_state ? &*tessellation_state : nullptr,
			&viewport_state, &rasterization_state, &multisample_state, depth_stencil_state ? &*depth_stencil_state : nullptr, &blend_state, &dynamic_state,
			layout, render_pass, subpass, base, base_index };
	}
}
