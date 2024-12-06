#pragma once

#include "../vulkan_glfw_pch.hpp"
#include "../defs.hpp"

namespace vma {
	enum class allocation_create_flag_bits : VmaAllocationCreateFlags {
		eDedicatedMemory = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
		eNeverAllocate = VMA_ALLOCATION_CREATE_NEVER_ALLOCATE_BIT,
		eMapped = VMA_ALLOCATION_CREATE_MAPPED_BIT,
		eUpperAddress = VMA_ALLOCATION_CREATE_UPPER_ADDRESS_BIT,
		eDontBind = VMA_ALLOCATION_CREATE_DONT_BIND_BIT,
		eWithinBudget = VMA_ALLOCATION_CREATE_WITHIN_BUDGET_BIT,
		eCanAlias = VMA_ALLOCATION_CREATE_CAN_ALIAS_BIT,
		eHostAccessSequentialWrite = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
		eHostAccessRandom = VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT,
		eHostAccessAllowTransferInstead = VMA_ALLOCATION_CREATE_HOST_ACCESS_ALLOW_TRANSFER_INSTEAD_BIT,
		eStrategyMinMemory = VMA_ALLOCATION_CREATE_STRATEGY_MIN_MEMORY_BIT,
		eStrategyMinTime = VMA_ALLOCATION_CREATE_STRATEGY_MIN_TIME_BIT,
		eStrategyMinOffset = VMA_ALLOCATION_CREATE_STRATEGY_MIN_OFFSET_BIT
	};
	using allocation_create_flags = vk::Flags<allocation_create_flag_bits>;
	enum class memory_usage {
		eUnknown = VMA_MEMORY_USAGE_UNKNOWN,
		eGpuLazilyAllocated = VMA_MEMORY_USAGE_GPU_LAZILY_ALLOCATED,
		eAuto = VMA_MEMORY_USAGE_AUTO,
		eAutoPreferDevice = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
		eAutoPreferHost = VMA_MEMORY_USAGE_AUTO_PREFER_HOST
	};
	struct allocation_create_info {
    	using NativeType = VmaAllocationCreateInfo;

		allocation_create_flags flags;
		VmaMemoryUsage usage;
		vk::MemoryPropertyFlags required_flags;
		vk::MemoryPropertyFlags preferred_flags;
		u32 memory_type_bits;
		VmaPool pool;
		void *p_user_data;
		float priority;

		allocation_create_info(allocation_create_flags flags={}, memory_usage usage={}, vk::MemoryPropertyFlags required_flags={}, vk::MemoryPropertyFlags preferred_flags={},
			u32 memory_type_bits=0, VmaPool pool=VK_NULL_HANDLE, void *p_user_data=nullptr, float priority={});
		operator const NativeType &() const;
	};
	struct buffer {
	public:
		buffer(std::nullptr_t);
		buffer(VmaAllocator allocator, const vk::BufferCreateInfo &buffer_create, const allocation_create_info &alloc_create);
		buffer(const buffer &) = delete;
		buffer(buffer &&o);
		~buffer();
		buffer &operator=(buffer &&o);
		void *map() const;
		void unmap() const;
		void flash(const void *data, const usize size) const;
		vk::Buffer operator*() const;
		VmaAllocation operator~() const;
	private:
		vk::Buffer handle;
		VmaAllocation alloc;
		VmaAllocator allocator;
	};
	struct image {
	public:
		image(std::nullptr_t);
		image(VmaAllocator allocator, const vk::ImageCreateInfo &image_create, const allocation_create_info &alloc_create);
		image(const image &) = delete;
		image(image &&o);
		~image();
		image &operator=(image &&o);
		vk::Image operator*() const;
		VmaAllocation operator~() const;
	private:
		vk::Image handle;
		VmaAllocation alloc;
		VmaAllocator allocator;
	};
	struct allocator {
	public:
		allocator(std::nullptr_t);
		allocator(VmaAllocator allocator);
		allocator(const VmaAllocatorCreateInfo &info);
		allocator(allocator &&o);
		~allocator();
		void late_init(VmaAllocator allocator);
		void late_init(const VmaAllocatorCreateInfo &info);
		VmaAllocator operator*() const;
		buffer create_buffer(const vk::BufferCreateInfo &buffer_create, const allocation_create_info &alloc_create) const;
		image create_image(const vk::ImageCreateInfo &image_create, const allocation_create_info &alloc_create) const;
	private:
		VmaAllocator handle;
	};
}
