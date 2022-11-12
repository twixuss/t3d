#pragma once
#include <t3d/font.h>
#include <t3d/blit.h>
#include <t3d/assets.h>
#include <t3d/editor/current.h>
#include <t3d/editor/window.h>
#include <t3d/editor/input.h>
#include <t3d/app.h>
#include <t3d/editor.h>

inline tg::Rect pad(tg::Rect viewport) {
	viewport.min += 2;
	viewport.max -= 2;
	return viewport;
}

u32 const font_size = 12;

void gui_panel(v4f color);
void gui_image(tg::Texture2D *texture);

enum Align {
	Align_top_left,
	Align_top,
	Align_top_right,
	Align_left,
	Align_center,
	Align_right,
	Align_bottom_left,
	Align_bottom,
	Align_bottom_right,
};
struct DrawTextParams {
	v2s position = {};
	Align align = Align_left;
};

u32 get_font_size(u32 font_size);

void label(v2s position, List<PlacedChar> placed_chars, SizedFont *font, v4f color);
void label(Span<utf8> string, u32 font_size, DrawTextParams params = {});
inline void label(utf8 const *string, u32 font_size, DrawTextParams params = {}) { return label(as_span(string), font_size, params); }
inline void label(Span<char>  string, u32 font_size, DrawTextParams params = {}) { return label((Span<utf8>)string, font_size, params); }
inline void label(char const *string, u32 font_size, DrawTextParams params = {}) { return label((Span<utf8>)as_span(string), font_size, params); }

bool button(umm id = 0, std::source_location location = std::source_location::current());
bool button(Span<utf8> text, umm id = 0, std::source_location location = std::source_location::current());
bool button(tg::Texture2D *texture, umm id = 0, std::source_location location = std::source_location::current());

template <class Init, class Edit, class Drag>
struct InputFieldCallbacks {
	Init on_init;
	Edit on_edit;
	Drag on_drag;
};

template <class InputFieldCallbacks>
bool input_field(InputFieldCallbacks callbacks, auto &state, auto &value, umm id, std::source_location location) {
	begin_input_user(true);

	auto &theme = editor->text_field_theme;
	auto font_size = 12;//get_font_size(theme.font_size);

	bool value_changed = false;
	bool stop_edit = false;
	bool apply_input = false;
	bool set_caret_from_mouse = false;
	if (state.editing) {
		if ((editor->key_state[256 + 0].state & KeyState_down)) {
			if (in_bounds(app->current_mouse_position, editor->current_scissor)) {
				set_caret_from_mouse = true;
			} else {
				apply_input = true;
				stop_edit = true;
				editor->key_state[256 + 0].start_position = app->current_mouse_position;
			}
		}
		if ((editor->key_state[256 + 1].state & KeyState_down)) {
			apply_input = true;
			stop_edit = true;
		}
		if (key_down(Key_escape)) {
			stop_edit = true;
		}
	} else {
		if (mouse_begin_drag(0)) {
			lock_input();
		}
		if (mouse_drag(0)) {
			callbacks.on_drag(app->window->mouse_delta.x);
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
		state.caret_position = state.string.count;
	}

	if (state.editing) {
		if (editor->key_state[Key_left_arrow].state & KeyState_repeated) {
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
		if (editor->key_state[Key_right_arrow].state & KeyState_repeated) {
			if (state.caret_position != state.string.count) {
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
			state.caret_position = state.string.count;
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

		for (auto c : editor->input_string) {
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
					state.caret_position = state.string.count;
					break;
				}
				case 3: { // Control + C
					u32 sel_min, sel_max;
					minmax(state.caret_position, state.selection_start, sel_min, sel_max);
					if (sel_max == sel_min) {
						sel_min = 0;
						sel_max = state.string.count;
					}
					if (sel_max != sel_min) {
						set_clipboard(app->window, Clipboard_text, Span<utf8>{state.string.data + sel_min, sel_max - sel_min});
					}
					break;
				}
				case 22: { // Control + V
					auto clipboard = with(temporary_allocator, (List<utf8>)get_clipboard(app->window, Clipboard_text));
					defer { free(clipboard); };

					u32 new_caret_position;
					if (state.caret_position == state.selection_start) {
						state.string.insert_at(clipboard, state.caret_position);
						new_caret_position = state.caret_position + clipboard.count;
					} else {
						u32 sel_min, sel_max;
						minmax(state.caret_position, state.selection_start, sel_min, sel_max);
						state.string.replace({state.string.data + sel_min, sel_max - sel_min}, clipboard);
						new_caret_position = sel_min + clipboard.count;
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
					if (editor->key_state[Key_shift].state & KeyState_held) {
						editor->should_switch_focus_to = editor->focusable_input_user_index - 1;
					} else {
						editor->should_switch_focus_to = editor->focusable_input_user_index + 1;
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
		editor->input_string.clear();

		if (editor->key_state[Key_delete].state & KeyState_repeated) {
			erase = Erase_delete;
		}


		if (erase != Erase_none) {
			if (state.caret_position == state.selection_start) {
				if (erase == Erase_backspace) {
					// Remove one character to the left of the caret
					if (state.string.count && state.caret_position) {
						state.selection_start = state.caret_position -= 1;
						state.string.erase_at(state.caret_position);
						state.caret_blink_time = 0;
					}
				} else  {
					// Remove one character to the right of the caret
					if (state.string.count && state.caret_position != state.string.count) {
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
	} else if (in_bounds(app->current_mouse_position, editor->current_scissor)) {
		color = theme.hovered_color;
	}

	gui_panel(color);

	if (state.editing) {
		tg::Rect caret_viewport = editor->current_viewport;
		caret_viewport.min.x = editor->current_viewport.min.x;
		caret_viewport.max.x = caret_viewport.min.x + 1;


		if (state.string.count) {
			auto font = get_font_at_size(app->font_collection, font_size);
			ensure_all_chars_present(state.string, font);
			auto placed_chars = with(temporary_allocator, get_text_info(state.string, font, {.place_chars=true}).placed_chars);

			if (set_caret_from_mouse) {
				u32 new_caret_position = placed_chars.count;

				for (u32 char_index = 0; char_index < placed_chars.count; char_index += 1) {
					if (placed_chars[char_index].position.center().x - state.text_offset > (app->current_mouse_position.x - editor->current_viewport.min.x)) {
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

			if (placed_chars.back().position.max.x > editor->current_viewport.size().x) {
				state.text_offset = max(state.text_offset, caret_x - (s32)(editor->current_viewport.size().x * 3 / 4));
				state.text_offset = min(state.text_offset, caret_x - (s32)(editor->current_viewport.size().x * 1 / 4));
				state.text_offset = min(state.text_offset, (s32)placed_chars.back().position.max.x - editor->current_viewport.size().x + 1);
				state.text_offset = max(state.text_offset, 0);
				//state.text_offset = min<s32>(state.text_offset, caret_x - (s32)(editor->current_viewport.size().x * 1 / 4));

				//s32 rel_caret_pos = caret_x - state.text_offset;
				//s32 diff = rel_caret_pos - (s32)(editor->current_viewport.size().x * 3 / 4);
				//if (diff < 0) {
				//	state.text_offset += diff;
				//}
				//
				//rel_caret_pos = caret_x - state.text_offset;
				//diff = rel_caret_pos - (s32)(editor->current_viewport.size().x / 4);
				//if (diff > 0) {
				//	state.text_offset -= diff;
				//}

				//state.text_offset = clamp<s32>(
				//	(s32)editor->current_viewport.size().x / 4 - (s32)caret_x,
				//	0,
				//	-placed_chars.back().position.max.x + editor->current_viewport.size().x - 1
				//);
			}

			caret_viewport.min.x += caret_x - state.text_offset;
			caret_viewport.max.x += caret_x - state.text_offset;

			if (state.caret_position != state.selection_start) {
				u32 sel_min, sel_max;
				minmax(state.caret_position, state.selection_start, sel_min, sel_max);

				u32 min_x = placed_chars[sel_min    ].position.min.x;
				u32 max_x = placed_chars[sel_max - 1].position.max.x;

				tg::Rect selection_viewport = editor->current_viewport;
				selection_viewport.min.x = editor->current_viewport.min.x + min_x;
				selection_viewport.max.x = selection_viewport.min.x + max_x - min_x;

				push_viewport(selection_viewport) gui_panel({0.25f,0.25f,0.5f,1});
			}

			label((List<utf8>)to_string(value), font_size, {.position = {-state.text_offset, 0}});
			//label(placed_chars, font, {.position = {-state.text_offset, 0}}, id, location, V4f(1));
		}




		if (state.caret_blink_time <= 0.5f) {
			push_viewport (caret_viewport) {
				gui_panel({1, 1, 1, 1});
			}
		}
		state.caret_blink_time += app->frame_time;
		if (state.caret_blink_time >= 1) {
			state.caret_blink_time -= 1;
		}
	} else {
		label((List<utf8>)to_string(value), font_size);
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

inline Optional<f64> parse_expression(FloatFieldToken *&t, FloatFieldToken *end) {
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
					case '+': result += right.value(); break;
					case '-': result -= right.value(); break;
					case '*': result *= right.value(); break;
					case '/': result /= right.value(); break;
				}
			}
		}
	}

	return result;
}

inline bool float_field(f32 &value, umm id = 0, std::source_location location = std::source_location::current()) {
	auto &state = editor->float_field_states.get_or_insert({id, location});

	return input_field(InputFieldCallbacks{
		.on_init = [&]() {
			state.string = with(default_allocator, (List<utf8>)to_string(FormatFloat{.value = value, .precision = 6}));
			state.original_value = value;
		},
		.on_edit = [&] {
			if (!state.string.count) {
				value = 0;
				return true;
			}

			List<FloatFieldToken> tokens;
			tokens.allocator = temporary_allocator;

			auto current_char_p = state.string.data;
			auto next_char_p = current_char_p;
			auto end = state.string.end();

			utf32 c = 0;
			auto next_char = [&] {
				current_char_p = next_char_p;
				if (current_char_p >= end) {
					return false;
				}
				auto got = get_char_and_advance_utf8(&next_char_p);
				if (got) {
					c = got.value();
					return true;
				}
				return false;
			};

			next_char();

			while (current_char_p < end) {
				if (is_alpha(c) || c == '_') {
					FloatFieldToken token;
					token.kind = FloatFieldToken_number;
					Span<utf8> token_string;
					token_string.data = current_char_p;

					while (next_char() && (is_alpha(c) || c == '_' || is_digit(c))) {
						// Skip to end of identifier
					}

					token_string.count = current_char_p - token_string.data;

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
				} else if (is_digit(c) || c == '.') {
					FloatFieldToken token;
					token.kind = FloatFieldToken_number;

					Span<utf8> token_string;
					token_string.data = current_char_p;

					while (next_char() && is_digit(c)) {
						// Skip to end of whole part
					}

					if (current_char_p != end) {
						if (c == '.') {
							while (next_char() && is_digit(c)) {
								// Skip to end of fractional part
							}
						}
					}

					token_string.count = current_char_p - token_string.data;
					auto parsed = parse_f32(token_string);
					if (!parsed) {
						return false;
					}
					token.value = parsed.value();
					tokens.add(token);
				} else {
					switch (c) {
						case '+':
						case '-':
						case '*':
						case '/': {
							FloatFieldToken token;
							token.kind = c;
							tokens.add(token);
							next_char();
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
				if (parsed || state.string.count == 0) {
					value = parsed ? parsed.value() : 0;
					return true;
				}
			}
			return false;
		},
		.on_drag = [&](f32 delta){
			value += delta * (key_held(Key_shift, {.anywhere = true}) ? 0.01f : 0.1f);
		},
	}, state, value, id, location);
}

inline bool text_field(List<utf8> &value, umm id = 0, std::source_location location = std::source_location::current()) {
	auto &state = editor->text_field_states.get_or_insert({id, location});

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
	}, state, value, id, location);
}


inline void begin_scrollbar(umm id = 0, std::source_location location = std::source_location::current()) {
	auto &state = editor->scroll_bar_states.get_or_insert({id, location});
}
inline void end_scrollbar(umm id = 0, std::source_location location = std::source_location::current()) {
	auto &state = editor->scroll_bar_states.get_or_insert({id, location});
}


s32 const line_height = 16;

inline void header(Span<utf8> text) {
	tg::Rect line_viewport = editor->current_viewport;
	line_viewport.min.y = editor->current_viewport.max.y - line_height - editor->current_property_y;
	line_viewport.max.y = line_viewport.min.y + line_height;
	push_viewport(line_viewport) {
		label(text, font_size);
	}
	editor->current_property_y += line_height + 2;
}

inline void property_separator() {
	s32 const separator_height = 2;
	tg::Rect line_viewport = editor->current_viewport;
	line_viewport.min.y = editor->current_viewport.max.y - separator_height - editor->current_property_y;
	line_viewport.max.y = line_viewport.min.y + separator_height;
	push_viewport(line_viewport) {
		gui_panel({.05, .05, .05, 1});
	}
	editor->current_property_y += separator_height + 2;
}


template <class Fn>
void draw_asset_property(Span<utf8> name, Span<utf8> path, umm id, std::source_location location, Span<Span<utf8>> file_extensions, Fn &&update) {
	tg::Rect line_viewport = editor->current_viewport;
	line_viewport.min.y = editor->current_viewport.max.y - line_height - editor->current_property_y;
	line_viewport.max.y = line_viewport.min.y + line_height;
	push_viewport(line_viewport) {
		s32 text_width;

		auto font = get_font_at_size(app->font_collection, font_size);
		ensure_all_chars_present(name, font);
		auto placed_text = with(temporary_allocator, get_text_info(name, font, {.place_chars=true}).placed_chars);
		text_width = placed_text.back().position.max.x;
		label(name, font_size);

		auto value_viewport = editor->current_viewport;
		value_viewport.min.x += text_width + 2;

		push_viewport(value_viewport) {
			gui_panel({.05, .05, .05, 1});
			label(path, font_size);

			bool result = (editor->key_state[256 + 0].state & KeyState_up) && in_bounds(app->current_mouse_position, editor->current_scissor);

			if (result) {
				int x = 5;
			}

			if (editor->drag_and_drop_kind == DragAndDrop_file) {
				auto dragging_path = as_utf8(editor->drag_and_drop_data);
				auto found = find_if(file_extensions, [&](Span<utf8> extension) {
					return ends_with(dragging_path, extension);
				});

				if (found) {
					gui_panel({0.1, 1.0, 0.1, 0.2});
				}
			}
			if (accept_drag_and_drop(DragAndDrop_file)) {
				update(as_utf8(editor->drag_and_drop_data));
			}
		}
	}

	editor->current_property_y += line_height + 2;
}

void gui_begin_frame();

void gui_draw();
