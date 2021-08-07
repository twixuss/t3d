#pragma once
#include "font.h"
#include "blit.h"
#include "editor/current.h"
#include "assets.h"

struct ButtonTheme {
	v4f color = foreground_color;
	v4f hover_enter_color = foreground_color * highlight_color * 1.5f;
	v4f hover_stay_color = foreground_color * highlight_color;
	v4f press_color = {.1f, .1f, .1f, 1};
	v4f click_color = {.5f, .5f, 1, 1};
	f32 hover_enter_speed = 10;
	f32 hover_stay_speed = 20;
	f32 press_speed = 20;
	f32 click_speed = 10;
};

ButtonTheme default_button_theme;

struct TextFieldTheme {
	v4f color = background_color;
	v4f hovered_color = {.15f, .15f, .15f, 1};
	v4f edit_color = {.2f, .2f, .1f, 1};
};

TextFieldTheme default_text_field_theme;

struct GuiKey {
	umm id;
	std::source_location location;
	bool operator==(GuiKey const &that) {
		return id == that.id && location == that.location;
	}
};

template <>
umm get_hash(GuiKey key) {
	return key.id * (umm)954277 + key.location.column() * (umm)152753 + key.location.line() * (umm)57238693 + (umm)key.location.file_name();
}

struct ButtonState {
	f32 hover_enter_t;
	f32 hover_stay_t;
	f32 press_t;
	f32 click_t;
	bool previously_hovered;
};

template <class T>
struct FieldState {
	bool editing;
	List<utf8> string;
	f32 caret_blink_time;
	T original_value;
	u32 caret_position;
	u32 selection_start;
	s32 text_offset;
};

HashMap<GuiKey, ButtonState>            button_states;
HashMap<GuiKey, FieldState<f32>>        float_field_states;
HashMap<GuiKey, FieldState<List<utf8>>> text_field_states;


void blit(v4f color) {
	tg::set_rasterizer({
		.depth_test = false,
		.depth_write = false,
	});
	tg::set_shader(blit_color_shader);
	tg::set_blend(tg::BlendFunction_add, tg::Blend_source_alpha, tg::Blend_one_minus_source_alpha);
	tg::set_topology(tg::Topology_triangle_list);
	tg::set_shader_constants(blit_color_constants, 0);
	tg::update_shader_constants(blit_color_constants, {.color = color});
	tg::draw(3);
}

void blit(tg::Texture2D *texture) {
	tg::set_rasterizer({
		.depth_test = false,
		.depth_write = false,
	});
	tg::set_shader(blit_texture_shader);
	tg::set_blend(tg::BlendFunction_add, tg::Blend_source_alpha, tg::Blend_one_minus_source_alpha);
	tg::set_topology(tg::Topology_triangle_list);
	tg::set_texture(texture, 0);
	tg::draw(3);
}


u32 const font_size = 12;

struct DrawTextParams {
	v2s position = {};
};

void draw_text(Span<PlacedChar> placed_text, SizedFont *font, DrawTextParams params = {}) {
	if (!placed_text.size)
		return;

	struct Vertex {
		v2f position;
		v2f uv;
	};

	List<Vertex> vertices;
	vertices.allocator = temporary_allocator;

	for (auto &c : placed_text) {
		Span<Vertex> quad = {
			{{c.position.min.x, c.position.min.y}, {c.uv.min.x, c.uv.min.y}},
			{{c.position.max.x, c.position.min.y}, {c.uv.max.x, c.uv.min.y}},
			{{c.position.max.x, c.position.max.y}, {c.uv.max.x, c.uv.max.y}},
			{{c.position.min.x, c.position.max.y}, {c.uv.min.x, c.uv.max.y}},
		};
		vertices += {
			quad[1], quad[0], quad[2],
			quad[2], quad[0], quad[3],
		};
	}

	if (text_vertex_buffer) {
		tg::update_vertex_buffer(text_vertex_buffer, as_bytes(vertices));
	} else {
		text_vertex_buffer = tg::create_vertex_buffer(as_bytes(vertices), {
			tg::Element_f32x2, // position	
			tg::Element_f32x2, // uv	
		});
	}
	tg::set_rasterizer({.depth_test = false, .depth_write = false});
	tg::set_topology(tg::Topology_triangle_list);
	tg::set_blend(tg::BlendFunction_add, tg::Blend_secondary_color, tg::Blend_one_minus_secondary_color);
	tg::set_shader(text_shader);
	tg::set_shader_constants(text_shader_constants, 0);
	tg::update_shader_constants(text_shader_constants, {
		.inv_half_viewport_size = v2f{2,-2} / (v2f)current_viewport.size(),
		.offset = (v2f)params.position,
	});
	tg::set_vertex_buffer(text_vertex_buffer);
	tg::set_texture(font->texture, 0);
	tg::draw(vertices.size);
}
void draw_text(Span<utf8> string, DrawTextParams params = {}) {
	auto font = get_font_at_size(font_collection, font_size);
	ensure_all_chars_present(string, font);
	return draw_text(with(temporary_allocator, place_text(string, font)), font, params);
}
void draw_text(utf8 const *string, DrawTextParams params = {}) { draw_text(as_span(string), params); }
void draw_text(Span<char>  string, DrawTextParams params = {}) { draw_text((Span<utf8>)string, params); }
void draw_text(char const *string, DrawTextParams params = {}) { draw_text((Span<utf8>)as_span(string), params); }

bool button_base(umm id, ButtonTheme const &theme, std::source_location location) {
	auto &state = button_states.get_or_insert({id, location});
	
	bool result = mouse_click(0);
	if (result) {
		state.click_t = 1;
	}

	bool currently_hovered = in_bounds(current_mouse_position, current_scissor);
	if (currently_hovered && !state.previously_hovered) {
		state.hover_enter_t = 1;
	}

	state.press_t       = lerp<f32>(state.press_t,      (f32)mouse_held(0),     frame_time * theme.press_speed);
	state.hover_stay_t  = lerp<f32>(state.hover_stay_t, (f32)currently_hovered, frame_time * theme.hover_stay_speed);

	v4f color = theme.color;
	color = lerp(color, theme.hover_stay_color,  V4f(state.hover_stay_t));
	color = lerp(color, theme.hover_enter_color, V4f(state.hover_enter_t));
	color = lerp(color, theme.press_color,       V4f(state.press_t));
	color = lerp(color, theme.click_color,       V4f(state.click_t));
	blit(color);

	state.click_t       = lerp<f32>(state.click_t,       0, frame_time * theme.click_speed);
	state.hover_enter_t = lerp<f32>(state.hover_enter_t, 0, frame_time * theme.hover_enter_speed);

	state.previously_hovered = currently_hovered;

	return result;
}

bool button(Span<utf8> text, umm id = 0, ButtonTheme const &theme = default_button_theme, std::source_location location = std::source_location::current()) {
	auto result = button_base(id, theme, location);

	draw_text(text);

	return result;
}

bool button(tg::Texture2D *texture, umm id = 0, ButtonTheme const &theme = default_button_theme, std::source_location location = std::source_location::current()) {
	auto result = button_base(id, theme, location);
	
	blit(texture);
	
	return result;
}

bool button(tg::Viewport button_viewport, Span<utf8> text, umm id = 0, ButtonTheme const &theme = default_button_theme, std::source_location location = std::source_location::current()) {
	push_current_viewport(button_viewport) {
		return button(text, id, theme, location);
	}
	return false;
}
bool button(tg::Viewport button_viewport, tg::Texture2D *texture, umm id = 0, ButtonTheme const &theme = default_button_theme, std::source_location location = std::source_location::current()) {
	push_current_viewport(button_viewport) {
		return button(texture, id, theme, location);
	}
	return false;
}

Optional<f32> parse_f32(Span<utf8> string) {
	if (!string.size)
		return {};

	u64 whole_part = 0;
	auto c = string.data;
	auto end = string.end();

	bool negative = false;
	if (*c == '-') {
		negative = true;
		++c;
	}

	bool do_fract_part = false;
	while (1) {
		if (c == end)
			break;

		if (*c == '.' || *c == ',') {
			do_fract_part = true;
			break;
		}

		u32 digit = *c - '0';
		if (digit >= 10)
			return {};

		whole_part *= 10;
		whole_part += digit;

		++c;
	}

	u64 fract_part  = 0;
	u64 fract_denom = 1;

	if (do_fract_part) {
		++c;
		while (1) {
			if (c == end) {
				break;
			}

			u32 digit = *c - '0';
			if (digit >= 10)
				return {};

			fract_denom *= 10;
			fract_part *= 10;
			fract_part += digit;

			++c;
		}
	}

	f64 result = (f64)whole_part + (f64)fract_part / (f64)fract_denom;
	if (negative) {
		result = -result;
	}
	return result;
}

template <class Init, class Edit, class Drag>
struct InputFieldCallbacks {
	Init on_init;
	Edit on_edit;
	Drag on_drag;
};

template <class InputFieldCallbacks>
bool input_field(InputFieldCallbacks callbacks, auto &state, auto &value, auto &theme) {
	begin_input_user(true);

	bool value_changed = false;
	bool stop_edit = false;
	bool apply_input = false;
	bool set_caret_from_mouse = false;
	if (state.editing) {
		if ((key_state[256 + 0].state & KeyState_down)) {
			if (in_bounds(current_mouse_position, current_scissor)) {
				set_caret_from_mouse = true;
			} else {
				apply_input = true;
				stop_edit = true;
				key_state[256 + 0].start_position = current_mouse_position;
			}
		}
		if ((key_state[256 + 1].state & KeyState_down)) {
			apply_input = true;
			stop_edit = true;
		}
		if (mouse_click(0)) {
			print("click\n");
		}
		if (key_down(Key_escape)) {
			stop_edit = true;
		}
	} else {
		if (mouse_begin_drag(0)) {
			lock_input();
		}
		if (mouse_drag(0)) {
			callbacks.on_drag(window->mouse_delta.x);
			value_changed = true;
		}
		if (mouse_end_drag(0)) {
			unlock_input();
		}
	}
	if (mouse_click(0) || should_focus()) {
		lock_input();
		
		apply_input = false;
		stop_edit = false;

		free(state.string);
		state.editing = true;
		callbacks.on_init();
		state.caret_blink_time = 0;
		state.selection_start = 0;
		state.caret_position = state.string.size;
	}

	if (state.editing) {
		if (key_state[Key_left_arrow].state & KeyState_repeated) {
			if (state.caret_position) {
				if (state.selection_start == state.caret_position) {
					state.caret_position -= 1;
				} else {
					if (key_held(Key_shift)) {
						state.caret_position -= 1;
					} else {
						state.selection_start = state.caret_position = min(state.selection_start, state.caret_position);
					}
				}
			}
			state.caret_blink_time = 0;
			if (!key_held(Key_shift)) {
				state.selection_start = state.caret_position;
			}
		}
		if (key_state[Key_right_arrow].state & KeyState_repeated) {
			if (state.caret_position != state.string.size) {
				if (state.selection_start == state.caret_position) {
					state.caret_position += 1;
				} else {
					if (key_held(Key_shift)) {
						state.caret_position += 1;
					} else {
						state.selection_start = state.caret_position = max(state.selection_start, state.caret_position);
					}
				}
			}
			state.caret_blink_time = 0;
			if (!key_held(Key_shift)) {
				state.selection_start = state.caret_position;
			}
		}
		if (key_down(Key_home)) {
			state.caret_position = 0;
			if (!key_held(Key_shift)) {
				state.selection_start = state.caret_position;
			}
		}
		if (key_down(Key_end)) {
			state.caret_position = state.string.size;
			if (!key_held(Key_shift)) {
				state.selection_start = state.caret_position;
			}
		}

		enum Erase {
			Erase_none,
			Erase_backspace,
			Erase_delete,
		};

		Erase erase = Erase_none;

		for (auto c : input_string) {
			switch (c) {
				case '\b': {
					erase = Erase_backspace;
					break;
				}
				case '\r': {
					break;
				}
				case 1: { // Control + A
					state.selection_start = 0;
					state.caret_position = state.string.size;
					break;
				}
				case 3: { // Control + C
					u32 sel_min, sel_max;
					minmax(state.caret_position, state.selection_start, sel_min, sel_max);
					if (sel_max == sel_min) {
						sel_min = 0;
						sel_max = state.string.size;
					}
					if (sel_max != sel_min) {
						set_clipboard(window, Clipboard_text, Span<utf8>{state.string.data + sel_min, sel_max - sel_min});
					}
					break;
				}
				case 22: { // Control + V
					auto clipboard = with(temporary_allocator, (List<utf8>)get_clipboard(window, Clipboard_text));
					defer { free(clipboard); };

					u32 new_caret_position;
					if (state.caret_position == state.selection_start) {
						state.string.insert_at(clipboard, state.caret_position);
						new_caret_position = state.caret_position + clipboard.size;
					} else {
						u32 sel_min, sel_max;
						minmax(state.caret_position, state.selection_start, sel_min, sel_max);
						state.string.replace({state.string.data + sel_min, sel_max - sel_min}, clipboard);
						new_caret_position = sel_min + clipboard.size;
					}
					state.caret_position = state.selection_start = new_caret_position;
					break;
				}
				case 127: { // Control + Backspace
					if (state.caret_position != 0) {
						u32 erase_start = state.caret_position - 1;
						while (erase_start != -1 && state.string[erase_start] != ' ') {
							erase_start -= 1;
						}
						if (erase_start + 1 != state.caret_position)
							erase_start += 1;
						state.string.erase({state.string.data + erase_start, state.caret_position - erase_start});
						state.caret_position = state.selection_start = erase_start;
					}
					break;
				}
				case '\t': {
					stop_edit = true;
					apply_input = true;
					if (key_state[Key_shift].state & KeyState_held) {
						should_switch_focus_to = focusable_input_user_index - 1;
					} else {
						should_switch_focus_to = focusable_input_user_index + 1;
					}
					break;
				}
				default: {
					if (state.caret_position == state.selection_start) {
						state.string.insert_at(c, state.caret_position);
						state.caret_position += 1;
						state.caret_blink_time = 0;
						//if (!key_held(Key_shift)) 
						state.selection_start = state.caret_position;
					} else {
						u32 sel_min, sel_max;
						minmax(state.caret_position, state.selection_start, sel_min, sel_max);
						state.string.replace({state.string.data + sel_min, sel_max - sel_min}, c);
						state.caret_position = state.selection_start = sel_min + 1;
					}
					break;
				}
			}
		}
		input_string.clear();

		if (key_state[Key_delete].state & KeyState_repeated) {
			erase = Erase_delete;
		}


		if (erase != Erase_none) {
			if (state.caret_position == state.selection_start) {
				if (erase == Erase_backspace) {
					// Remove one character to the left of the caret
					if (state.string.size && state.caret_position) {
						state.selection_start = state.caret_position -= 1;
						state.string.erase_at(state.caret_position);
						state.caret_blink_time = 0;
					}
				} else  {
					// Remove one character to the right of the caret
					if (state.string.size && state.caret_position != state.string.size) {
						state.string.erase_at(state.caret_position);
						state.caret_blink_time = 0;
					}
				}
			} else {
				// Remove selection
				u32 sel_min, sel_max;
				minmax(state.caret_position, state.selection_start, sel_min, sel_max);

				state.string.erase({state.string.data + sel_min, sel_max - sel_min});
				state.caret_blink_time = 0;
				state.caret_position = state.selection_start = sel_min;
			}
		}

		if (key_down(Key_enter)) {
			apply_input = true;
			stop_edit = true;
		}

		value_changed = callbacks.on_edit();
	}

	if (apply_input || stop_edit) {
		unlock_input();
		state.editing = false;
	}

	if (stop_edit && !apply_input) {
		value = state.original_value;	
		value_changed = true;
	}

	v4f color = theme.color;
	if (state.editing) {
		color = theme.edit_color;
	} else if (in_bounds(current_mouse_position, current_scissor)) {
		color = theme.hovered_color;
	}
			
	blit(color);

	if (state.editing) {
		tg::Viewport caret_viewport = current_viewport;
		caret_viewport.min.x = current_viewport.min.x;
		caret_viewport.max.x = caret_viewport.min.x + 1;


		if (state.string.size) {
			auto font = get_font_at_size(font_collection, font_size);
			ensure_all_chars_present(state.string, font);
			auto placed_chars = with(temporary_allocator, place_text(state.string, font));

			if (set_caret_from_mouse) {
				u32 new_caret_position = placed_chars.size;

				for (u32 char_index = 0; char_index < placed_chars.size; char_index += 1) {
					if (placed_chars[char_index].position.center().x - state.text_offset > (current_mouse_position.x - current_viewport.min.x)) {
						new_caret_position = char_index;
						break;
					}
				}

				state.caret_blink_time = 0;
				state.caret_position = state.selection_start = new_caret_position;
			}

			s32 caret_x = 0;
			if (state.caret_position != 0) {
				caret_x = placed_chars[state.caret_position - 1].position.max.x;
			}
			if (caret_x == 0)
				state.text_offset = 0;
			
			if (placed_chars.back().position.max.x > current_viewport.size().x) {
				state.text_offset = max(state.text_offset, caret_x - (s32)(current_viewport.size().x * 3 / 4));
				state.text_offset = min(state.text_offset, caret_x - (s32)(current_viewport.size().x * 1 / 4));
				state.text_offset = min(state.text_offset, (s32)placed_chars.back().position.max.x - current_viewport.size().x + 1);
				state.text_offset = max(state.text_offset, 0);
				//state.text_offset = min<s32>(state.text_offset, caret_x - (s32)(current_viewport.size().x * 1 / 4));

				//s32 rel_caret_pos = caret_x - state.text_offset;
				//s32 diff = rel_caret_pos - (s32)(current_viewport.size().x * 3 / 4);
				//if (diff < 0) {
				//	state.text_offset += diff;
				//}
				//
				//rel_caret_pos = caret_x - state.text_offset;
				//diff = rel_caret_pos - (s32)(current_viewport.size().x / 4);
				//if (diff > 0) {
				//	state.text_offset -= diff;
				//}


				print("%\n", state.text_offset);

				//state.text_offset = clamp<s32>(
				//	(s32)current_viewport.size().x / 4 - (s32)caret_x, 
				//	0, 
				//	-placed_chars.back().position.max.x + current_viewport.size().x - 1
				//);
			}

			caret_viewport.min.x += caret_x - state.text_offset;
			caret_viewport.max.x += caret_x - state.text_offset;

			if (state.caret_position != state.selection_start) {
				u32 sel_min, sel_max;
				minmax(state.caret_position, state.selection_start, sel_min, sel_max);

				u32 min_x = placed_chars[sel_min    ].position.min.x;
				u32 max_x = placed_chars[sel_max - 1].position.max.x;

				tg::Viewport selection_viewport = current_viewport;
				selection_viewport.min.x = current_viewport.min.x + min_x;
				selection_viewport.max.x = selection_viewport.min.x + max_x - min_x;

				push_current_viewport(selection_viewport) blit({0.25f,0.25f,0.5f,1});
			}
		
			draw_text(placed_chars, font, {.position = {-state.text_offset, 0}});
		}
		



		if (state.caret_blink_time <= 0.5f) {
			push_current_viewport (caret_viewport) {
				blit({1, 1, 1, 1});
			}
		}
		state.caret_blink_time += frame_time;
		if (state.caret_blink_time >= 1) {
			state.caret_blink_time -= 1;
		}
	} else {
		draw_text((List<utf8>)to_string(value));
	}

	return value_changed;
}

using FloatFieldTokenKind = u16;
enum : FloatFieldTokenKind {
	FloatFieldToken_number = 0x100,
	FloatFieldToken_pi,
	FloatFieldToken_e,
	FloatFieldToken_tau,
};

struct FloatFieldToken {
	FloatFieldTokenKind kind;
	f64 value;
};

Optional<f64> parse_expression(FloatFieldToken *&t, FloatFieldToken *end) {
	if (t == end)
		return {};
	bool negative = false;
	while (1) {
		if (t->kind == '-') {
			negative = !negative;	
			++t;
			if (t == end)
				break;
		} else if (t->kind == '+') {
			++t;
			if (t == end)
				break;
		} else {
			break;
		}
	}
	if (t == end)
		return {};

	if (t->kind != FloatFieldToken_number) {
		return {};
	}

	f64 result = t->value;
	if (negative) {
		result = -result;
	}

	++t;
	if (t != end) {
		if (t->kind == '+' || t->kind == '-' || t->kind == '*' || t->kind == '/') {
			auto op = t->kind;
			++t;

			auto right = parse_expression(t, end);

			if (right) {
				switch (op) {
					case '+': result += right.value; break;
					case '-': result -= right.value; break;
					case '*': result *= right.value; break;
					case '/': result /= right.value; break;
				}
			}
		}
	}

	return result;
}

bool float_field(f32 &value, umm id = 0, TextFieldTheme const &theme = default_text_field_theme, std::source_location location = std::source_location::current()) {
	auto &state = float_field_states.get_or_insert({id, location});

	return input_field(InputFieldCallbacks{
		.on_init = [&]() {
			state.string = with(default_allocator, (List<utf8>)to_string(FormatFloat{.value = value, .precision = 6}));
			state.original_value = value;
		},
		.on_edit = [&] {
			if (!state.string.size) {
				value = 0;
				return true;
			}

			List<FloatFieldToken> tokens;
			tokens.allocator = temporary_allocator;

			auto c = state.string.data;
			auto end = state.string.end();

			while (c != end) {
				if (is_alpha(*c) || *c == '_') {
					FloatFieldToken token;
					token.kind = FloatFieldToken_number;
					Span<utf8> token_string;
					token_string.data = c;

					c += 1;
					while (c != end && (is_alpha(*c) || *c == '_' || is_digit(*c))) {
						c += 1;
					}

					token_string.size = c - token_string.data;
			
					if (token_string == u8"pi"s) {
						token.value = pi;
					} else if (token_string == u8"tau"s) {
						token.value = tau;
					} else if (token_string == u8"e"s) {
						token.value = 2.71828182846;
					} else {
						return false;
					}

					tokens.add(token);
				} else if (is_digit(*c)) {
					FloatFieldToken token;
					token.kind = FloatFieldToken_number;

					Span<utf8> token_string;
					token_string.data = c;

					c += 1;
					while (c != end && (is_digit(*c))) {
						c += 1;
					}

					if (c != end) {
						if (*c == '.') {
							c += 1;
							while (c != end && (is_digit(*c))) {
								c += 1;
							}
						}
					}

					token_string.size = c - token_string.data;
					auto parsed = parse_f32(token_string);
					if (!parsed) {
						return false;
					}
					token.value = parsed.value;
					tokens.add(token);
				} else {
					switch (*c) {
						case '+':
						case '-':
						case '*':
						case '/': {
							FloatFieldToken token;
							token.kind = *c;
							tokens.add(token);
							c += 1;
							break;
						}
						default: {
							return false;
						}
					}
				}
			}

			{
				auto t = tokens.data;
				auto end = tokens.end();

				auto parsed = parse_expression(t, end);
				if (parsed || state.string.size == 0) {
					value = parsed ? parsed.value : 0;
					return true;
				}
			}
			return false;
		},
		.on_drag = [&](f32 delta){
			value += delta * (key_held(Key_shift, {.anywhere = true}) ? 0.01f : 0.1f);
		},
	}, state, value, theme);
}

bool text_field(List<utf8> &value, umm id = 0, TextFieldTheme const &theme = default_text_field_theme, std::source_location location = std::source_location::current()) {
	auto &state = text_field_states.get_or_insert({id, location});
	
	return input_field(InputFieldCallbacks{
		.on_init = [&]() {
			state.string = copy(value);
			state.original_value = copy(value);
		},
		.on_edit = [&] {
			value.set(state.string);
			return true;
		},
		.on_drag = [&](f32 delta){
		},
	}, state, value, theme);
}

s32 current_property_y;
s32 const line_height = 16;

void header(Span<utf8> text) {
	tg::Viewport line_viewport = current_viewport;
	line_viewport.min.y = current_viewport.max.y - line_height - current_property_y;
	line_viewport.max.y = line_viewport.min.y + line_height;
	push_current_viewport(line_viewport) draw_text(text);
	current_property_y += line_height + 2;
}

void property_separator() {
	s32 const separator_height = 2;
	tg::Viewport line_viewport = current_viewport;
	line_viewport.min.y = current_viewport.max.y - separator_height - current_property_y;
	line_viewport.max.y = line_viewport.min.y + separator_height;
	push_current_viewport(line_viewport) blit({.05, .05, .05, 1});
	current_property_y += separator_height + 2;
}

u64 get_id(u64 id, std::source_location location) {
	return ((u64)location.file_name() << 48) + ((u64)location.line() << 32) + ((u64)location.column() << 16) + id;
}

void draw_property(Span<utf8> name, f32 &value, u64 id, std::source_location location) {
	tg::Viewport line_viewport = current_viewport;
	line_viewport.min.y = current_viewport.max.y - line_height - current_property_y;
	line_viewport.max.y = line_viewport.min.y + line_height;

	push_current_viewport(line_viewport) {
		s32 text_width = 0;

		auto font = get_font_at_size(font_collection, font_size);
		ensure_all_chars_present(name, font);
		auto placed_text = with(temporary_allocator, place_text(name, font));
		text_width = placed_text.back().position.max.x;
		draw_text(placed_text, font);

		auto value_viewport = line_viewport;
		value_viewport.min.x += text_width + 2;

		push_current_viewport(value_viewport) {
			float_field(value, get_id(id, location));
		}
	}
	
	current_property_y += line_height + 2;
}

void draw_property(Span<utf8> name, v3f &value, u64 id, std::source_location location) {
	header(name);

	auto line_viewport = current_viewport;
	line_viewport.min.y = line_viewport.max.y - line_height - current_property_y;
	line_viewport.max.y = line_viewport.min.y + line_height;


	auto x_viewport = line_viewport;
	auto y_viewport = line_viewport;
	auto z_viewport = line_viewport;

	x_viewport.max.x = x_viewport.min.x + line_viewport.size().x / 3;

	y_viewport.min.x = x_viewport.max.x;
	y_viewport.max.x = x_viewport.min.x + line_viewport.size().x * 2 / 3;

	z_viewport.min.x = y_viewport.max.x;
	z_viewport.max.x = line_viewport.max.x;

	push_current_viewport(x_viewport) draw_text("X");
	push_current_viewport(y_viewport) draw_text("Y");
	push_current_viewport(z_viewport) draw_text("Z");
	
	x_viewport.min.x += font_size;
	y_viewport.min.x += font_size;
	z_viewport.min.x += font_size;

	push_current_viewport(x_viewport) float_field(value.x, get_id(id, location));
	push_current_viewport(y_viewport) float_field(value.y, get_id(id, location));
	push_current_viewport(z_viewport) float_field(value.z, get_id(id, location));

	current_property_y += line_height + 2;
}

void draw_property(Span<utf8> name, quaternion &value, u64 id, std::source_location location) {
	v3f angles = degrees(to_euler_angles(value));
	draw_property(name, angles, id,location);
	value = quaternion_from_euler(radians(angles));
}

void draw_property(Span<utf8> name, List<utf8> &value, u64 id, std::source_location location) {
	header(name);

	tg::Viewport line_viewport = current_viewport;
	line_viewport.min.y = line_viewport.max.y - line_height - current_property_y;
	line_viewport.max.y = line_viewport.min.y + line_height;

	push_current_viewport(line_viewport) text_field(value, get_id(id, location));

	current_property_y += line_height + 2;
}

template <class Fn>
void draw_asset_property(Span<utf8> name, Span<utf8> path, u64 id, std::source_location location, Span<Span<utf8>> file_extensions, Fn &&update) {
	tg::Viewport line_viewport = current_viewport;
	line_viewport.min.y = current_viewport.max.y - line_height - current_property_y;
	line_viewport.max.y = line_viewport.min.y + line_height;
	push_current_viewport(line_viewport) {
		s32 text_width;

		auto font = get_font_at_size(font_collection, font_size);
		ensure_all_chars_present(name, font);
		auto placed_text = with(temporary_allocator, place_text(name, font));
		text_width = placed_text.back().position.max.x;
		draw_text(placed_text, font);

		auto value_viewport = current_viewport;
		value_viewport.min.x += text_width + 2;

		push_current_viewport(value_viewport) {
			blit({.05, .05, .05, 1});
			draw_text(path);
	
			bool result = (key_state[256 + 0].state & KeyState_up) && in_bounds(current_mouse_position, current_scissor);

			if (result) {
				int x = 5;
			}

			if (drag_and_drop_kind == DragAndDrop_file) {
				auto dragging_path = as_utf8(drag_and_drop_data);
				auto found = find_if(file_extensions, [&](Span<utf8> extension) {
					return ends_with(dragging_path, extension);
				});

				if (found) {
					blit({0.1, 1.0, 0.1, 0.2});
				}
			}
			if (accept_drag_and_drop(DragAndDrop_file)) {
				update(as_utf8(drag_and_drop_data));
			}
		}
	}
	
	current_property_y += line_height + 2;
}

void draw_property(Span<utf8> name, Texture2D *&value, u64 id, std::source_location location) {
	Span<utf8> extensions[] = {
		u8".png"s,
		u8".jpg"s,
		u8".hdr"s,
	};
	draw_asset_property(name, value ? value->name : u8"null"s, id, location, extensions, [&] (Span<utf8> path) {
		auto new_texture = assets.textures_2d.get(path);	
		if (new_texture) {
			value = new_texture;
		}
	});
}

void draw_property(Span<utf8> name, Mesh *&value, u64 id, std::source_location location) {
	Span<utf8> extensions[] = {
		u8"idk"s
	};
	draw_asset_property(name, value ? value->name : u8"null"s, id, location, extensions, [&] (Span<utf8> path) {
		
	});
}
