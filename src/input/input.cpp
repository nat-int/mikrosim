#include "input.hpp"
#include "../rendering/window.hpp"

namespace input {
	bool input_handler::is_key_down(i32 key) const { return key_held[usize(key)] && !key_held_last[usize(key)]; }
	bool input_handler::is_key_held(i32 key) const { return key_held[usize(key)]; }
	bool input_handler::is_key_up(i32 key) const { return !key_held[usize(key)] && key_held_last[usize(key)]; }
	bool input_handler::is_mouse_down(i32 button) const { return mouse_held[usize(button)] && !mouse_held_last[usize(button)]; }
	bool input_handler::is_mouse_held(i32 button) const { return mouse_held[usize(button)]; }
	bool input_handler::is_mouse_up(i32 button) const { return !mouse_held[usize(button)] && mouse_held_last[usize(button)]; }
	glm::vec2 input_handler::get_cursor_pos() const { return cursor_pos; }
	glm::vec2 input_handler::get_last_cursor_pos() const { return last_cursor_pos; }
	glm::vec2 input_handler::get_cursor_delta() const { return cursor_pos - last_cursor_pos; }
	void input_handler::update(GLFWwindow *w) {
		key_held_last.swap(key_held);
		mouse_held_last.swap(mouse_held);
		last_cursor_pos = cursor_pos;
		f64 x, y;
		glfwGetCursorPos(w, &x, &y);
		cursor_pos = {x, y};
		for (i32 i = GLFW_KEY_SPACE; i < i32(key_held.size()); i++) { // is space always the first key?
			key_held[usize(i)] = glfwGetKey(w, i) == GLFW_PRESS;
		}
		for (i32 i = 0; i < i32(mouse_held.size()); i++) {
			mouse_held[usize(i)] = glfwGetMouseButton(w, i) == GLFW_PRESS;
		}
	}
	void input_handler::update(const rend::window &w) {
		update(w.window_handle);
	}
}

