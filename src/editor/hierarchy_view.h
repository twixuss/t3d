#pragma once
#include "window.h"
#include "../blit.h"
#include "../entity.h"
#include "../font.h"
#include "../selection.h"

bool button(t3d::Viewport button_viewport, Span<utf8> text, v4f color) {
	push_current_viewport(button_viewport) {
		t3d::set_viewport(button_viewport);

		t3d::set_shader(blit_color_shader);
		t3d::set_blend(t3d::BlendFunction_disable, {}, {});
		t3d::set_topology(t3d::Topology_triangle_list);
		t3d::set_shader_constants(blit_color_constants, 0);
		t3d::update_shader_constants(blit_color_constants, {.color = color});
		t3d::draw(3);

		draw_text(text);
		
		return mouse_up(0);
	}
	return false;
}

struct HierarchyViewWindow : EditorWindow {
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

		s32 const button_height = 16;
		s32 const button_padding = 2;
		v2s next_pos = viewport.top_left() + v2s{button_padding, -(button_padding + button_height)};

		for_each(entities, [&](Entity &entity) {
			if (entity.flags & Entity_editor) {
				return;
			}

			t3d::Viewport button_viewport;
			button_viewport.position = next_pos;
			button_viewport.size = {viewport.size.x - (u32)button_padding * 2, (u32)button_height};
			
			v4f button_color = V4f(.15);
			if (in_bounds(current_mouse_position, button_viewport.aabb())) {
				button_color = V4f(.2);
			}
			if (&entity == selected_entity) {
				button_color.xy *= 1.5f;
			}
			
			if (button(button_viewport, entity.debug_name, button_color)) {
				selected_entity = &entity;
			}

			next_pos.y -= button_height + button_padding;
		});
	}
};

HierarchyViewWindow *create_hierarchy_view() {
	auto result = create_editor_window<HierarchyViewWindow>();
	result->kind = EditorWindow_hierarchy_view;
	return result;
}
