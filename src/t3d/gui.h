#pragma once
#include <t3d/font.h>
#include <t3d/blit.h>
#include <t3d/assets.h>
#include <t3d/editor/current.h>
#include <t3d/editor/window.h>
#include <t3d/editor/input.h>
#include <t3d/shared.h>

inline tg::Viewport pad(tg::Viewport viewport) {
	viewport.min += 2;
	viewport.max -= 2;
	return viewport;
}

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

inline constexpr ButtonTheme default_button_theme;

struct TextFieldTheme {
	v4f color = background_color;
	v4f hovered_color = {.15f, .15f, .15f, 1};
	v4f edit_color = {.2f, .2f, .1f, 1};
};

inline constexpr TextFieldTheme default_text_field_theme;

void gui_panel(v4f color);
void gui_image(tg::Texture2D *texture);

u32 const font_size = 12;

struct DrawTextParams {
	v2s position = {};
};

void label(Span<PlacedChar> placed_chars, SizedFont *font, DrawTextParams params = {});
void label(Span<utf8> string, DrawTextParams params = {});
inline void label(utf8 const *string, DrawTextParams params = {}) { label(as_span(string), params); }
inline void label(Span<char>  string, DrawTextParams params = {}) { label((Span<utf8>)string, params); }
inline void label(char const *string, DrawTextParams params = {}) { label((Span<utf8>)as_span(string), params); }

bool button_base(umm id, ButtonTheme const &theme, std::source_location location);
inline bool button(Span<utf8> text, umm id = 0, ButtonTheme const &theme = default_button_theme, std::source_location location = std::source_location::current()) {
	auto result = button_base(id, theme, location);

	label(text);

	return result;
}

inline bool button(tg::Texture2D *texture, umm id = 0, ButtonTheme const &theme = default_button_theme, std::source_location location = std::source_location::current()) {
	auto result = button_base(id, theme, location);

	gui_image(texture);

	return result;
}

inline bool button(tg::Viewport button_viewport, Span<utf8> text, umm id = 0, ButtonTheme const &theme = default_button_theme, std::source_location location = std::source_location::current()) {
	push_current_viewport(button_viewport) {
		return button(text, id, theme, location);
	}
	return false;
}
inline bool button(tg::Viewport button_viewport, tg::Texture2D *texture, umm id = 0, ButtonTheme const &theme = default_button_theme, std::source_location location = std::source_location::current()) {
	push_current_viewport(button_viewport) {
		return button(texture, id, theme, location);
	}
	return false;
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
		if ((shared->key_state[256 + 0].state & KeyState_down)) {
			if (in_bounds(shared->current_mouse_position, shared->current_scissor)) {
				set_caret_from_mouse = true;
			} else {
				apply_input = true;
				stop_edit = true;
				shared->key_state[256 + 0].start_position = shared->current_mouse_position;
			}
		}
		if ((shared->key_state[256 + 1].state & KeyState_down)) {
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
			callbacks.on_drag(shared->window->mouse_delta.x);
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
		if (shared->key_state[Key_left_arrow].state & KeyState_repeated) {
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
		if (shared->key_state[Key_right_arrow].state & KeyState_repeated) {
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

		for (auto c : shared->input_string) {
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
						set_clipboard(shared->window, Clipboard_text, Span<utf8>{state.string.data + sel_min, sel_max - sel_min});
					}
					break;
				}
				case 22: { // Control + V
					auto clipboard = with(temporary_allocator, (List<utf8>)get_clipboard(shared->window, Clipboard_text));
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
					if (shared->key_state[Key_shift].state & KeyState_held) {
						shared->should_switch_focus_to = shared->focusable_input_user_index - 1;
					} else {
						shared->should_switch_focus_to = shared->focusable_input_user_index + 1;
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
		shared->input_string.clear();

		if (shared->key_state[Key_delete].state & KeyState_repeated) {
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
	} else if (in_bounds(shared->current_mouse_position, shared->current_scissor)) {
		color = theme.hovered_color;
	}

	gui_panel(color);

	if (state.editing) {
		tg::Viewport caret_viewport = shared->current_viewport;
		caret_viewport.min.x = shared->current_viewport.min.x;
		caret_viewport.max.x = caret_viewport.min.x + 1;


		if (state.string.size) {
			auto font = get_font_at_size(shared->font_collection, font_size);
			ensure_all_chars_present(state.string, font);
			auto placed_chars = with(temporary_allocator, place_text(state.string, font));

			if (set_caret_from_mouse) {
				u32 new_caret_position = placed_chars.size;

				for (u32 char_index = 0; char_index < placed_chars.size; char_index += 1) {
					if (placed_chars[char_index].position.center().x - state.text_offset > (shared->current_mouse_position.x - shared->current_viewport.min.x)) {
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

			if (placed_chars.back().position.max.x > shared->current_viewport.size().x) {
				state.text_offset = max(state.text_offset, caret_x - (s32)(shared->current_viewport.size().x * 3 / 4));
				state.text_offset = min(state.text_offset, caret_x - (s32)(shared->current_viewport.size().x * 1 / 4));
				state.text_offset = min(state.text_offset, (s32)placed_chars.back().position.max.x - shared->current_viewport.size().x + 1);
				state.text_offset = max(state.text_offset, 0);
				//state.text_offset = min<s32>(state.text_offset, caret_x - (s32)(shared->current_viewport.size().x * 1 / 4));

				//s32 rel_caret_pos = caret_x - state.text_offset;
				//s32 diff = rel_caret_pos - (s32)(shared->current_viewport.size().x * 3 / 4);
				//if (diff < 0) {
				//	state.text_offset += diff;
				//}
				//
				//rel_caret_pos = caret_x - state.text_offset;
				//diff = rel_caret_pos - (s32)(shared->current_viewport.size().x / 4);
				//if (diff > 0) {
				//	state.text_offset -= diff;
				//}


				print("%\n", state.text_offset);

				//state.text_offset = clamp<s32>(
				//	(s32)shared->current_viewport.size().x / 4 - (s32)caret_x,
				//	0,
				//	-placed_chars.back().position.max.x + shared->current_viewport.size().x - 1
				//);
			}

			caret_viewport.min.x += caret_x - state.text_offset;
			caret_viewport.max.x += caret_x - state.text_offset;

			if (state.caret_position != state.selection_start) {
				u32 sel_min, sel_max;
				minmax(state.caret_position, state.selection_start, sel_min, sel_max);

				u32 min_x = placed_chars[sel_min    ].position.min.x;
				u32 max_x = placed_chars[sel_max - 1].position.max.x;

				tg::Viewport selection_viewport = shared->current_viewport;
				selection_viewport.min.x = shared->current_viewport.min.x + min_x;
				selection_viewport.max.x = selection_viewport.min.x + max_x - min_x;

				push_current_viewport(selection_viewport) gui_panel({0.25f,0.25f,0.5f,1});
			}

			label(placed_chars, font, {.position = {-state.text_offset, 0}});
		}




		if (state.caret_blink_time <= 0.5f) {
			push_current_viewport (caret_viewport) {
				gui_panel({1, 1, 1, 1});
			}
		}
		state.caret_blink_time += shared->frame_time;
		if (state.caret_blink_time >= 1) {
			state.caret_blink_time -= 1;
		}
	} else {
		label((List<utf8>)to_string(value));
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
					case '+': result += right.get(); break;
					case '-': result -= right.get(); break;
					case '*': result *= right.get(); break;
					case '/': result /= right.get(); break;
				}
			}
		}
	}

	return result;
}

inline bool float_field(f32 &value, umm id = 0, TextFieldTheme const &theme = default_text_field_theme, std::source_location location = std::source_location::current()) {
	auto &state = shared->float_field_states.get_or_insert({id, location});

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
				if (got.valid()) {
					c = got.get();
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

					token_string.size = current_char_p - token_string.data;

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

					token_string.size = current_char_p - token_string.data;
					auto parsed = parse_f32(token_string);
					if (!parsed) {
						return false;
					}
					token.value = parsed.get();
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
				if (parsed || state.string.size == 0) {
					value = parsed ? parsed.get() : 0;
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

inline bool text_field(List<utf8> &value, umm id = 0, TextFieldTheme const &theme = default_text_field_theme, std::source_location location = std::source_location::current()) {
	auto &state = shared->text_field_states.get_or_insert({id, location});

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


inline void begin_scrollbar(umm id = 0, std::source_location location = std::source_location::current()) {
	auto &state = shared->scroll_bar_states.get_or_insert({id, location});
}
inline void end_scrollbar(umm id = 0, std::source_location location = std::source_location::current()) {
	auto &state = shared->scroll_bar_states.get_or_insert({id, location});
}


s32 const line_height = 16;

inline void header(Span<utf8> text) {
	tg::Viewport line_viewport = shared->current_viewport;
	line_viewport.min.y = shared->current_viewport.max.y - line_height - shared->current_property_y;
	line_viewport.max.y = line_viewport.min.y + line_height;
	push_current_viewport(line_viewport) label(text);
	shared->current_property_y += line_height + 2;
}

inline void property_separator() {
	s32 const separator_height = 2;
	tg::Viewport line_viewport = shared->current_viewport;
	line_viewport.min.y = shared->current_viewport.max.y - separator_height - shared->current_property_y;
	line_viewport.max.y = line_viewport.min.y + separator_height;
	push_current_viewport(line_viewport) gui_panel({.05, .05, .05, 1});
	shared->current_property_y += separator_height + 2;
}

inline u64 get_id(u64 id, std::source_location location) {
	return ((u64)location.file_name() << 48) + ((u64)location.line() << 32) + ((u64)location.column() << 16) + id;
}


template <class Fn>
void draw_asset_property(Span<utf8> name, Span<utf8> path, u64 id, std::source_location location, Span<Span<utf8>> file_extensions, Fn &&update) {
	tg::Viewport line_viewport = shared->current_viewport;
	line_viewport.min.y = shared->current_viewport.max.y - line_height - shared->current_property_y;
	line_viewport.max.y = line_viewport.min.y + line_height;
	push_current_viewport(line_viewport) {
		s32 text_width;

		auto font = get_font_at_size(shared->font_collection, font_size);
		ensure_all_chars_present(name, font);
		auto placed_text = with(temporary_allocator, place_text(name, font));
		text_width = placed_text.back().position.max.x;
		label(placed_text, font);

		auto value_viewport = shared->current_viewport;
		value_viewport.min.x += text_width + 2;

		push_current_viewport(value_viewport) {
			gui_panel({.05, .05, .05, 1});
			label(path);

			bool result = (shared->key_state[256 + 0].state & KeyState_up) && in_bounds(shared->current_mouse_position, shared->current_scissor);

			if (result) {
				int x = 5;
			}

			if (shared->drag_and_drop_kind == DragAndDrop_file) {
				auto dragging_path = as_utf8(shared->drag_and_drop_data);
				auto found = find_if(file_extensions, [&](Span<utf8> extension) {
					return ends_with(dragging_path, extension);
				});

				if (found) {
					gui_panel({0.1, 1.0, 0.1, 0.2});
				}
			}
			if (accept_drag_and_drop(DragAndDrop_file)) {
				update(as_utf8(shared->drag_and_drop_data));
			}
		}
	}

	shared->current_property_y += line_height + 2;
}
