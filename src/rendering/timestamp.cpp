#include "timestamp.hpp"

namespace rend {
	bool supports_timestamps(const physical_device_info &phy_device, u32 queue) {
		return phy_device.properties.limits.timestampPeriod > 0 &&
			(phy_device.properties.limits.timestampComputeAndGraphics ||
			 phy_device.handle.getQueueFamilyProperties()[queue].timestampValidBits);
	}
}

