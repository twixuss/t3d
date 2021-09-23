#include "draw_property.h"
#include "gui.h"

void draw_property(Span<utf8> name, f32 &value, u64 id, std::source_location location) {
	tg::Viewport line_viewport = shared->current_viewport;
	line_viewport.min.y = shared->current_viewport.max.y - line_height - shared->current_property_y;
	line_viewport.max.y = line_viewport.min.y + line_height;

	push_current_viewport(line_viewport) {
		s32 text_width = 0;

		auto font = get_font_at_size(shared->font_collection, font_size);
		ensure_all_chars_present(name, font);
		auto placed_text = with(temporary_allocator, place_text(name, font));
		text_width = placed_text.back().position.max.x;
		label(placed_text, font);

		auto value_viewport = line_viewport;
		value_viewport.min.x += text_width + 2;

		push_current_viewport(value_viewport) {
			float_field(value, get_id(id, location));
		}
	}

	shared->current_property_y += line_height + 2;
}
void draw_property(Span<utf8> name, v3f &value, u64 id, std::source_location location) {
	header(name);

	auto line_viewport = shared->current_viewport;
	line_viewport.min.y = line_viewport.max.y - line_height - shared->current_property_y;
	line_viewport.max.y = line_viewport.min.y + line_height;


	auto x_viewport = line_viewport;
	auto y_viewport = line_viewport;
	auto z_viewport = line_viewport;

	x_viewport.max.x = x_viewport.min.x + line_viewport.size().x / 3;

	y_viewport.min.x = x_viewport.max.x;
	y_viewport.max.x = x_viewport.min.x + line_viewport.size().x * 2 / 3;

	z_viewport.min.x = y_viewport.max.x;
	z_viewport.max.x = line_viewport.max.x;

	push_current_viewport(x_viewport) label("X");
	push_current_viewport(y_viewport) label("Y");
	push_current_viewport(z_viewport) label("Z");

	x_viewport.min.x += font_size;
	y_viewport.min.x += font_size;
	z_viewport.min.x += font_size;

	push_current_viewport(x_viewport) float_field(value.x, get_id(id, location));
	push_current_viewport(y_viewport) float_field(value.y, get_id(id, location));
	push_current_viewport(z_viewport) float_field(value.z, get_id(id, location));

	shared->current_property_y += line_height + 2;
}

void draw_property(Span<utf8> name, quaternion &value, u64 id, std::source_location location) {
	v3f angles = degrees(to_euler_angles(value));
	draw_property(name, angles, id,location);
	value = quaternion_from_euler(radians(angles));
}

void draw_property(Span<utf8> name, List<utf8> &value, u64 id, std::source_location location) {
	header(name);

	tg::Viewport line_viewport = shared->current_viewport;
	line_viewport.min.y = line_viewport.max.y - line_height - shared->current_property_y;
	line_viewport.max.y = line_viewport.min.y + line_height;

	push_current_viewport(line_viewport) text_field(value, get_id(id, location));

	shared->current_property_y += line_height + 2;
}

void draw_property(Span<utf8> name, Texture2D *&value, u64 id, std::source_location location) {
	Span<utf8> extensions[] = {
		u8".png"s,
		u8".jpg"s,
		u8".hdr"s,
	};
	draw_asset_property(name, value ? value->name : u8"null"s, id, location, extensions, [&] (Span<utf8> path) {
		auto new_texture = shared->assets.get_texture_2d(path);
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
