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
#include "buffers.hpp"

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
	template<> struct vglm<u32> { constexpr static vk::Format format = vk::Format::eR32Uint; };
	template<> struct vglm<glm::uvec2> { constexpr static vk::Format format = vk::Format::eR32G32Uint; };
	template<> struct vglm<glm::uvec3> { constexpr static vk::Format format = vk::Format::eR32G32B32Uint; };
	template<> struct vglm<glm::uvec4> { constexpr static vk::Format format = vk::Format::eR32G32B32A32Uint; };

	class basic_compute_pipeline {
	public:
		vk::raii::ShaderModule shader;
		vk::raii::DescriptorSetLayout dsl;
		vk::raii::PipelineLayout layout;
		vk::raii::Pipeline pipeline;
		std::vector<vk::raii::DescriptorSet> ds;

		basic_compute_pipeline();
		template<typename PC>
		inline void create(const vk::raii::Device &device, const std::string &shf,
			const std::vector<std::pair<vk::DescriptorType, u32>> &bindings) {
			shader = load_shader_from_file(device, shf);
			std::vector<vk::DescriptorSetLayoutBinding> _bindings(bindings.size());
			for (u32 i = 0; i < bindings.size(); i++) {
				const auto b = bindings[i];
				_bindings[i] = {i, b.first, b.second, vk::ShaderStageFlagBits::eCompute};
			}
			dsl = device.createDescriptorSetLayout({{}, _bindings});
			if constexpr (std::is_same<PC, void>::value) {
				layout = device.createPipelineLayout({{}, *dsl, {}});
			} else {
				vk::PushConstantRange pcr{vk::ShaderStageFlagBits::eCompute, 0, sizeof(PC)};
				layout = device.createPipelineLayout({{}, *dsl, pcr});
			}
			pipeline = device.createComputePipeline(nullptr, {{}, {{}, vk::ShaderStageFlagBits::eCompute, shader, "main"}, layout});
		}
		void bind(vk::CommandBuffer cmd) const;
		template<typename PC>
		inline void push_constant(vk::CommandBuffer cmd, const PC &constant) const {
			cmd.pushConstants(layout, vk::ShaderStageFlagBits::eCompute, 0, sizeof(PC), &constant);
		}
		void bind_ds(vk::CommandBuffer cmd, const std::vector<vk::DescriptorSet> &ds) const;
		void dispatch(vk::CommandBuffer cmd, u32 sx, u32 sy, u32 sz) const;
		template<typename PC>
		inline void bind_dispatch(vk::CommandBuffer cmd, const PC &constant,
			const std::initializer_list<vk::DescriptorSet> &ds, u32 sx, u32 sy, u32 sz) const {
			bind(cmd);
			push_constant(cmd, constant);
			bind_ds(cmd, ds);
			dispatch(cmd, sx, sy, sz);
		}
		/// the buffers array is {buff_arr, buffer size, cycle offset}
		template<usize N>
		inline std::vector<vk::raii::DescriptorSet> make_buffer_cycle_ds(const vk::raii::Device &device,
			const vk::raii::DescriptorPool &dpool,
			const std::vector<std::tuple<const std::array<vma::buffer, N> &, u32, u32>> &buffers) const {
			std::vector<vk::DescriptorSetLayout> dsls(N, dsl);
			std::vector<vk::raii::DescriptorSet> out = device.allocateDescriptorSets({dpool, dsls});
			std::vector<vk::WriteDescriptorSet> writes;
			writes.reserve(N * buffers.size());
			std::vector<vk::DescriptorBufferInfo> dbis;
			dbis.reserve(N * buffers.size());
			u32 bi = 0;
			for (const auto &[buff_arr, size, cycle_offset] : buffers) {
				for (usize i = 0; i < N; i++) {
					dbis.push_back({*buff_arr[(i+cycle_offset)%N], 0, size});
					writes.push_back({out[i], bi, 0, vk::DescriptorType::eStorageBuffer, {}, dbis.back(), {}});
				}
				bi++;
			}
			device.updateDescriptorSets(writes, {});
			return out;
		}
	};

	struct barrier {
		constexpr static vk::AccessFlags shader_r = vk::AccessFlagBits::eShaderRead;
		constexpr static vk::AccessFlags shader_w = vk::AccessFlagBits::eShaderWrite;
		constexpr static vk::AccessFlags shader_rw =
			vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite;
		constexpr static vk::AccessFlags mem_r = vk::AccessFlagBits::eMemoryRead;
		constexpr static vk::AccessFlags mem_w = vk::AccessFlagBits::eMemoryWrite;
		constexpr static vk::AccessFlags trans_r = vk::AccessFlagBits::eTransferRead;
		constexpr static vk::AccessFlags trans_w = vk::AccessFlagBits::eTransferWrite;

		vk::CommandBuffer cmd;
		vk::PipelineStageFlags from;
		vk::PipelineStageFlags to;
		vk::DependencyFlags flags;
		std::vector<vk::BufferMemoryBarrier> buffer_barriers;
		std::vector<vk::ImageMemoryBarrier> image_barriers;
		vk::AccessFlags preset_from;
		vk::AccessFlags preset_to;

		barrier(vk::CommandBuffer cmd, vk::PipelineStageFlags from, vk::PipelineStageFlags to,
			vk::DependencyFlags flags={});
		~barrier();
		template<typename F> static void transfer_compute(vk::CommandBuffer cmd, F fun) {
			barrier b(cmd, vk::PipelineStageFlagBits::eTransfer,
				vk::PipelineStageFlagBits::eComputeShader);
			fun(b);
		}
		template<typename F> static void compute_transfer(vk::CommandBuffer cmd, F fun) {
			barrier b(cmd, vk::PipelineStageFlagBits::eComputeShader,
				vk::PipelineStageFlagBits::eTransfer);
			fun(b);
		}
		template<typename F> static void compute_compute(vk::CommandBuffer cmd, F fun) {
			barrier b(cmd, vk::PipelineStageFlagBits::eComputeShader,
				vk::PipelineStageFlagBits::eComputeShader);
			fun(b);
		}
		void preset(vk::AccessFlags f, vk::AccessFlags t);
		void buff(vk::AccessFlags from, vk::AccessFlags to, vk::Buffer buff,
			vk::DeviceSize size, vk::DeviceSize offset=0);
		void buff(vk::Buffer buff, vk::DeviceSize size, vk::DeviceSize offset=0);
	};
}
