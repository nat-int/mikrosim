#include "buffers.hpp"
#include "device.hpp"

namespace rend {
	simple_mesh::simple_mesh() : vb(nullptr), vert_count(0) { }
	simple_mesh::simple_mesh(vma::buffer &&vb, u32 vc) : vb(std::forward<vma::buffer>(vb)), vert_count(vc) { }
	void simple_mesh::set(vma::buffer &&vb, u32 vc) { this->vb = std::forward<vma::buffer>(vb); vert_count = vc; }
	void simple_mesh::bind_draw(vk::CommandBuffer command_buffer) const {
		command_buffer.bindVertexBuffers(0, { *vb }, { 0 });
		command_buffer.draw(vert_count, 1, 0, 0);
	}
	void simple_mesh::draw(vk::CommandBuffer command_buffer) const { command_buffer.draw(vert_count, 1, 0, 0); }
	replace_buffer::replace_buffer(const vma::allocator &alloc, vk::BufferUsageFlags usage, const vma::allocation_create_info &alloc_ci,
		vk::BufferCreateFlags create_flags, vk::SharingMode sharing_mode, const vk::ArrayProxyNoTemporaries<const u32> &queues) : alloc(alloc),
		old(array_init<vma::buffer, compile_options::frames_in_flight>(nullptr)), buff(nullptr), usage(usage), create_flags(create_flags),
		sharing_mode(sharing_mode), queues(queues), alloc_ci(alloc_ci) { }
	void replace_buffer::update_change(usize new_size) {
		for (u32 i = compile_options::frames_in_flight - 1; i > 0; i--) {
			old[i] = std::move(old[i-1]);
		}
		old[0] = std::move(buff);
		buff = alloc.create_buffer({create_flags, new_size, usage, sharing_mode, queues}, alloc_ci);
	}
	void replace_buffer::update_keep() {
		for (i32 i = compile_options::frames_in_flight - 1; i > 0; i--) {
			old[usize(i)] = std::move(old[usize(i-1)]);
		}
	}
	void replace_simple_mesh::update_keep() { replace_buffer::update_keep(); }
	void replace_simple_mesh::bind_draw(vk::CommandBuffer command_buffer) const {
		command_buffer.bindVertexBuffers(0, { *buff }, { 0 });
		command_buffer.draw(u32(vertc), 1, 0, 0);
	}
	void replace_simple_mesh::draw(vk::CommandBuffer command_buffer) const {
		command_buffer.draw(u32(vertc), 1, 0, 0); }
	vk::Buffer replace_simple_mesh::get_old(usize age) const { return *old[age]; }
	vk::Buffer replace_simple_mesh::operator*() const { return *buff; }
	VmaAllocation replace_simple_mesh::operator~() const { return ~buff; }

	dyn_stager::dyn_stager(const vma::allocator &alloc) : alloc(alloc) { }
	dyn_stager::~dyn_stager() { }
	void fake_stager::prepare(u32 ) { }
	void fake_stager::upload_buffer(const vma::buffer &target, dsize at, const void *data, usize size, u32) {
		void *m = target.map();
		std::memcpy(static_cast<u8 *>(m) + at, data, size);
		target.unmap();
	}
	void fake_stager::commit(vk::CommandBuffer, u32) { }
	void multi_stager_frame::prepare(const vma::allocator &alloc) {
		buffers.clear();
		maps.clear();
		free_maps.clear();
		bump_size = start_size;
		push_buffer(alloc);
	}
	void multi_stager_frame::upload_buffer(const vma::allocator &alloc, const vma::buffer &target, dsize at, const void *data, usize size) {
		const auto [i, off, map] = prepare_upload(alloc, size);
		std::memcpy(map, data, size);
		tasks.push_back({*buffers[i], *target, {off, at, size}});
	}
	void multi_stager_frame::commit(vk::CommandBuffer command_buffer) {
		for (const auto &b : buffers) {
			b.unmap();
		}
		for (const auto &[src, dst, cpy] : tasks) {
			// TODO: maybe consider batching copies when they have common src and dst buffers (unordered map<pair<buff, buff>, vec<buffer_copy>>)?
			command_buffer.copyBuffer(src, dst, cpy);
		}
		tasks.clear();
	}
	void multi_stager_frame::push_buffer(const vma::allocator &alloc) {
		buffers.push_back(alloc.create_buffer({{}, bump_size, vk::BufferUsageFlagBits::eTransferSrc},
			{vma::allocation_create_flag_bits::eHostAccessSequentialWrite, vma::memory_usage::eAuto}));
		maps.push_back({static_cast<u8 *>(buffers.back().map()), 0});
	}
	std::tuple<usize, dsize, u8 *> multi_stager_frame::prepare_upload(const vma::allocator &alloc, usize size) {
		auto fmi = free_maps.lower_bound(size);
		usize i;
		if (fmi == free_maps.end()) {
			i = buffers.size();
			bump_size = std::max(bump_size + bump_size / 2, size);
			push_buffer(alloc);
			free_maps.insert({ bump_size - size, i });
		} else {
			i = fmi->second;
			usize ns = fmi->first - size;
			free_maps.erase(fmi);
			free_maps.insert({ ns, i });
		}
		const auto [map_pos, map_offset] = maps[i];
		maps[i].first += size;
		maps[i].second += size;
		return { i, map_offset, map_pos };
	}
	void multi_stager_frame_synced::prepare(const vma::allocator &alloc) {
		rwl.lockw();
		multi_stager_frame::prepare(alloc);
		rwl.unlockw();
	}
	void multi_stager_frame_synced::upload_buffer(const vma::allocator &alloc, const vma::buffer &target, dsize at, const void *data, usize size) {
		rwl.lockr();
		mut.lock();
		const auto [i, off, map] = prepare_upload(alloc, size);
		tasks.push_back({*buffers[i], *target, {off, at, size}});
		mut.unlock();
		std::memcpy(map, data, size);
		rwl.unlockr();
	}
	void multi_stager_frame_synced::commit(vk::CommandBuffer command_buffer) {
		rwl.lockw();
		multi_stager_frame::commit(command_buffer);
		rwl.unlockw();
	}
	void multi_stager::prepare(u32 frame) { frames[frame].prepare(alloc); }
	void multi_stager::upload_buffer(const vma::buffer &target, dsize at, const void *data, usize size, u32 frame) { frames[frame].upload_buffer(alloc, target, at, data, size); }
	void multi_stager::commit(vk::CommandBuffer command_buffer, u32 frame) { frames[frame].commit(command_buffer); }
	void multi_stager_synced::prepare(u32 frame) { frames[frame].prepare(alloc); }
	void multi_stager_synced::upload_buffer(const vma::buffer &target, dsize at, const void *data, usize size, u32 frame) { frames[frame].upload_buffer(alloc, target, at, data, size); }
	void multi_stager_synced::commit(vk::CommandBuffer command_buffer, u32 frame) { frames[frame].commit(command_buffer); }
	buffer_handler::buffer_handler(vk::PhysicalDevice physical_device, const vk::raii::Device &device, const vma::allocator &allocator,
		vk::CommandPool command_pool, vk::Queue transfer_queue) : physical_device(physical_device), device(device), allocator(allocator),
		command_pool(command_pool), transfer_queue(transfer_queue) {
		vk::PhysicalDeviceMemoryProperties mem_properties = physical_device.getMemoryProperties();
		use_staging = false;
		if (mem_properties.memoryHeapCount > 1 && mem_properties.memoryTypeCount > 1) {
			for (u32 i = 0; i < mem_properties.memoryTypeCount; i++) {
				if (!(mem_properties.memoryTypes[i].propertyFlags & (vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent))) {
					use_staging = true;
					break;
				}
			}
		}
		logs::debugln("buffer handler", "staging is ", use_staging ? "on" : "off");
	}
	bool buffer_handler::should_stage() const { return use_staging; }
	vma::buffer buffer_handler::make_device_buffer_dedicated_stage(const void *data, u32 size, vk::BufferUsageFlags usage, vk::BufferCreateFlags create_flags,
		vk::SharingMode sharing_mode, const vk::ArrayProxyNoTemporaries<const u32> &queues) const {
		if (use_staging) {
			vma::buffer buff_stage = allocator.create_buffer({{}, size, vk::BufferUsageFlagBits::eTransferSrc, vk::SharingMode::eExclusive},
				vma::allocation_create_info{vma::allocation_create_flag_bits::eHostAccessSequentialWrite, vma::memory_usage::eAuto});
			buff_stage.flash(data, size);
			vma::buffer buff = CONSTANTAN_DEVICE_BUFFER_MAKE_FROM_ARGS(| vk::BufferUsageFlagBits::eTransferDst, {}, vma::memory_usage::eAutoPreferDevice);
			{
				rend::autocommand cmd(device, command_pool, transfer_queue);
				cmd.copyBuffer(*buff_stage, *buff, { {0, 0, size} });
			}
			return buff;
		} else {
			vma::buffer buff = CONSTANTAN_DEVICE_BUFFER_MAKE_FROM_ARGS(, vma::allocation_create_flag_bits::eHostAccessSequentialWrite, vma::memory_usage::eAuto);
			buff.flash(data, size);
			return buff;
		}
	}
	vma::image buffer_handler::make_device_image_dedicated_stage(const void *data, u32 size, vk::ImageUsageFlagBits usage,
		const vk::Extent3D &image_extent, const vk::ImageSubresourceLayers &img_subres_layers, vk::Format image_format, vk::ImageLayout final_layout,
		u32 mip_levels, u32 array_layers, vk::ImageCreateFlags create_flags, vk::SampleCountFlagBits samples, vk::ImageTiling tiling,
		vk::ImageType image_type, vk::SharingMode sharing_mode, const vk::ArrayProxyNoTemporaries<const u32> &queues) const {
		vma::buffer buff_stage = allocator.create_buffer({{}, size, vk::BufferUsageFlagBits::eTransferSrc, vk::SharingMode::eExclusive},
			vma::allocation_create_info{vma::allocation_create_flag_bits::eHostAccessSequentialWrite, vma::memory_usage::eAuto});
		buff_stage.flash(data, size);
		vma::image img = allocator.create_image({create_flags, image_type, image_format, image_extent, mip_levels, array_layers, samples, tiling,
			usage | vk::ImageUsageFlagBits::eTransferDst, sharing_mode, queues}, vma::allocation_create_info{{}, vma::memory_usage::eAutoPreferDevice});
		{
			rend::autocommand cmd(device, command_pool, transfer_queue);
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eBottomOfPipe, vk::PipelineStageFlagBits::eTransfer, {}, {}, {},
				{{{}, vk::AccessFlagBits::eTransferWrite, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal,
					VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED, *img, vk::ImageSubresourceRange{img_subres_layers.aspectMask, 0, mip_levels, 0, array_layers}}});
			cmd.copyBufferToImage(*buff_stage, *img, vk::ImageLayout::eTransferDstOptimal, { {0, 0, 0, img_subres_layers, { 0, 0, 0 }, image_extent} });
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eTopOfPipe, {}, {}, {},
				{{vk::AccessFlagBits::eTransferWrite, {}, vk::ImageLayout::eTransferDstOptimal, final_layout,
					VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED, *img, vk::ImageSubresourceRange{img_subres_layers.aspectMask, 0, mip_levels, 0, array_layers}}});
		}
		return img;
	}
	const vk::raii::Device &buffer_handler::get_device() const { return device; }
	const vma::allocator &buffer_handler::get_alloc() const { return allocator; }
	multi_stager buffer_handler::get_multi_stager() const { return multi_stager(allocator); }
	multi_stager_synced buffer_handler::get_multi_stager_synced() const { return multi_stager_synced(allocator); }
	dyn_stager *buffer_handler::get_stager() const { return use_staging ? static_cast<dyn_stager *>(new multi_stager(allocator)) : new fake_stager(allocator); }
	dyn_stager *buffer_handler::get_stager_synced() const { return use_staging ? static_cast<dyn_stager *>(new multi_stager_synced(allocator)) : new fake_stager(allocator); }

	texture2d::texture2d(vma::image &&image, const vk::raii::Device &device, vk::Format format, const vk::ImageSubresourceRange &subresource_range,
		const vk::ComponentMapping &component_mapping) : img(std::forward<vma::image>(image)), iv(nullptr) {
		iv = device.createImageView(vk::ImageViewCreateInfo{{}, *img, vk::ImageViewType::e2D, format, component_mapping, subresource_range});
	}
	void texture2d::set_debug_names(const namer &nm, const std::string &name) const {
		nm(*img, (name + " image").c_str());
		nm(*iv, (name + " image view").c_str());
	}
	vk::DescriptorImageInfo texture2d::get_dii(vk::Sampler sampler, vk::ImageLayout layout) const { return { sampler, *iv, layout }; }
	u32 texture2d::get_width() const { return width; }
	u32 texture2d::get_height() const { return height; }
}
