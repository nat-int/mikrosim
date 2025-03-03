#pragma once

#include <array>
#include <glm/glm.hpp>
#include "../vulkan_glfw_pch.hpp"
#include "vma_wrap.hpp"
#include "../util.hpp"

namespace rend {
	class context;
	struct vertex2d {
		glm::vec2 position;
		glm::vec2 uv;
		glm::vec4 color;
	};
	std::vector<vertex2d> quad6v2d(glm::vec2 a, glm::vec2 b, const glm::vec4 &col, glm::vec2 uva={0.f,0.f}, glm::vec2 uvb={1.f,1.f});
	void quad6v2d(std::vector<vertex2d> &out, glm::vec2 a, glm::vec2 b, const glm::vec4 &col, glm::vec2 uva={0.f,0.f}, glm::vec2 uvb={1.f,1.f});
	enum class anchor2d {
		center,
		top, topright, right, bottomright, bottom, bottomleft, left, topleft,
	};
	class renderer2d {
	public:
		renderer2d(const context &ctx, const vk::raii::Device &device, vk::RenderPass render_pass, u32 width, u32 height);
		void push_projection(vk::CommandBuffer cmds, const glm::mat4 &proj) const;
		void push_textured_projection(vk::CommandBuffer cmds, const glm::mat4 &proj) const;
		void bind_texture(vk::CommandBuffer cmds, vk::DescriptorSet ds) const;
		void bind_solid_pipeline(vk::CommandBuffer cmds) const;
		void bind_circle_pipeline(vk::CommandBuffer cmds) const;
		void bind_textured_pipeline(vk::CommandBuffer cmds) const;
		glm::mat4 proj(anchor2d a) const;
		void resize(f32 width, f32 height);
		template<size_t N> inline std::vector<vk::raii::DescriptorSet> make_texture_ds(vk::DescriptorPool pool) const {
			const auto dsls = array_init<vk::DescriptorSetLayout, N>(*texture_dsl);
			return device.allocateDescriptorSets(vk::DescriptorSetAllocateInfo{pool, dsls});
		}
		template<typename IT> inline void write_texture_ds(std::vector<vk::WriteDescriptorSet> &writes, IT begin, IT end, const vk::DescriptorImageInfo *dii) const {
			for (; begin != end; begin++) {
				writes.push_back(vk::WriteDescriptorSet{*begin, 0, 0, 0, 1, vk::DescriptorType::eCombinedImageSampler, dii, nullptr, nullptr});
			}
		}
		template<typename IT> inline std::vector<vk::WriteDescriptorSet> write_texture_ds(IT begin, IT end, const vk::DescriptorImageInfo *dii) const {
			std::vector<vk::WriteDescriptorSet> writes;
			write_texture_ds(writes, begin, end, dii);
			return writes;
		}
		template<typename IT> inline void write_texture_ds_raii(std::vector<vk::WriteDescriptorSet> &writes, IT begin, IT end, const vk::DescriptorImageInfo *dii) const {
			for (; begin != end; begin++) {
				writes.push_back({**begin, 0, 0, 1, vk::DescriptorType::eCombinedImageSampler, dii, nullptr, nullptr});
			}
		}
		template<typename IT> inline std::vector<vk::WriteDescriptorSet> write_texture_ds_raii(IT begin, IT end, const vk::DescriptorImageInfo *dii) const {
			std::vector<vk::WriteDescriptorSet> writes;
			write_texture_ds_raii(writes, begin, end, dii);
			return writes;
		}
		const context &get_ctx() const;
		const vk::raii::Device &get_device() const;
		vk::Sampler get_linear_sampler() const;
		vk::Sampler get_nearest_sampler() const;
	private:
		const context &ctx;
		const vk::raii::Device &device;
		vk::DescriptorPool descriptor_pool;
		vk::raii::DescriptorSetLayout texture_dsl;
		vk::raii::PipelineLayout solid_pipeline_layout;
		vk::raii::PipelineLayout textured_pipeline_layout;
		std::vector<vk::raii::ShaderModule> shaders;
		vk::raii::Pipeline solid_pipeline;
		vk::raii::Pipeline circle_pipeline;
		vk::raii::Pipeline textured_pipeline;
		vk::raii::Sampler linear_sampler;
		vk::raii::Sampler nearest_sampler;
		std::array<glm::mat4, 9> projs;
	};
	std::ostream &operator<<(std::ostream &os, const vertex2d &v);
}
