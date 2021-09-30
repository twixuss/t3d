#include "draw_property.h"
#include "gui.h"

void draw_property(Span<utf8> name, f32 &value, std::source_location location) {
	tg::Viewport line_viewport = editor->current_viewport;
	line_viewport.min.y = editor->current_viewport.max.y - line_height - editor->current_property_y;
	line_viewport.max.y = line_viewport.min.y + line_height;

	push_viewport(line_viewport) {
		s32 text_width = 0;

		auto font = get_font_at_size(app->font_collection, font_size);
		ensure_all_chars_present(name, font);
		auto placed_text = with(temporary_allocator, get_text_info(name, font, {.place_chars=true}).placed_chars);
		text_width = placed_text.back().position.max.x;
		label({}, placed_text, font, V4f(1));

		auto value_viewport = line_viewport;
		value_viewport.min.x += text_width + 2;

		push_viewport(value_viewport) {
			float_field(value, 0, location);
		}
	}

	editor->current_property_y += line_height + 2;
}
void draw_property(Span<utf8> name, v3f &value, std::source_location location) {
	header(name);

	auto line_viewport = editor->current_viewport;
	line_viewport.min.y = line_viewport.max.y - line_height - editor->current_property_y;
	line_viewport.max.y = line_viewport.min.y + line_height;


	auto x_viewport = line_viewport;
	auto y_viewport = line_viewport;
	auto z_viewport = line_viewport;

	x_viewport.max.x = x_viewport.min.x + line_viewport.size().x / 3;

	y_viewport.min.x = x_viewport.max.x;
	y_viewport.max.x = x_viewport.min.x + line_viewport.size().x * 2 / 3;

	z_viewport.min.x = y_viewport.max.x;
	z_viewport.max.x = line_viewport.max.x;

	push_viewport(x_viewport) label("X", font_size);
	push_viewport(y_viewport) label("Y", font_size);
	push_viewport(z_viewport) label("Z", font_size);

	x_viewport.min.x += font_size;
	y_viewport.min.x += font_size;
	z_viewport.min.x += font_size;

	push_viewport(x_viewport) float_field(value.x, 0, location);
	push_viewport(y_viewport) float_field(value.y, 1, location);
	push_viewport(z_viewport) float_field(value.z, 2, location);

	editor->current_property_y += line_height + 2;
}
void draw_property(Span<utf8> name, quaternion &value, std::source_location location) {
	v3f angles = degrees(to_euler_angles(value));
	draw_property(name, angles, location);
	value = quaternion_from_euler(radians(angles));
}
void draw_property(Span<utf8> name, List<utf8> &value, std::source_location location) {
	header(name);

	tg::Viewport line_viewport = editor->current_viewport;
	line_viewport.min.y = line_viewport.max.y - line_height - editor->current_property_y;
	line_viewport.max.y = line_viewport.min.y + line_height;

	push_viewport(line_viewport) text_field(value, 0, location);

	editor->current_property_y += line_height + 2;
}
void draw_property(Span<utf8> name, Texture2D *&value, std::source_location location) {
	Span<utf8> extensions[] = {
		u8".png"s,
		u8".jpg"s,
		u8".hdr"s,
	};
	draw_asset_property(name, value ? value->name : u8"null"s, 0, location, extensions, [&] (Span<utf8> path) {
		auto new_texture = app->assets.get_texture_2d(path);
		if (new_texture) {
			value = new_texture;
		}
	});
}
void draw_property(Span<utf8> name, Mesh *&value, std::source_location location) {
	Span<utf8> extensions[] = {
		u8"idk"s
	};
	draw_asset_property(name, value ? value->name : u8"null"s, 0, location, extensions, [&] (Span<utf8> path) {

	});
}
