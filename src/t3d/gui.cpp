#include "gui.h"
#include <t3d/app.h>

void gui_panel(v4f color) {
	editor->gui_draws.add({.kind = GuiDraw_rect_colored, .viewport = app->current_viewport, .scissor = app->current_scissor, .rect_colored = {.color = color}});
}

void gui_image(tg::Texture2D *texture) {
	editor->gui_draws.add({.kind = GuiDraw_rect_textured, .viewport = app->current_viewport, .scissor = app->current_scissor, .rect_textured = {.texture = texture}});
}

void label(Span<PlacedChar> placed_chars, SizedFont *font, DrawTextParams params) {
	if (placed_chars.size == 0)
		return;

	editor->gui_draws.add({.kind = GuiDraw_label, .viewport = app->current_viewport, .scissor = app->current_scissor, .label = {.position = params.position, .placed_chars = with(temporary_allocator, as_list(placed_chars)), .font = font}});
}
void label(Span<utf8> string, u32 font_size, DrawTextParams params) {
	if (string.size == 0)
		return;

	auto font = get_font_at_size(app->font_collection, font_size);
	ensure_all_chars_present(string, font);

	label(with(temporary_allocator, place_text(string, font)), font, params);
}

bool button_base(umm id, ButtonTheme const &theme, std::source_location location) {
	auto &state = editor->button_states.get_or_insert({id, location});

	bool result = mouse_click(0);
	if (result) {
		state.click_t = 1;
	}

	bool currently_hovered = in_bounds(app->current_mouse_position, app->current_scissor);
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

	gui_panel(color);

	state.click_t       = lerp<f32>(state.click_t,       0, app->frame_time * theme.click_speed);
	state.hover_enter_t = lerp<f32>(state.hover_enter_t, 0, app->frame_time * theme.hover_enter_speed);

	state.previously_hovered = currently_hovered;


	return result;
}

void gui_begin_frame() {
	editor->input_user_index = 0;
	editor->focusable_input_user_index = 0;
}

void gui_draw() {
	for (auto &draw : editor->gui_draws) {
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
