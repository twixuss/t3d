#pragma once
#include "font.h"
#include "blit.h"
#include "editor/current.h"
#include "assets.h"

struct ButtonTheme {
	v4f color = {.18f, .18f, .18f, 1};
	v4f hovered_color = {.25f, .25f, .25f, 1};
	v4f pressed_color = {.1f, .1f, .1f, 1};
};

ButtonTheme default_button_theme;

struct TextFieldTheme {
	v4f color = {.05f, .05f, .05f, 1};
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

umm get_hash(GuiKey const &key) {
	return key.id * (umm)954277 + key.location.column() * (umm)152753 + key.location.line() * (umm)57238693 + (umm)key.location.file_name();
}

template <class T>
struct FieldState {
	bool editing;
	List<utf8> string;
	f32 caret_blink_time;
	T original_value;
	u32 caret_position;
	u32 selection_start;
};

HashMap<GuiKey, FieldState<f32>>        float_field_states;
HashMap<GuiKey, FieldState<List<utf8>>> text_field_states;


void blit(v4f color) {
	t3d::set_rasterizer({
		.depth_test = false,
		.depth_write = false,
	});
	t3d::set_shader(blit_color_shader);
	t3d::set_blend(t3d::BlendFunction_add, t3d::Blend_source_alpha, t3d::Blend_one_minus_source_alpha);
	t3d::set_topology(t3d::Topology_triangle_list);
	t3d::set_shader_constants(blit_color_constants, 0);
	t3d::update_shader_constants(blit_color_constants, {.color = color});
	t3d::draw(3);
}

void blit(t3d::Texture *texture) {
	t3d::set_rasterizer({
		.depth_test = false,
		.depth_write = false,
	});
	t3d::set_shader(blit_texture_shader);
	t3d::set_blend(t3d::BlendFunction_add, t3d::Blend_source_alpha, t3d::Blend_one_minus_source_alpha);
	t3d::set_topology(t3d::Topology_triangle_list);
	t3d::set_texture(texture, 0);
	t3d::draw(3);
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
		t3d::update_vertex_buffer(text_vertex_buffer, as_bytes(vertices));
	} else {
		text_vertex_buffer = t3d::create_vertex_buffer(as_bytes(vertices), {
			t3d::Element_f32x2, // position	
			t3d::Element_f32x2, // uv	
		});
	}
	t3d::set_rasterizer({.depth_test = false, .depth_write = false});
	t3d::set_topology(t3d::Topology_triangle_list);
	t3d::set_blend(t3d::BlendFunction_add, t3d::Blend_secondary_color, t3d::Blend_one_minus_secondary_color);
	t3d::set_shader(text_shader);
	t3d::set_shader_constants(text_shader_constants, 0);
	t3d::update_shader_constants(text_shader_constants, {
		.inv_half_viewport_size = v2f{2,-2} / (v2f)current_viewport.size(),
		.offset = (v2f)params.position,
	});
	t3d::set_vertex_buffer(text_vertex_buffer);
	t3d::set_texture(font->texture, 0);
	t3d::draw(vertices.size);
}
void draw_text(Span<utf8> string, DrawTextParams params = {}) {
	auto font = get_font_at_size(font_collection, font_size);
	ensure_all_chars_present(string, font);
	return draw_text(with(temporary_allocator, place_text(string, font)), font, params);
}
void draw_text(utf8 const *string, DrawTextParams params = {}) { draw_text(as_span(string), params); }
void draw_text(Span<char>  string, DrawTextParams params = {}) { draw_text((Span<utf8>)string, params); }
void draw_text(char const *string, DrawTextParams params = {}) { draw_text((Span<utf8>)as_span(string), params); }

bool button(Span<utf8> text, ButtonTheme const &theme = default_button_theme) {
	v4f color = theme.color;
	if (mouse_held(0)) {
		color = theme.pressed_color;
	} else if (in_bounds(current_mouse_position, current_scissor)) {
		color = theme.hovered_color;
	}
			
	blit(color);
	draw_text(text);
		
	return mouse_click(0);
}

bool button(t3d::Texture *texture, ButtonTheme const &theme = default_button_theme) {
	v4f color = theme.color;
	if (mouse_held(0)) {
		color = theme.pressed_color;
	} else if (in_bounds(current_mouse_position, current_scissor)) {
		color = theme.hovered_color;
	}
	
	blit(color);
	blit(texture);
		
	return mouse_click(0);
}

bool button(t3d::Viewport button_viewport, Span<utf8> text, ButtonTheme const &theme = default_button_theme) {
	push_current_viewport(button_viewport) {
		return button(text, theme);
	}
	return false;
}
bool button(t3d::Viewport button_viewport, t3d::Texture *texture, ButtonTheme const &theme = default_button_theme) {
	push_current_viewport(button_viewport) {
		return button(texture, theme);
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

bool edit_float(f32 &value, umm id = 0, TextFieldTheme const &theme = default_text_field_theme, std::source_location location = std::source_location::current()) {
	begin_input_user(true);

	auto &state = float_field_states.get_or_insert({id, location});

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
			value += window->mouse_delta.x * (key_held(Key_shift, {.anywhere = true}) ? 0.01f : 0.1f);
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
		state.string = with(default_allocator, (List<utf8>)to_string(FormatFloat{.value = value, .precision = 6}));
		state.caret_blink_time = 0;
		state.original_value = value;
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
						if (!key_held(Key_shift)) state.selection_start = state.caret_position;
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

		auto parsed = parse_f32(state.string);
		if (parsed || state.string.size == 0) {
			value = parsed ? parsed.value : 0;
			value_changed = true;
		}
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
		t3d::Viewport caret_viewport = current_viewport;
		caret_viewport.min.x = current_viewport.min.x;
		caret_viewport.max.x = caret_viewport.min.x + 1;


		if (state.string.size) {
			auto font = get_font_at_size(font_collection, font_size);
			ensure_all_chars_present(state.string, font);
			auto placed_chars = with(temporary_allocator, place_text(state.string, font));

			if (set_caret_from_mouse) {
				u32 new_caret_position = placed_chars.size;

				for (u32 char_index = 0; char_index < placed_chars.size; char_index += 1) {
					if (placed_chars[char_index].position.center().x > (current_mouse_position.x - current_viewport.min.x)) {
						new_caret_position = char_index;
						break;
					}
				}

				state.caret_blink_time = 0;
				state.caret_position = state.selection_start = new_caret_position;
			}

			u32 caret_x = 0;
			if (state.caret_position != 0) {
				caret_x = placed_chars[state.caret_position - 1].position.max.x;
			}

			v2s text_offset = {};
				
			s32 offset = 0;
			if (placed_chars.back().position.max.x > current_viewport.size().x) {
				offset = clamp<s32>(
					(s32)current_viewport.size().x / 2 - (s32)caret_x, 
					0, 
					-placed_chars.back().position.max.x + current_viewport.size().x - 1
				);
			}

			text_offset.x = offset;

			caret_viewport.min.x += offset + caret_x;
			caret_viewport.max.x += offset + caret_x;

			if (state.caret_position != state.selection_start) {
				u32 sel_min, sel_max;
				minmax(state.caret_position, state.selection_start, sel_min, sel_max);

				u32 min_x = placed_chars[sel_min    ].position.min.x;
				u32 max_x = placed_chars[sel_max - 1].position.max.x;

				t3d::Viewport selection_viewport = current_viewport;
				selection_viewport.min.x = current_viewport.min.x + min_x;
				selection_viewport.max.x = selection_viewport.min.x + max_x - min_x;

				push_current_viewport(selection_viewport) blit({0.25f,0.25f,0.5f,1});
			}
		
			draw_text(placed_chars, font, {.position = text_offset});
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

void text_field(List<utf8> &value, umm id = 0, TextFieldTheme const &theme = default_text_field_theme, std::source_location location = std::source_location::current()) {
	auto &state = text_field_states.get_or_insert({id, location});
	
	bool stop_edit = false;
	bool apply_input = false;
	if (state.editing) {
		if (mouse_down_no_lock(0, {.anywhere = true})) {
			apply_input = true;
			stop_edit = true;
		}
		if (mouse_down_no_lock(1, {.anywhere = true})) {
			stop_edit = true;
		}
	}
	if (mouse_click(0)) {
		lock_input();
		
		apply_input = false;
		stop_edit = false;

		free(state.string);
		free(state.original_value);

		state.editing = true;
		state.string = copy(value);
		state.caret_blink_time = 0;
		state.original_value = copy(value);
		state.selection_start = 0;
		state.caret_position = state.string.size;
	}

	if (state.editing) {
		if (key_repeat(Key_left_arrow)) {
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
		if (key_repeat(Key_right_arrow)) {
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
				default: {
					if (state.caret_position == state.selection_start) {
						state.string.insert_at(c, state.caret_position);
						state.caret_position += 1;
						state.caret_blink_time = 0;
						if (!key_held(Key_shift)) state.selection_start = state.caret_position;
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
		if (key_repeat(Key_delete)) {
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

		value.set(state.string);
	}

	if (apply_input || stop_edit) {
		unlock_input();
		state.editing = false;
	}

	if (stop_edit && !apply_input) {
		value.set(state.original_value);
	}

	v4f color = theme.color;
	if (state.editing) {
		color = theme.edit_color;
	} else if (in_bounds(current_mouse_position, current_scissor)) {
		color = theme.hovered_color;
	}
			
	blit(color);

	if (state.editing) {
		t3d::Viewport caret_viewport = current_viewport;
		caret_viewport.min.x = current_viewport.min.x;
		caret_viewport.max.x = caret_viewport.min.x + 1;


		if (state.string.size) {
			auto font = get_font_at_size(font_collection, font_size);
			ensure_all_chars_present(state.string, font);
			auto placed_chars = with(temporary_allocator, place_text(state.string, font));

			u32 caret_x = 0;
			if (state.caret_position != 0) {
				caret_x = placed_chars[state.caret_position - 1].position.max.x;
			}
		
			caret_viewport.min.x += caret_x;

			if (state.caret_position != state.selection_start) {
				u32 sel_min, sel_max;
				minmax(state.caret_position, state.selection_start, sel_min, sel_max);

				u32 min_x = placed_chars[sel_min    ].position.min.x;
				u32 max_x = placed_chars[sel_max - 1].position.max.x;

				t3d::Viewport selection_viewport = current_viewport;
				selection_viewport.min.x = current_viewport.min.x + min_x;
				selection_viewport.max.x = selection_viewport.min.x + max_x - min_x;

				push_current_viewport(selection_viewport) blit({0.25f,0.25f,0.5f,1});
			}
		
			draw_text(placed_chars, font);
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

}

s32 current_property_y;
s32 const line_height = 16;

void header(Span<utf8> text) {
	t3d::Viewport line_viewport = current_viewport;
	line_viewport.min.y = current_viewport.max.y - line_height - current_property_y;
	line_viewport.max.y = line_viewport.min.y + line_height;
	push_current_viewport(line_viewport) draw_text(text);
	current_property_y += line_height + 2;
}

void property_separator() {
	s32 const separator_height = 2;
	t3d::Viewport line_viewport = current_viewport;
	line_viewport.min.y = current_viewport.max.y - separator_height - current_property_y;
	line_viewport.max.y = line_viewport.min.y + separator_height;
	push_current_viewport(line_viewport) blit({.05, .05, .05, 1});
	current_property_y += separator_height + 2;
}

void draw_property(Span<utf8> name, f32 &value, std::source_location location) {
	t3d::Viewport line_viewport = current_viewport;
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
			edit_float(value, (umm)location.line() * (umm)location.file_name());
		}
	}
	
	current_property_y += line_height + 2;
}

void draw_property(Span<utf8> name, v3f &value, std::source_location location) {
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

	push_current_viewport(x_viewport) edit_float(value.x, (umm)location.line() * (umm)location.file_name());
	push_current_viewport(y_viewport) edit_float(value.y, (umm)location.line() * (umm)location.file_name());
	push_current_viewport(z_viewport) edit_float(value.z, (umm)location.line() * (umm)location.file_name());

	current_property_y += line_height + 2;
}

void draw_property(Span<utf8> name, quaternion &value, std::source_location location) {
	header(name);

	v3f angles = degrees(to_euler_angles(value));
	draw_property(name, angles, location);
	value = quaternion_from_euler(radians(angles));
}

void draw_property(Span<utf8> name, List<utf8> &value, std::source_location location) {
	header(name);

	t3d::Viewport line_viewport = current_viewport;
	line_viewport.min.y = line_viewport.max.y - line_height - current_property_y;
	line_viewport.max.y = line_viewport.min.y + line_height;

	push_current_viewport(line_viewport) text_field(value, (umm)location.line() * (umm)location.file_name());

	current_property_y += line_height + 2;
}

template <class Fn>
void draw_asset_property(Span<utf8> name, Span<utf8> path, std::source_location location, Fn &&update) {
	t3d::Viewport line_viewport = current_viewport;
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

			if (accept_drag_and_drop()) {
				update(get_drag_and_drop_file());
			}
		}
	}
	
	current_property_y += line_height + 2;
}

void draw_property(Span<utf8> name, Texture *&value, std::source_location location) {
	draw_asset_property(name, value ? value->name : u8"null"s, location, [&] (Span<utf8> path) {
		auto new_texture = assets.textures.get(path);	
		if (new_texture) {
			value = new_texture;
		}
	});
}

void draw_property(Span<utf8> name, Mesh *&value, std::source_location location) {
	draw_asset_property(name, value ? value->name : u8"null"s, location, [&] (Span<utf8> path) {
		
	});
}
