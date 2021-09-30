#include "gui.h"
#include <t3d/app.h>

static void add_gui_draw(GuiDraw draw) {
	draw.viewport = editor->current_viewport;
	draw.scissor = editor->current_scissor;
	editor->gui_draws.add(draw);
}

void gui_panel(v4f color) {
	add_gui_draw({.kind = GuiDraw_rect_colored, .rect_colored = {.color = color}});
}

void gui_image(tg::Texture2D *texture) {
	add_gui_draw({.kind = GuiDraw_rect_textured, .rect_textured = {.texture = texture}});
}

u32 get_font_size(u32 font_size) {
	if (!font_size)
		return editor->current_viewport.size().y;
	return font_size;
}

void label(v2s position, List<PlacedChar> placed_chars, SizedFont *font, v4f color) {
	add_gui_draw({
		.kind = GuiDraw_label,
		.label = {
			.position = position,
			.placed_chars = placed_chars,
			.font = font,
			.color = color,
		},
	});
}

void label(Span<utf8> string, u32 font_size, DrawTextParams params) {
	if (string.size == 0)
		return;

	if (!font_size)
		font_size = editor->current_viewport.size().y;

	auto &theme = editor->label_theme;

	auto font = with(temporary_allocator, get_font_at_size(app->font_collection, font_size));
	ensure_all_chars_present(string, font);

	auto info = get_text_info(string, font, {.place_chars=true,.bounds=true});
	v2s offset = {
		editor->current_viewport.size().x - info.bounds.size().x,
		editor->current_viewport.size().y - font_size * info.line_count,
	};
	switch (params.align) {
		case Align_top_left:
			break;
		case Align_top:
			params.position.x += offset.x / 2;
			break;
		case Align_top_right:
			params.position.x += offset.x;
			break;

		case Align_left:
			params.position.y += offset.y / 2;
			break;
		case Align_center:
			params.position.x += offset.x / 2;
			params.position.y += offset.y / 2;
			break;
		case Align_right:
			params.position.x += offset.x;
			params.position.y += offset.y / 2;
			break;

		case Align_bottom_left:
			params.position.y += offset.y;
			break;
		case Align_bottom:
			params.position.x += offset.x / 2;
			params.position.y += offset.y;
			break;
		case Align_bottom_right:
			params.position.x += offset.x;
			params.position.y += offset.y;
			break;
		default: invalid_code_path("not implemented");
	}

	label(params.position, info.placed_chars, font, theme.color);
}

//
// NOTE: changes `editor->current_viewport` and does not reset it
//
static bool button_base(umm id, std::source_location location) {
	auto &state = editor->button_states.get_or_insert({id, location});

	auto &theme = editor->button_theme;

	bool clicked = mouse_click(0);
	if (clicked) {
		state.click_t = 1;
	}

	bool currently_hovered = in_bounds(app->current_mouse_position, editor->current_scissor);
	if (currently_hovered && !state.previously_hovered) {
		state.hover_enter_t = 1;
	}

	state.press_t       = lerp<f32>(state.press_t,      (f32)mouse_held(0),     app->frame_time * theme.press_speed);
	state.hover_stay_t  = lerp<f32>(state.hover_stay_t, (f32)currently_hovered, app->frame_time * theme.hover_stay_speed);

	v4f color = theme.color;
	color = lerp(color, theme.hover_stay_color,  V4f(state.hover_stay_t));
	color = lerp(color, theme.hover_enter_color, V4f(state.hover_enter_t));
	color = lerp(color, theme.press_color,       V4f(state.press_t));
	color = lerp(color, theme.click_color,       V4f(state.click_t));

	state.click_t       = lerp<f32>(state.click_t,       0, app->frame_time * theme.click_speed);
	state.hover_enter_t = lerp<f32>(state.hover_enter_t, 0, app->frame_time * theme.hover_enter_speed);

	state.previously_hovered = currently_hovered;

	editor->current_viewport.min.x += theme.left_padding;
	editor->current_viewport.min.y += theme.bottom_padding;
	editor->current_viewport.max.x -= theme.right_padding;
	editor->current_viewport.max.y -= theme.top_padding;

	gui_panel(color);

	return clicked;
}
bool button(umm id, std::source_location location) {
	auto old = editor->current_viewport;
	defer { editor->current_viewport = old; };
	auto result = button_base(id, location);
	return result;
}
bool button(Span<utf8> text, umm id, std::source_location location) {
	auto old = editor->current_viewport;
	defer { editor->current_viewport = old; };

	auto result = button_base(id, location);

	label(text, editor->button_theme.font_size, {.position = {2,2}});

	return result;
}

bool button(tg::Texture2D *texture, umm id, std::source_location location) {
	auto old = editor->current_viewport;
	defer { editor->current_viewport = old; };

	auto result = button_base(id, location);
	gui_image(texture);
	return result;
}

void gui_begin_frame() {
	editor->input_user_index = 0;
	editor->focusable_input_user_index = 0;
}

void gui_draw() {
	for (auto &draw : editor->gui_draws) {
		if (volume(draw.scissor) <= 0)
			continue;

		app->tg->set_viewport(draw.viewport);
		app->tg->set_scissor(draw.scissor);
		switch (draw.kind) {
			case GuiDraw_rect_colored: {
				auto &rect_colored = draw.rect_colored;
				blit(rect_colored.color);
				break;
			}
			case GuiDraw_rect_textured: {
				auto &rect_textured = draw.rect_textured;
				blit(rect_textured.texture);
				break;
			}
			case GuiDraw_label: {
				auto &label = draw.label;

				auto font = label.font;
				auto placed_text = label.placed_chars;

				assert(placed_text.size);

				struct Vertex {
					v2f position;
					v2f uv;
				};

				List<Vertex> vertices;
				vertices.allocator = temporary_allocator;

				for (auto &c : placed_text) {
					Span<Vertex> quad = {
						{{(f32)c.position.min.x, (f32)c.position.min.y}, {(f32)c.uv.min.x, (f32)c.uv.min.y}},
						{{(f32)c.position.max.x, (f32)c.position.min.y}, {(f32)c.uv.max.x, (f32)c.uv.min.y}},
						{{(f32)c.position.max.x, (f32)c.position.max.y}, {(f32)c.uv.max.x, (f32)c.uv.max.y}},
						{{(f32)c.position.min.x, (f32)c.position.max.y}, {(f32)c.uv.min.x, (f32)c.uv.max.y}},
					};
					vertices += {
						quad[1], quad[0], quad[2],
						quad[2], quad[0], quad[3],
					};
				}

				if (app->text_vertex_buffer) {
					app->tg->update_vertex_buffer(app->text_vertex_buffer, as_bytes(vertices));
				} else {
					app->text_vertex_buffer = app->tg->create_vertex_buffer(as_bytes(vertices), {
						tg::Element_f32x2, // position
						tg::Element_f32x2, // uv
					});
				}
				app->tg->set_rasterizer({.depth_test = false, .depth_write = false});
				app->tg->set_topology(tg::Topology_triangle_list);
				app->tg->set_blend(tg::BlendFunction_add, tg::Blend_secondary_color, tg::Blend_one_minus_secondary_color);
				app->tg->set_shader(app->text_shader);
				app->tg->set_shader_constants(app->text_shader_constants, 0);
				app->tg->update_shader_constants(app->text_shader_constants, {
					.inv_half_viewport_size = v2f{2,-2} / (v2f)draw.viewport.size(),
					.offset = (v2f)label.position,
					.color = label.color,
				});
				app->tg->set_vertex_buffer(app->text_vertex_buffer);
				app->tg->set_sampler(tg::Filtering_nearest, 0);
				app->tg->set_texture(font->texture, 0);
				app->tg->draw(vertices.size);
				break;
			}
			default: invalid_code_path("not implemented"); break;
		}
	}
	editor->gui_draws.clear();
}
