#pragma once

#include <map>
#include "../util.hpp"
#include "../vulkan_glfw_pch.hpp"
#include "context.hpp"
#include "defs.hpp"
#include "vma_wrap.hpp"

namespace rend {
	struct buffer_reg { dsize size; dsize offset=0; };
	template<typename T>
	class mapped_dedicated_uniform_buffer {
	public:
		inline mapped_dedicated_uniform_buffer(const vma::allocator &allocator, vk::SharingMode sharing_mode=vk::SharingMode::eExclusive,
			const vk::ArrayProxyNoTemporaries<const u32> &queues={}) {
			handle = allocator.create_buffer({{}, sizeof(T), vk::BufferUsageFlagBits::eUniformBuffer, sharing_mode, queues},
				{ vma::allocation_create_flag_bits::eHostAccessRandom, vma::memory_usage::eAuto });
			map = static_cast<T *>(handle.map());
		}
		inline ~mapped_dedicated_uniform_buffer() { handle.unmap(); }
		inline T *get_map() const { return map; }
		inline vk::DescriptorBufferInfo descriptor_info() const { return {*handle, 0, sizeof(T)}; }
	private:
		vma::buffer handle;
		T *map;
	};
	class simple_mesh {
	public:
		simple_mesh();
		simple_mesh(vma::buffer &&vb, u32 vc);
		void set(vma::buffer &&vb, u32 vc);
		void bind_draw(vk::CommandBuffer command_buffer) const;
		void draw(vk::CommandBuffer command_buffer) const;
	private:
		vma::buffer vb;
		u32 vert_count;
	};
	class replace_buffer {
	public:
		const vma::allocator &alloc;
		std::array<vma::buffer, compile_options::frames_in_flight> old;
		vma::buffer buff;

		replace_buffer(const vma::allocator &alloc, vk::BufferUsageFlags usage, const vma::allocation_create_info &alloc_ci,
			vk::BufferCreateFlags create_flags={}, vk::SharingMode sharing_mode=vk::SharingMode::eExclusive,
			const vk::ArrayProxyNoTemporaries<const u32> &queues={});
		void update_change(usize new_size);
		void update_keep();
	private:
		vk::BufferUsageFlags usage;
		vk::BufferCreateFlags create_flags;
		vk::SharingMode sharing_mode;
		vk::ArrayProxyNoTemporaries<const u32> queues;
		vma::allocation_create_info alloc_ci;
	};
	class replace_simple_mesh : protected replace_buffer {
	public:
		using replace_buffer::replace_buffer;
		template<typename T> inline void update_change(T &stager, usize frame, const void *data, usize new_size, usize nvertc) {
			if (new_size > size || !**this)
				replace_buffer::update_change(new_size);
			else
				replace_buffer::update_keep();
			stager.upload_buffer(buff, 0, data, new_size, frame);
			size = new_size;
			vertc = nvertc;
		}
		template<typename T, typename ST> inline void update_change(ST &stager, usize frame, const T &data, usize nvertc) {
			update_change(stager, frame, data.data(), data.size() * sizeof(data[0]), nvertc);
		}
		void update_keep();
		void bind_draw(vk::CommandBuffer command_buffer) const;
		void draw(vk::CommandBuffer command_buffer) const;
		vk::Buffer get_old(usize age=0) const;
		vk::Buffer operator*() const;
		VmaAllocation operator~() const;
	private:
		usize size=0;
		usize vertc=0;
	};


	class dyn_stager {
	public:
		dyn_stager(const vma::allocator &alloc);
		virtual ~dyn_stager();
		virtual void prepare(u32 frame=0) = 0;
		virtual void upload_buffer(const vma::buffer &target, dsize at, const void *data, usize size, u32 frame=0) = 0;
		virtual void commit(vk::CommandBuffer command_buffer, u32 frame=0) = 0;
	protected:
		const vma::allocator &alloc;
	};
	class fake_stager : public dyn_stager {
	public:
		using dyn_stager::dyn_stager;
		void prepare(u32 frame=0);
		void upload_buffer(const vma::buffer &target, dsize at, const void *data, usize size, u32 frame=0);
		void commit(vk::CommandBuffer command_buffer, u32 frame=0);
	};
	class multi_stager_frame {
	public:
		void prepare(const vma::allocator &alloc);
		void upload_buffer(const vma::allocator &alloc, const vma::buffer &target, dsize at, const void *data, usize size);
		void commit(vk::CommandBuffer command_buffer);
	protected:
		constexpr static dsize start_size = 512;
		std::vector<vma::buffer> buffers;
		std::vector<std::pair<u8 *, dsize>> maps;
		std::multimap<usize, usize> free_maps;
		std::vector<std::tuple<vk::Buffer, vk::Buffer, vk::BufferCopy>> tasks;
		dsize bump_size;

		void push_buffer(const vma::allocator &alloc);
		std::tuple<usize, dsize, u8 *> prepare_upload(const vma::allocator &alloc, usize size);
	};
	class multi_stager_frame_synced : public multi_stager_frame {
	public:
		void prepare(const vma::allocator &alloc);
		void upload_buffer(const vma::allocator &alloc, const vma::buffer &target, dsize at, const void *data, usize size);
		void commit(vk::CommandBuffer command_buffer);
	private:
		rw_lock rwl;
		std::mutex mut;
	};
	class multi_stager : public dyn_stager {
	public:
		constexpr static usize framec = compile_options::frames_in_flight + 1;

		using dyn_stager::dyn_stager;
		void prepare(u32 frame=0);
		void upload_buffer(const vma::buffer &target, dsize at, const void *data, usize size, u32 frame=0);
		void commit(vk::CommandBuffer command_buffer, u32 frame=0);
	private:
		std::array<multi_stager_frame, framec> frames;
	};
	class multi_stager_synced : public dyn_stager {
	public:
		constexpr static usize framec = compile_options::frames_in_flight + 1;

		using dyn_stager::dyn_stager;
		void prepare(u32 frame=0);
		void upload_buffer(const vma::buffer &target, dsize at, const void *data, usize size, u32 frame=0);
		void commit(vk::CommandBuffer command_buffer, u32 frame=0);
	private:
		std::array<multi_stager_frame_synced, framec> frames;
	};

	class buffer_handler {
	public:
		buffer_handler(vk::PhysicalDevice physical_device, const vk::raii::Device &device, const vma::allocator &allocator,
			vk::CommandPool command_pool, vk::Queue transfer_queue);

		bool should_stage() const;
#define CONSTANAN_DEVICE_BUFFER_MAKE_ARGS vk::BufferUsageFlags usage, vk::BufferCreateFlags create_flags={},\
			vk::SharingMode sharing_mode=vk::SharingMode::eExclusive, const vk::ArrayProxyNoTemporaries<const u32> &queues={}
#define CONSTANTAN_DEVICE_BUFFER_MAKE_FROM_ARGS(usage_mixin, alloc_create_flags, alloc_type) \
			allocator.create_buffer({create_flags, size, usage usage_mixin, sharing_mode, queues}, {alloc_create_flags, alloc_type})
		vma::buffer make_device_buffer_dedicated_stage(const void *data, u32 size, CONSTANAN_DEVICE_BUFFER_MAKE_ARGS) const;
		template<typename T> inline vma::buffer make_device_buffer_dedicated_stage(const T &data, CONSTANAN_DEVICE_BUFFER_MAKE_ARGS) const {
			return make_device_buffer_dedicated_stage(data.data(), static_cast<u32>(data.size() * sizeof(data[0])), usage, create_flags, sharing_mode, queues);
		}

		template<typename T>
		vma::buffer make_device_buffer_auto(T &stager, u32 frame, const void *data, u32 size, CONSTANAN_DEVICE_BUFFER_MAKE_ARGS) const {
			if (use_staging) {
				return make_device_buffer_staged(stager, frame, data, size, usage, create_flags, sharing_mode, queues);
			} else {
				vma::buffer buff = CONSTANTAN_DEVICE_BUFFER_MAKE_FROM_ARGS(, vma::allocation_create_flag_bits::eHostAccessSequentialWrite, vma::memory_usage::eAuto);
				buff.flash(data, size);
				return buff;
			}
		}
		template<typename T>
		vma::buffer make_device_buffer_staged(T &stager, u32 frame, const void *data, u32 size, CONSTANAN_DEVICE_BUFFER_MAKE_ARGS) const {
			vma::buffer buff = CONSTANTAN_DEVICE_BUFFER_MAKE_FROM_ARGS(| vk::BufferUsageFlagBits::eTransferDst, {}, vma::memory_usage::eAutoPreferDevice);
			stager.upload_buffer(buff, 0, data, size, frame);
			return buff;
		}
#define CONSTANAN_DEVICE_BUFFER_MAKE_OVERLOAD_GEN(fname)\
		template<typename T, typename ST> inline vma::buffer fname(ST &stager, u32 frame, const T &data, CONSTANAN_DEVICE_BUFFER_MAKE_ARGS) const {\
			return fname(stager, frame, data.data(), data.size() * sizeof(data[0]), usage, create_flags, sharing_mode, queues);\
		}
		CONSTANAN_DEVICE_BUFFER_MAKE_OVERLOAD_GEN(make_device_buffer_auto)
		CONSTANAN_DEVICE_BUFFER_MAKE_OVERLOAD_GEN(make_device_buffer_staged)
#undef CONSTANAN_DEVICE_BUFFER_MAKE_OVERLOAD_GEN

		vma::image make_device_image_dedicated_stage(const void *data, u32 size, vk::ImageUsageFlagBits usage,
			const vk::Extent3D &image_extent, const vk::ImageSubresourceLayers &img_subres_layers={vk::ImageAspectFlagBits::eColor, 0, 0, 1},
			vk::Format image_format=vk::Format::eA8B8G8R8UnormPack32, vk::ImageLayout final_layout=vk::ImageLayout::eShaderReadOnlyOptimal, u32 mip_levels=1,
			u32 array_layers=1, vk::ImageCreateFlags create_flags={}, vk::SampleCountFlagBits samples=vk::SampleCountFlagBits::e1,
			vk::ImageTiling tiling=vk::ImageTiling::eOptimal, vk::ImageType image_type=vk::ImageType::e2D,
			vk::SharingMode sharing_mode=vk::SharingMode::eExclusive, const vk::ArrayProxyNoTemporaries<const u32> &queues={}) const;
		template<typename T> inline vma::image make_device_image_dedicated_stage(const T &data, vk::ImageUsageFlagBits usage,
			const vk::Extent3D &image_extent, const vk::ImageSubresourceLayers &img_subres_layers={vk::ImageAspectFlagBits::eColor, 0, 0, 1},
			vk::Format image_format=vk::Format::eA8B8G8R8UnormPack32, vk::ImageLayout final_layout=vk::ImageLayout::eShaderReadOnlyOptimal, u32 mip_levels=1,
			u32 array_layers=1, vk::ImageCreateFlags create_flags={}, vk::SampleCountFlagBits samples=vk::SampleCountFlagBits::e1,
			vk::ImageTiling tiling=vk::ImageTiling::eOptimal, vk::ImageType image_type=vk::ImageType::e2D,
			vk::SharingMode sharing_mode=vk::SharingMode::eExclusive, const vk::ArrayProxyNoTemporaries<const u32> &queues={}) const {
			return make_device_image_dedicated_stage(data.data(), data.size() * sizeof(data[0]), usage, image_extent, img_subres_layers, image_format, final_layout,
				mip_levels, array_layers, create_flags, samples, tiling, image_type, sharing_mode, queues);
		}
		const vk::raii::Device &get_device() const;
		const vma::allocator &get_alloc() const;
		multi_stager get_multi_stager() const;
		multi_stager_synced get_multi_stager_synced() const;
		dyn_stager *get_stager() const;
		dyn_stager *get_stager_synced() const;
	private:
		vk::PhysicalDevice physical_device;
		const vk::raii::Device &device;
		const vma::allocator &allocator;
		vk::CommandPool command_pool;
		vk::Queue transfer_queue;
		bool use_staging;
	};

	class texture2d {
	public:
		texture2d(vma::image &&image, const vk::raii::Device &device, vk::Format format=vk::Format::eA8B8G8R8UnormPack32,
			const vk::ImageSubresourceRange &subresource_range={vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1}, const vk::ComponentMapping &component_mapping={});
		void set_debug_names(const namer &nm, const std::string &name) const;
		vk::DescriptorImageInfo get_dii(vk::Sampler sampler=VK_NULL_HANDLE, vk::ImageLayout layout=vk::ImageLayout::eShaderReadOnlyOptimal) const;
		u32 get_width() const;
		u32 get_height() const;
	private:
		u32 width, height;
		vma::image img;
		vk::raii::ImageView iv;
	};
}

