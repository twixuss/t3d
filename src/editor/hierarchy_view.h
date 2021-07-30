#pragma once
#include "window.h"
#include "../blit.h"
#include "../entity.h"
#include "../gui.h"
#include "../selection.h"

struct HierarchyViewWindow : EditorWindow {
	v2u get_min_size() {
		return {160, 160};
	}
	void resize(t3d::Viewport viewport) {
		this->viewport = viewport;
	}
	void render() {
		t3d::set_render_target(t3d::back_buffer);

		blit(background_color);

		s32 const button_height = 16;
		s32 const button_padding = 2;
		v2s next_pos = v2s{viewport.min.x, viewport.max.y} + v2s{button_padding, -(button_padding + button_height)};

		for_each(entities, [&](Entity &entity) {
			if (entity.flags & Entity_editor) {
				return;
			}

			t3d::Viewport button_viewport;
			button_viewport.min = next_pos;
			button_viewport.max = button_viewport.min + v2s{viewport.size().x - button_padding * 2, button_height};
			

			ButtonTheme theme = default_button_theme;
			if (selection.kind == Selection_entity && &entity == selection.entity) {
				theme.color.xy *= 1.5f;
				theme.hovered_color.xy *= 1.5f;
				theme.pressed_color.xy *= 1.5f;
			}

			if (button(button_viewport, entity.name, theme)) {
				selection.set(&entity);
			}

			next_pos.y -= button_height + button_padding;
		});

		if (mouse_click(0)) {
			selection.unset();
		}
	}
};

HierarchyViewWindow *create_hierarchy_view() {
	return create_editor_window<HierarchyViewWindow>(EditorWindow_hierarchy_view);
}
