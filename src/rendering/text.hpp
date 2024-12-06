#pragma once
#include <optional>
#include <string_view>
#include <vector>
#include "../format/qoi.hpp"
#include "2d.hpp"
#include "buffers.hpp"

namespace rend {
	class text {
	public:
		void load_ascii_atlas(const buffer_handler &buff_handler);
		void init_ascii_atlas_ds2d(const renderer2d &rend2d, vk::DescriptorPool pool);
		vk::DescriptorSet get_ds2d(u32 frame) const;
		float scr_px_range2d(float text_px) const;
		std::vector<vertex2d> vb_only_text2d(const std::string_view str, float &x, float &y, float size, float startx, float line_height, const glm::vec4 &color) const;
		std::vector<vertex2d> vb_only_text2d(const std::string_view str, float &x, float &y, float size, float startx, float line_height, const glm::vec4 &color, std::vector<float> &spaces) const;
	private:
		std::optional<texture2d> ascii_atlas;
		std::vector<vk::raii::DescriptorSet> ascii_atlas_ds2d;
	};
}
