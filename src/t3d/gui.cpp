#include "gui.h"
#include <t3d/shared.h>

void gui_panel(v4f color) {
	shared->gui_draws.add({.kind = GuiDraw_rect_colored, .viewport = shared->current_viewport, .scissor = shared->current_scissor, .rect_colored = {.color = color}});
}

void gui_image(tg::Texture2D *texture) {
	shared->gui_draws.add({.kind = GuiDraw_rect_textured, .viewport = shared->current_viewport, .scissor = shared->current_scissor, .rect_textured = {.texture = texture}});
}

void label(Span<PlacedChar> placed_chars, SizedFont *font, DrawTextParams params) {
	if (placed_chars.size == 0)
		return;

	shared->gui_draws.add({.kind = GuiDraw_label, .viewport = shared->current_viewport, .scissor = shared->current_scissor, .label = {.position = params.position, .placed_chars = with(temporary_allocator, as_list(placed_chars)), .font = font}});
}
void label(Span<utf8> string, DrawTextParams params) {
	if (string.size == 0)
		return;

	auto font = get_font_at_size(shared->font_collection, font_size);
	ensure_all_chars_present(string, font);

	label(with(temporary_allocator, place_text(string, font)), font, params);
}

bool button_base(umm id, ButtonTheme const &theme, std::source_location location) {
	auto &state = shared->button_states.get_or_insert({id, location});

	bool result = mouse_click(0);
	if (result) {
		state.click_t = 1;
	}

	bool currently_hovered = in_bounds(shared->current_mouse_position, shared->current_scissor);
	if (currently_hovered && !state.previously_hovered) {
		state.hover_enter_t = 1;
	}

	state.press_t       = lerp<f32>(state.press_t,      (f32)mouse_held(0),     shared->frame_time * theme.press_speed);
	state.hover_stay_t  = lerp<f32>(state.hover_stay_t, (f32)currently_hovered, shared->frame_time * theme.hover_stay_speed);

	v4f color = theme.color;
	color = lerp(color, theme.hover_stay_color,  V4f(state.hover_stay_t));
	color = lerp(color, theme.hover_enter_color, V4f(state.hover_enter_t));
	color = lerp(color, theme.press_color,       V4f(state.press_t));
	color = lerp(color, theme.click_color,       V4f(state.click_t));

	gui_panel(color);

	state.click_t       = lerp<f32>(state.click_t,       0, shared->frame_time * theme.click_speed);
	state.hover_enter_t = lerp<f32>(state.hover_enter_t, 0, shared->frame_time * theme.hover_enter_speed);

	state.previously_hovered = currently_hovered;


	return result;
}
