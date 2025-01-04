#pragma once
#include <array>
#include "../vulkan_glfw_pch.hpp"
#include "../defs.hpp"
#include "device.hpp"

namespace rend {
	bool supports_timestamps(const physical_device_info &phy_device, u32 queue);

	template<u32 N>
	class timestamps {
	public:
		vk::raii::QueryPool qpool;
		bool active;
		std::array<u64, N> last;

		inline timestamps() : qpool(nullptr), active(true) {
			std::fill_n(last.begin(), N, 0);
		}
		inline void init(const vk::raii::Device &device) {
			qpool = device.createQueryPool({{}, vk::QueryType::eTimestamp, N});
		}
		inline void update() {
			if (active || !*qpool)
				return;
			auto [r, t] = qpool.getResult<std::array<u64, N*2>>(0, N, sizeof(u64),
				vk::QueryResultFlagBits::e64 | vk::QueryResultFlagBits::eWithAvailability);
			if (r != vk::Result::eSuccess)
				return;
			for (u32 i = 0; i < N; i++) {
				if (t[N + i] == 0)
					return;
			}
			std::copy_n(t.begin(), N, last.begin());
			active = true;
		}
		inline void reset(vk::CommandBuffer cmd) const {
			if (active && *qpool)
				cmd.resetQueryPool(*qpool, 0, N);
		}
		inline void stamp(vk::CommandBuffer cmd, vk::PipelineStageFlagBits stage, u32 query) const {
			if (active && *qpool)
				cmd.writeTimestamp(stage, *qpool, query);
		}
		inline void end() {
			active = false;
		}
		inline u64 delta(u32 i, u32 from, f64 period) const {
			return u64(f64(last[i] - last[from]) * period);
		}
	};
}

