#include "vma_wrap.hpp"

#include <algorithm>
#include "context.hpp"

namespace vma {
	allocation_create_info::allocation_create_info(allocation_create_flags flags, memory_usage usage, vk::MemoryPropertyFlags required_flags, vk::MemoryPropertyFlags preferred_flags,
		u32 memory_type_bits, VmaPool pool, void *p_user_data, float priority) : flags(flags), usage(static_cast<VmaMemoryUsage>(usage)), required_flags(required_flags),
		preferred_flags(preferred_flags), memory_type_bits(memory_type_bits), pool(pool), p_user_data(p_user_data), priority(priority) { }
	allocation_create_info::operator const NativeType &() const {
		return *reinterpret_cast<const NativeType *>(this);
	}
	buffer::buffer(std::nullptr_t) : handle(nullptr), alloc(nullptr), allocator(VK_NULL_HANDLE) { }
	buffer::buffer(VmaAllocator allocator, const vk::BufferCreateInfo &buffer_create, const allocation_create_info &alloc_create) : allocator(allocator) {
		const VkBufferCreateInfo bc = buffer_create;
		const VmaAllocationCreateInfo ac = alloc_create;
		VkBuffer h;
		if (vmaCreateBuffer(allocator, &bc, &ac, &h, &alloc, nullptr) != VK_SUCCESS) {
			throw std::runtime_error("failed to create buffer through vma");
		}
		handle = h;
	}
	buffer::buffer(buffer &&o) : handle(o.handle), alloc(o.alloc), allocator(o.allocator) { o.handle = VK_NULL_HANDLE; o.alloc = nullptr; }
	buffer::~buffer() {
		if (handle)
			vmaDestroyBuffer(allocator, handle, alloc);
	}
	buffer &buffer::operator=(buffer &&o) {
		if (handle)
			vmaDestroyBuffer(allocator, handle, alloc);
		handle = o.handle; o.handle = VK_NULL_HANDLE;
		alloc = o.alloc; o.alloc = nullptr;
		allocator = o.allocator;
		return *this;
	}
	void *buffer::map() const {
		void *out;
		if (vmaMapMemory(allocator, alloc, &out) != VK_SUCCESS) {
			throw std::runtime_error("failed to map buffer through vma");
		}
		return out;
	}
	void buffer::unmap() const {
		vmaUnmapMemory(allocator, alloc);
	}
	void buffer::flash(const void *data, const usize size) const {
		void *m = map();
		const char *cd = static_cast<const char *>(data);
		std::copy(cd, cd+size, static_cast<char *>(m));
		unmap();
	}
	vk::Buffer buffer::operator*() const { return handle; }
	VmaAllocation buffer::operator~() const { return alloc; }

	image::image(std::nullptr_t) : handle(nullptr) { }
	image::image(VmaAllocator allocator, const vk::ImageCreateInfo &image_create, const allocation_create_info &alloc_create) : allocator(allocator) {
		const VkImageCreateInfo ic = image_create;
		const VmaAllocationCreateInfo ac = alloc_create;
		VkImage h;
		if (vmaCreateImage(allocator, &ic, &ac, &h, &alloc, nullptr) != VK_SUCCESS) {
			throw std::runtime_error("failed to create image through vma");
		}
		handle = h;
	}
	image::image(image &&o) : handle(o.handle), alloc(o.alloc), allocator(o.allocator) { o.handle = VK_NULL_HANDLE; o.alloc = nullptr; }
	image::~image() {
		if (handle)
			vmaDestroyImage(allocator, handle, alloc);
	}
	image &image::operator=(image &&o) {
		if (handle)
			vmaDestroyImage(allocator, handle, alloc);
		handle = o.handle; o.handle = VK_NULL_HANDLE;
		alloc = o.alloc; o.alloc = nullptr;
		allocator = o.allocator;
		return *this;
	}
	vk::Image image::operator*() const { return handle; }
	VmaAllocation image::operator~() const { return alloc; }

	allocator::allocator(std::nullptr_t) : handle(VK_NULL_HANDLE) { }
	allocator::allocator(VmaAllocator allocator) : handle(allocator) { }
	allocator::allocator(const VmaAllocatorCreateInfo &info) { vmaCreateAllocator(&info, &handle); }
	allocator::allocator(allocator &&o) : handle(o.handle) { o.handle = VK_NULL_HANDLE; }
	allocator::~allocator() {
		if (handle != VK_NULL_HANDLE)
			vmaDestroyAllocator(handle);
	}
	void allocator::late_init(VmaAllocator allocator) { handle = allocator; }
	void allocator::late_init(const VmaAllocatorCreateInfo &info) { vmaCreateAllocator(&info, &handle); }
	VmaAllocator allocator::operator*() const { return handle; }
	buffer allocator::create_buffer(const vk::BufferCreateInfo &buffer_create, const allocation_create_info &alloc_create) const {
		return buffer(handle, buffer_create, alloc_create);
	}
	image allocator::create_image(const vk::ImageCreateInfo &image_create, const allocation_create_info &alloc_create) const {
		return image(handle, image_create, alloc_create);
	}
}

#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>
