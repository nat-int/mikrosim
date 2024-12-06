#pragma once
#include <array>
#include <glm/glm.hpp>
#include "../vulkan_glfw_pch.hpp"
#include "../defs.hpp"

namespace rend { class window; }
namespace input {
	class input_handler {
	private:
		glm::vec2 cursor_pos;
		glm::vec2 last_cursor_pos;
		std::array<bool, GLFW_KEY_LAST+1> key_held_last;
		std::array<bool, GLFW_KEY_LAST+1> key_held;
		std::array<bool, GLFW_MOUSE_BUTTON_LAST+1> mouse_held_last;
		std::array<bool, GLFW_MOUSE_BUTTON_LAST+1> mouse_held;
	public:
		bool is_key_down(i32 key) const;
		bool is_key_held(i32 key) const;
		bool is_key_up(i32 key) const;
		bool is_mouse_down(i32 button) const;
		bool is_mouse_held(i32 button) const;
		bool is_mouse_up(i32 button) const;
		glm::vec2 get_cursor_pos() const;
		glm::vec2 get_last_cursor_pos() const;
		glm::vec2 get_cursor_delta() const;
		void update(GLFWwindow *w);
		void update(const rend::window &w);
	};
}

