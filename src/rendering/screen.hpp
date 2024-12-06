#pragma once
#include "../defs.hpp"
#include "2d.hpp"
#include "buffers.hpp"
#include "text.hpp"

namespace rend {
	template<anchor2d a> struct offsets {};
	template<> struct offsets<anchor2d::center> { constexpr static float x = .5f, y = .5f; };
	template<> struct offsets<anchor2d::top> { constexpr static float x = .5f, y = 0.f; };
	template<> struct offsets<anchor2d::topright> { constexpr static float x = 1.f, y = 0.f; };
	template<> struct offsets<anchor2d::right> { constexpr static float x = 1.f, y = .5f; };
	template<> struct offsets<anchor2d::bottomright> { constexpr static float x = 1.f, y = 1.f; };
	template<> struct offsets<anchor2d::bottom> { constexpr static float x = .5f, y = 1.f; };
	template<> struct offsets<anchor2d::bottomleft> { constexpr static float x = 0.f, y = 1.f; };
	template<> struct offsets<anchor2d::left> { constexpr static float x = 0.f, y = .5f; };
	template<> struct offsets<anchor2d::topleft> { constexpr static float x = 0.f, y = 0.f; };
	template<anchor2d a> struct is_bottom_anchor : std::false_type { };
	template<> struct is_bottom_anchor<anchor2d::bottomright> : std::true_type { };
	template<> struct is_bottom_anchor<anchor2d::bottom> : std::true_type { };
	template<> struct is_bottom_anchor<anchor2d::bottomleft> : std::true_type { };

	struct screen_render_info {
		constexpr static usize unfocused = usize(-1);
		u32 frame;
		vma::cnst::buffer text_col_quad{nullptr};
		vma::cnst::buffer focus_col_quad{nullptr};
		vma::cnst::buffer inactive_text_col_quad{nullptr};
		vma::cnst::buffer button_background_col_quad{nullptr};
		vma::cnst::buffer input_background_col_quad{nullptr};
		vma::cnst::buffer text_sel_background_col_quad{nullptr};
		usize focused;
		glm::vec2 cursor_pos;

		const buffer_handler &buff_h;
		const cnst::rend::text &txt;
		multi_stager *stager;

		// TODO: uninline
		inline screen_render_info(const buffer_handler &buff_h, const cnst::rend::text &txt) : buff_h(buff_h), txt(txt) { }
	};
	namespace {
		template<glm::vec4 Col> inline void make_quad(vma::cnst::buffer &buff, const buffer_handler &buff_h, multi_stager &stager) {
			if (!*buff) {
				buff = buff_h.make_device_buffer_staged(stager, 0, quad6v2d({0.f, 0.f}, {1.f, 1.f}, Col), vk::BufferUsageFlagBits::eVertexBuffer);
			}
		}
	}

	namespace component {
		// components dedicated stage - keeping things simple for now
		// TODO: use some common staging buffer?
		// TODO: uninline
		class base {
		public:
			constexpr static bool focusable = false;
			u32 id;

			inline void on_key(i32 key, i32 act, i32 mods) { (void)key; (void)act; (void)mods; }
			inline void on_char(u32 codepoint) { (void)codepoint; }
			inline void on_mousebutton(i32 button, i32 act, i32 mods, const glm::vec2 &rp) { (void)button; (void)act; (void)mods; (void)rp; }
			template<typename Style> inline void post(vk::CommandBuffer cmdbuf, const screen_render_info &sri) { (void)cmdbuf; (void)sri; }
			inline float width() const { return size.x; }
			inline float height() const { return size.y; }
			inline bool is_inside(glm::vec2 p) const { return p.x >= 0 && p.y >= 0 && p.x < size.x && p.y < size.y; }
			inline bool is_inside(const glm::mat4 &proj, glm::vec2 p, glm::vec2 pos) const { return is_inside(glm::vec2(proj * glm::vec4(p, 0, 1)) - pos); }
		protected:
			glm::vec2 size;
		};
		template<const char *Text>
		class static_text : public base {
		public:
			template<typename Style> inline void init(const renderer2d &, screen_render_info &sri) {
				float x=0,y=0;
				std::vector<vertex2d> vd = sri.txt.vb_only_text2d(Text, x, y, Style::text_size, 0.f, Style::text_size, Style::text_color);
				mesh.set(sri.buff_h.make_device_buffer_staged(*sri.stager, 0, vd, vk::BufferUsageFlagBits::eVertexBuffer), static_cast<u32>(vd.size()));
				size.x = x;
				size.y = y + Style::text_size;
			}
			template<typename Style> inline void render_text(vk::CommandBuffer cmdbuf, const renderer2d &rend2d, anchor2d anchor, float x, float y, const screen_render_info &) const {
				rend2d.push_text_projection(cmdbuf, glm::translate(rend2d.proj(anchor), glm::vec3(x, y, 0)));
				mesh.bind_draw(cmdbuf);
			}
			template<typename Style> inline void render_solid(vk::CommandBuffer, const renderer2d &, anchor2d, float, float, const screen_render_info &) const { }
		private:
			simple_mesh mesh;
		};
		template<u32 Width, const char *ShadowText>
		class text_input : public base {
		public:
			constexpr static bool focusable = true;
			std::u32string value;

			template<typename Style> inline void init(const renderer2d &, screen_render_info &sri) {
				float x=0,y=0;
				std::vector<vertex2d> vd = sri.txt.vb_only_text2d(ShadowText, x, y, Style::text_size, 0.f, Style::text_size, Style::inactive_text_color);
				shadow_mesh.set(sri.buff_h.make_device_buffer_staged(*sri.stager, 0, vd, vk::BufferUsageFlagBits::eVertexBuffer), static_cast<u32>(vd.size()));
				text_mesh.emplace(replace_simple_mesh(sri.buff_h.get_alloc(), vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eVertexBuffer,
					{{}, vma::cnst::memory_usage::eAuto}));
				size.x = Width + 2*Style::padding;
				size.y = Style::text_size + 2*Style::padding;
				make_quad<Style::input_background>(sri.input_background_col_quad, sri.buff_h, *sri.stager);
				make_quad<Style::focus_color>(sri.focus_col_quad, sri.buff_h, *sri.stager);
				make_quad<Style::text_color>(sri.text_col_quad, sri.buff_h, *sri.stager);
				make_quad<Style::text_sel_background>(sri.text_sel_background_col_quad, sri.buff_h, *sri.stager);
				val_changed = false;
				cursor = 0;
				sel_end = nsel;
				sps = {0};
			}
			template<typename Style> inline void post(vk::CommandBuffer cmdbuf, const screen_render_info &sri) {
				(void)cmdbuf;
				if (val_changed) {
					val_changed = false;
					float tx=0,ty=0;
					sps.clear();
					std::vector<vertex2d> nvd = sri.txt.vb_only_text2d(valu8(), tx, ty, Style::text_size, 0.f, Style::text_size, Style::text_color, sps);
					if (nvd.empty()) {
						text_mesh->update_keep();
					} else {
						text_mesh->update_change(*sri.stager, sri.frame, nvd, nvd.size());
					}
				} else {
					text_mesh->update_keep();
				}
			}
			template<typename Style> inline void render_text(vk::CommandBuffer cmdbuf, const renderer2d &rend2d, anchor2d anchor, float x, float y, const screen_render_info &sri) {
				(void)sri;
				rend2d.push_text_projection(cmdbuf, glm::translate(rend2d.proj(anchor), glm::vec3(x+Style::padding, y+Style::padding, 0)));
				if (value.empty())
					shadow_mesh.bind_draw(cmdbuf);
				else if (**text_mesh)
					text_mesh->bind_draw(cmdbuf);
			}
			template<typename Style> inline void render_solid(vk::CommandBuffer cmdbuf, const renderer2d &rend2d, anchor2d anchor, float x, float y, const screen_render_info &sri) const {
				const glm::mat4 proj = rend2d.proj(anchor);
				rend2d.push_text_projection(cmdbuf, glm::scale(glm::translate(proj, {x, y, 0}), {size.x, size.y, 1.f}));
				if (id == sri.focused || is_inside(proj, sri.cursor_pos, glm::vec2(x, y))) {
					cmdbuf.bindVertexBuffers(0, *sri.focus_col_quad, { 0 }); // TODO: check what's bound (at compile time)?
					cmdbuf.draw(6, 1, 0, 0);
					rend2d.push_text_projection(cmdbuf, glm::scale(glm::translate(proj, {x+Style::padding, y+Style::padding, 0}), {Width, Style::text_size, 1.f}));
				}
				cmdbuf.bindVertexBuffers(0, *sri.input_background_col_quad, { 0 });
				cmdbuf.draw(6, 1, 0, 0);
				if (sri.focused == id && !val_changed) {
					if (sel_end != nsel) {
						const auto [mnp, mxp] = std::minmax(cursor, sel_end);
						rend2d.push_text_projection(cmdbuf, glm::scale(glm::translate(proj, {x+sps[mnp]+Style::padding, y+Style::padding, 0}), {sps[mxp]-sps[mnp], Style::text_size, 1.f}));
						cmdbuf.bindVertexBuffers(0, *sri.text_sel_background_col_quad, { 0 });
						cmdbuf.draw(6, 1, 0, 0);
					}
					rend2d.push_text_projection(cmdbuf, glm::scale(glm::translate(proj, {x+sps[cursor]-1.f+Style::padding, y+Style::padding, 0}), {3.f, Style::text_size, 1.f}));
					cmdbuf.bindVertexBuffers(0, *sri.text_col_quad, { 0 });
					cmdbuf.draw(6, 1, 0, 0);
				}
			}
			inline void on_key(i32 key, i32 act, i32 mods) {
				auto movement_sel_check = [this, mods]() {
					if ((mods & GLFW_MOD_SHIFT) && sel_end == nsel)
						sel_end = cursor;
					else if (!(mods & GLFW_MOD_SHIFT) && sel_end != nsel)
						sel_end = nsel;
				};
				if (act == GLFW_PRESS || act == GLFW_REPEAT) {
					switch (key) {
					case GLFW_KEY_LEFT: if (cursor > 0) { movement_sel_check(); cursor--; } break;
					case GLFW_KEY_RIGHT: if (cursor < value.size()) { movement_sel_check(); cursor++; } break;
					case GLFW_KEY_BACKSPACE: if (!del_sel() && cursor > 0) { cursor--; value.erase(cursor, 1); val_changed = true; } break;
					case GLFW_KEY_DELETE: if (!del_sel() && cursor < value.size()) { value.erase(cursor, 1); val_changed = true; } break;
					case GLFW_KEY_HOME: if (cursor > 0) { movement_sel_check(); cursor = 0; } break;
					case GLFW_KEY_END: if (cursor < value.size()) { movement_sel_check(); cursor = value.size(); } break;
					case GLFW_KEY_C: if (mods & GLFW_MOD_CONTROL && sel_end != nsel) { glfwSetClipboardString(nullptr, utf32_to_utf8(sel_value()).c_str()); } break;
					case GLFW_KEY_V: if (mods & GLFW_MOD_CONTROL) { del_sel(); auto clip = utf8_to_utf32(glfwGetClipboardString(nullptr)); value.insert(cursor, clip); cursor += clip.size(); val_changed = true; } break;
					default: break;
					}
				}
			}
			inline void on_mousebutton(i32 button, i32 act, i32 mods, const glm::vec2 &rp) {
				if (act == GLFW_PRESS && button == GLFW_MOUSE_BUTTON_LEFT) {
					usize newpos = std::lower_bound(sps.begin(), sps.end(), rp.x) - sps.begin();
					if (newpos < sps.size() - 1 && sps[newpos + 1] - rp.x < rp.x - sps[newpos]) newpos++;
					if ((mods & GLFW_MOD_SHIFT) && sel_end == nsel)
						sel_end = cursor;
					else if (!(mods & GLFW_MOD_SHIFT) && sel_end != nsel)
						sel_end = nsel;
					cursor = newpos;
				}
			}
			inline void on_char(u32 codepoint) { del_sel(); value.insert(cursor++, 1, codepoint); val_changed = true; }
			inline std::string valu8() const { return utf32_to_utf8(value); }
		private:
			constexpr static usize nsel = usize(-1);

			simple_mesh shadow_mesh;
			std::optional<replace_simple_mesh> text_mesh;
			std::vector<float> sps;
			bool val_changed;
			usize cursor;
			usize sel_end;

			inline std::u32string sel_value() {
				if (sel_end == nsel)
					return std::u32string{};
				const auto [mnp, mxp] = std::minmax(cursor, sel_end);
				return value.substr(mnp, mxp-mnp);
			}
			inline bool del_sel() {
				if (sel_end == nsel)
					return false;
				const auto [mnp, mxp] = std::minmax(cursor, sel_end);
				value.erase(mnp, mxp-mnp);
				cursor = mnp;
				sel_end = nsel;
				val_changed = true;
				return true;
			}
		};
		template<const char *Text, u32 Width, typename FT=void (*)()>
		class button : public base {
		public:
			constexpr static bool focusable = true;

			template<typename Style> inline void init(const renderer2d &, screen_render_info &sri) {
				float x=0,y=0;
				std::vector<vertex2d> vd = sri.txt.vb_only_text2d(Text, x, y, Style::text_size, 0.f, Style::text_size, Style::text_color);
				mesh.set(sri.buff_h.make_device_buffer_staged(*sri.stager, 0, vd, vk::BufferUsageFlagBits::eVertexBuffer), static_cast<u32>(vd.size()));
				size.x = Width + Style::padding * 2;
				size.y = Style::text_size + Style::padding * 2;
				make_quad<Style::button_background>(sri.button_background_col_quad, sri.buff_h, *sri.stager);
				make_quad<Style::focus_color>(sri.focus_col_quad, sri.buff_h, *sri.stager);
			}
			template<typename Style> inline void render_text(vk::CommandBuffer cmdbuf, const renderer2d &rend2d, anchor2d anchor, float x, float y, const screen_render_info &) const {
				rend2d.push_text_projection(cmdbuf, glm::translate(rend2d.proj(anchor), glm::vec3(x+Style::padding, y+Style::padding, 0)));
				mesh.bind_draw(cmdbuf);
			}
			template<typename Style> inline void render_solid(vk::CommandBuffer cmdbuf, const renderer2d &rend2d, anchor2d anchor, float x, float y, const screen_render_info &sri) const {
				const glm::mat4 proj = rend2d.proj(anchor);
				rend2d.push_text_projection(cmdbuf, glm::scale(glm::translate(proj, {x, y, 0}), {size.x, size.y, 1.f}));
				if (id == sri.focused || is_inside(proj, sri.cursor_pos, glm::vec2(x, y))) {
					cmdbuf.bindVertexBuffers(0, *sri.focus_col_quad, { 0 }); // TODO: check what's bound (at compile time)?
					cmdbuf.draw(6, 1, 0, 0);
					rend2d.push_text_projection(cmdbuf, glm::scale(glm::translate(proj, {x+Style::padding, y+Style::padding, 0}), {Width, Style::text_size, 1.f}));
				}
				cmdbuf.bindVertexBuffers(0, *sri.button_background_col_quad, { 0 });
				cmdbuf.draw(6, 1, 0, 0);
			}
			inline void on_key(i32 key, i32 act, i32 mods) {
				(void)mods;
				if (act == GLFW_PRESS && (key == GLFW_KEY_ENTER || key == GLFW_KEY_SPACE)) {
					callback();
				}
			}
			inline void on_mousebutton(i32 button, i32 act, i32 mods, const glm::vec2 &rp) {
				(void)mods; (void)rp;
				if (act == GLFW_PRESS && button == GLFW_MOUSE_BUTTON_LEFT) {
					callback();
				}
			}
			inline void set_callback(FT &&cb) { callback = std::forward<FT>(cb); }
		private:
			simple_mesh mesh;
			FT callback;
		};
		class dyn_text : public base {
		public:
			std::u32string value;

			template<typename Style> inline void init(const renderer2d &, screen_render_info &sri) {
				text_mesh.emplace(replace_simple_mesh(sri.buff_h.get_alloc(), vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eVertexBuffer,
					{{}, vma::cnst::memory_usage::eAuto}));
				size = {0, 0};
				val_changed = false;
			}
			template<typename Style> inline void post(vk::CommandBuffer, const screen_render_info &sri) {
				if (val_changed) {
					val_changed = false;
					size = {0.f, 0.f};
					std::vector<vertex2d> nvd = sri.txt.vb_only_text2d(valu8(), size.x, size.y, Style::text_size, 0.f, Style::text_size, Style::text_color);
					size.y += Style::text_size;
					if (nvd.empty()) {
						text_mesh->update_keep();
					} else {
						text_mesh->update_change(*sri.stager, sri.frame, nvd, nvd.size());
					}
				} else {
					text_mesh->update_keep();
				}
			}
			template<typename Style> inline void render_text(vk::CommandBuffer cmdbuf, const renderer2d &rend2d, anchor2d anchor, float x, float y, const screen_render_info &) {
				if (**text_mesh) {
					rend2d.push_text_projection(cmdbuf, glm::translate(rend2d.proj(anchor), glm::vec3(x+Style::padding, y+Style::padding, 0)));
					text_mesh->bind_draw(cmdbuf);
				}
			}
			template<typename Style> inline void render_solid(vk::CommandBuffer, const renderer2d &, anchor2d, float, float, const screen_render_info &) const { }
			inline std::string valu8() const { return utf32_to_utf8(value); }
			inline void update(const std::string &val) { value = utf8_to_utf32(val); val_changed = true; }
			inline void update(const std::u32string &val) { value = val; val_changed = true; }
		private:
			constexpr static usize nsel = usize(-1);

			std::optional<replace_simple_mesh> text_mesh;
			bool val_changed;
		};
	}

	template<u8 R, u8 G, u8 B, u8 A=255>
	struct tcol {
		constexpr static glm::vec4 value = glm::vec4(R / 255.f, G / 255.f, B / 255.f, A / 255.f);
	};
	namespace tcols {
		using black = tcol<0,0,0>;
		using white = tcol<255,255,255>;
		using gray = tcol<127,127,127>;
	}
	template<u32 TextSizePx10, u32 PaddingPx10, u32 MarginPx10, typename TextCol, typename FocusCol, typename InactiveTextCol, typename ButtonBg, typename InputBg, typename TextSelBg>
	struct style {
		constexpr static f32 text_size = TextSizePx10 * 0.1f;
		constexpr static f32 padding = PaddingPx10 * 0.1f;
		constexpr static f32 margin = MarginPx10 * 0.1f;
		constexpr static glm::vec4 text_color = TextCol::value;
		constexpr static glm::vec4 focus_color = FocusCol::value;
		constexpr static glm::vec4 inactive_text_color = InactiveTextCol::value;
		constexpr static glm::vec4 button_background = ButtonBg::value;
		constexpr static glm::vec4 input_background = InputBg::value;
		constexpr static glm::vec4 text_sel_background = TextSelBg::value;
	};
	namespace styles {
		template<u32 TextSizePx10, u32 PaddingPx10, u32 MarginPx10>
		using black_blue_white = style<TextSizePx10, PaddingPx10, MarginPx10, tcols::black, tcol<20, 150, 255>, tcols::gray, tcol<60, 140, 240>, tcols::white, tcol<0, 0, 255, 100>>;
	}

	template<typename ...Ts>
	struct components;
	template<typename T>
	struct components<T> {
	public:
		inline void on_key(u32 focused, i32 key, i32 act, i32 mods) { if (comp.id == focused) comp.on_key(key, act, mods); }
		inline void on_char(u32 focused, u32 codepoint) { if (comp.id == focused) comp.on_char(codepoint); }
		template<typename Style, u32 Id=0> inline void init(const renderer2d &rend2d, screen_render_info &sri) {
			comp.id = Id;
			comp.template init<Style>(rend2d, sri);
		}

		inline usize get_focusable_id(usize min_id=0) const {
			if constexpr (T::focusable) {
				if (comp.id >= min_id)
					return comp.id;
			}
			return screen_render_info::unfocused;
		}
		template<typename CT, usize I=0> inline CT &get_comp() {
			if constexpr (std::is_same<T, CT>::value && I == 0) {
				return comp;
			}
			static_assert(std::is_same<T, CT>::value && I == 0, "component not found");
		}
		template<typename F> inline void for_each(F pred) { pred(comp); }
		template<typename ZT, typename F> inline void for_each_tz(F pred) { pred.template operator()<ZT>(comp); }
	protected:
		T comp;
	};
	template<typename T, typename ...Ts>
	struct components<T, Ts...> : public components<Ts...>, private components<T> {
	public:
		inline void on_key(u32 focused, i32 key, i32 act, i32 mods) { components<T>::on_key(focused, key, act, mods); components<Ts...>::on_key(focused, key, act, mods); }
		inline void on_char(u32 focused, u32 codepoint) { components<T>::on_char(focused, codepoint); components<Ts...>::on_char(focused, codepoint); }
		template<typename Style, u32 Id=0> inline void init(const renderer2d &rend2d, screen_render_info &sri) {
			components<T>::template init<Style, Id>(rend2d, sri);
			components<Ts...>::template init<Style, Id+1>(rend2d, sri);
		}
		inline usize get_focusable_id(usize min_id=0) const {
			if constexpr (T::focusable) {
				if (components<T>::comp.id >= min_id) {
					return components<T>::comp.id;
				}
			}
			return components<Ts...>::get_focusable_id(min_id);
		}
		template<typename CT, usize I=0> inline CT &get_comp() {
			if constexpr (std::is_same<T, CT>::value) {
				if constexpr (I == 0) {
					return components<T>::comp;
				} else {
					return components<Ts...>::template get_comp<CT, I-1>();
				}
			} else {
				return components<Ts...>::template get_comp<CT, I>();
			}
		}
		template<typename F> inline void for_each(F pred) {
			components<T>::for_each(pred);
			components<Ts...>::for_each(pred);
		}
		template<typename ZT, typename ...ZTs, typename F> inline void for_each_tz(F pred) {
			components<T>::template for_each_tz<ZT>(pred);
			components<Ts...>::template for_each_tz<ZTs...>(pred);
		}
	};

	template<anchor2d A, i32 OffX, i32 OffY, anchor2d InnerA=anchor2d::topleft>
	struct vstack_sublayout {
		constexpr static anchor2d anchor = A;
		using ioff = offsets<InnerA>;

		template<typename Style, typename P, typename T> inline void render_solid(vk::CommandBuffer cmdbuf, const renderer2d &rend2d, const screen_render_info &sri, T &comp) {
			comp.template render_solid<Style>(cmdbuf, rend2d, A, OffX - ioff::x * comp.width(), yoff - ioff::y * comp.height(), sri);
			if constexpr (is_bottom_anchor<A>::value)
				yoff -= comp.height() + Style::margin;
			else
				yoff += comp.height() + Style::margin;
		}
		template<typename Style, typename P, typename T> inline void render_text(vk::CommandBuffer cmdbuf, const renderer2d &rend2d, const screen_render_info &sri, T &comp) {
			comp.template render_text<Style>(cmdbuf, rend2d, A, OffX - ioff::x * comp.width(), yoff - ioff::y * comp.height(), sri);
			if constexpr (is_bottom_anchor<A>::value)
				yoff -= comp.height() + Style::margin;
			else
				yoff += comp.height() + Style::margin;
		}
		template<typename Style, typename P, typename T> inline void on_mousebutton(const renderer2d &rend2d, const glm::vec2 &pos, i32 button, i32 act, i32 mod, T &comp) {
			glm::vec2 cpos = { OffX - ioff::x * comp.width(), yoff - ioff::y * comp.height() };
			glm::vec2 rp = glm::vec2(rend2d.proj(A) * glm::vec4(pos, 0, 1)) - cpos;
			if (comp.is_inside(rp))
				comp.template on_mousebutton(button, act, mod, rp);
			if constexpr (is_bottom_anchor<A>::value)
				yoff -= comp.height() + Style::margin;
			else
				yoff += comp.height() + Style::margin;
		}
	private:
		float yoff = OffY;
	};
	template<anchor2d A>
	struct anchor_only_pos { constexpr static anchor2d anchor = A; };
	template<typename ...Ts>
	struct sublayouts {};
	template<typename ST>
	struct sublayouts<ST> {
		template<typename Style, typename P, typename T> inline void render_solid(vk::CommandBuffer cmdbuf, const renderer2d &rend2d, const screen_render_info &sri, T &comp) {
			if constexpr (ST::anchor == P::anchor) sl.template render_solid<Style, P>(cmdbuf, rend2d, sri, comp);
		}
		template<typename Style, typename P, typename T> inline void render_text(vk::CommandBuffer cmdbuf, const renderer2d &rend2d, const screen_render_info &sri, T &comp) {
			if constexpr (ST::anchor == P::anchor) sl.template render_text<Style, P>(cmdbuf, rend2d, sri, comp);
		}
		template<typename Style, typename P, typename T> inline void on_mousebutton(const renderer2d &rend2d, const glm::vec2 &pos, i32 button, i32 act, i32 mod, T &comp) {
			if constexpr (ST::anchor == P::anchor) sublayouts<ST>::template on_mousebutton<Style, P, T>(rend2d, pos, button, act, mod, comp);
		}
	private:
		ST sl;
	};
	template<typename ST, typename ...STs>
	struct sublayouts<ST, STs...> : public sublayouts<STs...>, private sublayouts<ST> {
		template<typename Style, typename P, typename T> inline void render_solid(vk::CommandBuffer cmdbuf, const renderer2d &rend2d, const screen_render_info &sri, T &comp) {
			sublayouts<ST>::template render_solid<Style, P, T>(cmdbuf, rend2d, sri, comp);
			sublayouts<STs...>::template render_solid<Style, P, T>(cmdbuf, rend2d, sri, comp);
		}
		template<typename Style, typename P, typename T> inline void render_text(vk::CommandBuffer cmdbuf, const renderer2d &rend2d, const screen_render_info &sri, T &comp) {
			sublayouts<ST>::template render_text<Style, P, T>(cmdbuf, rend2d, sri, comp);
			sublayouts<STs...>::template render_text<Style, P, T>(cmdbuf, rend2d, sri, comp);
		}
		template<typename Style, typename P, typename T> inline void on_mousebutton(const renderer2d &rend2d, const glm::vec2 &pos, i32 button, i32 act, i32 mod, T &comp) {
			sublayouts<ST>::template on_mousebutton<Style, P, T>(rend2d, pos, button, act, mod, comp);
			sublayouts<STs...>::template on_mousebutton<Style, P, T>(rend2d, pos, button, act, mod, comp);
		}
	};
	template<typename Sublayouts, typename ...Ps>
	struct map_layout {
		template<typename Style, typename ...CompTs> inline void render_solid(vk::CommandBuffer cmdbuf, const renderer2d &rend2d, const screen_render_info &sri, components<CompTs...> &comps) {
			Sublayouts sl{};
			comps.template for_each_tz<Ps...>([&]<typename P, typename T>(T &comp) {
				sl.template render_solid<Style, P>(cmdbuf, rend2d, sri, comp);
			});
		}
		template<typename Style, typename ...CompTs> inline void render_text(vk::CommandBuffer cmdbuf, const renderer2d &rend2d, const screen_render_info &sri, components<CompTs...> &comps) {
			Sublayouts sl{};
			comps.template for_each_tz<Ps...>([&]<typename P, typename T>(T &comp) {
				sl.template render_text<Style, P>(cmdbuf, rend2d, sri, comp);
			});
		}
		template<typename Style, typename ...CompTs> inline void on_mousebutton(const renderer2d &rend2d, const glm::vec2 &pos, i32 button, i32 act, i32 mod, components<CompTs...> &comps) {
			Sublayouts sl{};
			comps.template for_each_tz<Ps...>([&]<typename P, typename T>(T &comp) {
				sl.template on_mousebutton<Style, P>(rend2d, pos, button, act, mod, comp);
			});
		}
	};
	
	template<typename Components, typename Layout, typename Style>
	class screen {
	public:
		multi_stager stager;
		screen_render_info rend_info;
		Components comps;

		inline screen(const renderer2d &rend2d, const cnst::rend::text &txt, const buffer_handler &buffh) : stager(buffh.get_multi_stager()), rend_info(buffh, txt) {
			rend_info.frame = 0;
			rend_info.stager = &stager;
			stager.prepare(rend_info.frame);
			comps.template init<Style>(rend2d, rend_info);
		}
		screen(const screen &) = delete;
		inline screen(screen &&s) : stager(std::forward<multi_stager>(s.stager)), rend_info(std::forward<screen_render_info>(s.rend_info)),
			comps(std::forward<Components>(s.comps)) { rend_info.stager = &stager; }
		inline void on_key(i32 key, i32 act, i32 mods) {
			if (act == GLFW_PRESS && !mods) {
				if (key == GLFW_KEY_TAB) {
					rend_info.focused = comps.get_focusable_id(rend_info.focused+1);
					if (!has_focus()) {
						rend_info.focused = comps.get_focusable_id();
					}
					return;
				}
				if (key == GLFW_KEY_ESCAPE) {
					rend_info.focused = screen_render_info::unfocused;
					return;
				}
			}
			if (has_focus()) {
				comps.on_key(rend_info.focused, key, act, mods);
			}
		}
		inline void on_char(u32 codepoint) {
			if (has_focus()) {
				comps.on_char(rend_info.focused, codepoint);
			}
		}
		inline void on_cursorpos(f64 x, f64 y) {
			rend_info.cursor_pos = {x, y};
		}
		inline void on_mousebutton(const renderer2d &rend2d, i32 button, i32 act, i32 mods) {
			Layout().template on_mousebutton<Style>(rend2d, rend_info.cursor_pos, button, act, mods, comps);
		}
		inline void preframe(vk::CommandBuffer cmdbuf) { stager.commit(cmdbuf); }
		inline void preframe(const vk::raii::Device &device, vk::CommandPool pool, vk::Queue queue) {
			autocommand cmd(device, pool, queue);
			stager.commit(*cmd);
		}
		inline void post(vk::CommandBuffer cmdbuf) {
			stager.prepare(rend_info.frame);
			comps.for_each([&]<typename T>(T &comp) { comp.template post<Style>(cmdbuf, rend_info); });
			stager.commit(cmdbuf, rend_info.frame);
			rend_info.frame = (rend_info.frame + 1) % multi_stager::framec;
		}
		inline void render_solid(vk::CommandBuffer cmdbuf, const renderer2d &rend2d) {
			Layout().template render_solid<Style>(cmdbuf, rend2d, rend_info, comps);
		}
		inline void render_text(vk::CommandBuffer cmdbuf, const renderer2d &rend2d) {
			Layout().template render_text<Style>(cmdbuf, rend2d, rend_info, comps);
		}
		inline bool has_focus() const {
			return rend_info.focused != screen_render_info::unfocused;
		}
	};
}
