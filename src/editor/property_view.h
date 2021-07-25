#pragma once
#include "window.h"
#include "../blit.h"
#include "../entity.h"
#include "../font.h"
#include "../selection.h"

struct PropertyViewWindow : EditorWindow {
	void resize(t3d::Viewport viewport) {
		this->viewport = viewport;
	}
	void render() {
		t3d::set_rasterizer({
			.depth_test = false,
			.depth_write = false,
		});
		t3d::set_blend(t3d::BlendFunction_disable, {}, {});
		t3d::set_topology(t3d::Topology_triangle_list);

		t3d::set_render_target(t3d::back_buffer);
		t3d::set_shader(blit_color_shader);
		t3d::set_shader_constants(blit_color_constants, 0);

		t3d::update_shader_constants(blit_color_constants, {.color = V4f(.1)});
		t3d::set_viewport(viewport);
		t3d::draw(3);

		if (selected_entity) {
			draw_text(to_string(selected_entity->position));
		}
	}
};

PropertyViewWindow *create_property_view() {
	auto result = create_editor_window<PropertyViewWindow>();
	result->kind = EditorWindow_property_view;
	return result;
}
