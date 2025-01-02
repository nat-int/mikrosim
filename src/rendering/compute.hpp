#pragma once

#include <vector>
#include <initializer_list>
#include "../vulkan_glfw_pch.hpp"
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include "../defs.hpp"
#include "buffers.hpp"
#include "pipeline.hpp"

namespace rend {
	template<usize Frames>
	class basic_compute_process {
	public:
		const vk::raii::Device &device;
		const buffer_handler &bh;
		std::array<vma::buffer, Frames> main_buffer;
		std::vector<vma::buffer> proc_buffers;
		std::vector<usize> buffer_sizes;
		std::vector<basic_compute_pipeline> pipelines;
		usize frame;

		inline basic_compute_process(const vk::raii::Device &device, const buffer_handler &bh) :
			device(device), bh(bh), main_buffer(array_init<vma::buffer, Frames>(nullptr)), frame(0) {
			buffer_sizes.push_back(0);
		}
		template<typename T>
		inline void set_main_buffer(const std::vector<T> &data, vk::BufferUsageFlags usage_mixin={}) {
			for (usize i = 0; i < Frames; i++) {
				main_buffer[i] = bh.make_device_buffer_dedicated_stage(data,
					vk::BufferUsageFlagBits::eStorageBuffer | usage_mixin);
			}
			buffer_sizes[0] = sizeof(T) * data.size();
		}
		inline usize add_buffer(const usize bytes, vk::BufferUsageFlags usage_mixin={}) {
			proc_buffers.push_back(bh.get_alloc().create_buffer({{}, bytes,
				vk::BufferUsageFlagBits::eStorageBuffer | usage_mixin, vk::SharingMode::eExclusive, {}},
				{{}, vma::memory_usage::eAuto}));
			buffer_sizes.push_back(bytes);
			return proc_buffers.size();
		}
		template<typename PC>
		inline void add_main_buffer_pipeline(const vk::raii::DescriptorPool &dpool, const std::string &shf,
			const std::initializer_list<usize> &proc_buffs, u32 main_buff_count) {
			pipelines.emplace_back();
			pipelines.back().create<PC>(device, shf,
				std::vector<std::pair<vk::DescriptorType, u32>>(main_buff_count+proc_buffs.size(),
					{vk::DescriptorType::eStorageBuffer, 1}));
			std::vector<vk::DescriptorSetLayout> dsls(Frames, pipelines.back().dsl);
			pipelines.back().ds = device.allocateDescriptorSets({dpool, dsls});
			std::vector<vk::WriteDescriptorSet> writes;
			writes.reserve(Frames*(main_buff_count+proc_buffs.size()));
			std::vector<vk::DescriptorBufferInfo> dbis;
			dbis.reserve(Frames*(main_buff_count+proc_buffs.size()));
			for (u32 f = 0; f < Frames; f++) {
				for (u32 i = 0; i < main_buff_count; i++) {
					dbis.push_back({*main_buffer[(f+i)%Frames], 0, buffer_sizes[0]});
					writes.push_back({pipelines.back().ds[f], i, 0,
						vk::DescriptorType::eStorageBuffer, {}, dbis.back(), {}});
				}
				for (u32 i = 0; i < proc_buffs.size(); i++) {
					usize buff = proc_buffs.begin()[i];
					dbis.push_back({*proc_buffers[buff-1], 0, buffer_sizes[buff]});
					writes.push_back({pipelines.back().ds[f], main_buff_count+i, 0,
						vk::DescriptorType::eStorageBuffer, {}, dbis.back(), {}});
				}
			}
			device.updateDescriptorSets(writes, {});
		}
		template<typename PC>
		inline void add_proc_buffer_pipeline(const vk::raii::DescriptorPool &dpool, const std::string &shf,
			const std::initializer_list<usize> &buffs) {
			pipelines.emplace_back();
			pipelines.back().create<PC>(device, shf,
				std::vector<std::pair<vk::DescriptorType, u32>>(buffs.size(),
					{vk::DescriptorType::eStorageBuffer, 1}));
			pipelines.back().ds.push_back(std::move(device.allocateDescriptorSets(
				{dpool, {*pipelines.back().dsl}}).front()));
			std::vector<vk::WriteDescriptorSet> writes; writes.reserve(buffs.size());
			std::vector<vk::DescriptorBufferInfo> dbis; dbis.reserve(buffs.size());
			for (u32 i = 0; i < buffs.size(); i++) {
				usize buff = buffs.begin()[i];
				dbis.push_back({*proc_buffers[buff-1], 0, buffer_sizes[buff]});
				writes.push_back({*pipelines.back().ds[0], i, 0, vk::DescriptorType::eStorageBuffer, {},
					dbis.back(), {}});
			}
			device.updateDescriptorSets(writes, {});
		}

		struct raii_process {
			basic_compute_process<Frames> *proc;
			vk::CommandBuffer cmd;
			usize frame_id;

			inline basic_compute_pipeline &pl(usize plid) { return proc->pipelines[plid]; }
			inline vma::buffer &pbuff(usize buffid) { return proc->proc_buffers[buffid-1]; }
			inline vma::buffer &mbuff(usize frame_offset=0) { return proc->main_buffer[frame(frame_offset)]; }
			inline void fill_proc_buffer(usize buffid, u32 value=0) {
				cmd.fillBuffer(*pbuff(buffid), 0, proc->buffer_sizes[buffid], value);
			}
			inline void barrier_main_buffer(barrier &b, vk::AccessFlags from, vk::AccessFlags to,
				const std::vector<usize> &frame_offsets={0}) {
				for (usize off : frame_offsets) {
					b.buff(from, to, *mbuff(off), proc->buffer_sizes[0]);
				}
			}
			inline void barrier_proc_buffers(barrier &b, vk::AccessFlags from, vk::AccessFlags to,
				const std::vector<usize> &buffs) {
				for (usize buff : buffs) {
					b.buff(from, to, *pbuff(buff), proc->buffer_sizes[buff]);
				}
			}
			template<typename PC>
			inline void bind_dispatch(usize pipeline_id, const PC &push, glm::uvec3 kernel) {
				basic_compute_pipeline &p = pl(pipeline_id);
				p.bind_dispatch(cmd, push, {p.ds[frame()%p.ds.size()]}, kernel.x, kernel.y, kernel.z);
			}
			template<typename PC>
			inline void push_dispatch(usize pipeline_id, const PC &push, glm::uvec3 kernel) {
				basic_compute_pipeline &p = pl(pipeline_id);
				p.push_constant(cmd, push);
				p.dispatch(cmd, kernel.x, kernel.y, kernel.z);
			}
			inline void bind_pipeline(usize pipeline_id) {
				basic_compute_pipeline &p = pl(pipeline_id);
				p.bind(cmd);
				p.bind_ds(cmd, {p.ds[frame()%p.ds.size()]});
			}
			inline usize frame(usize offset=0) { return (frame_id + offset) % Frames; }
		};
		inline raii_process start(vk::CommandBuffer cmd) {
			usize f = frame;
			frame = (frame + 1) % Frames;
			return raii_process{this, cmd, f};
		}
	};
}

