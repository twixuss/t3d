#pragma once
#include <t3d/entity.h>
#include "window.h"
#include "../gui.h"
#include "../selection.h"

struct HierarchyView : EditorWindow {
	v2u get_min_size() {
		return {160, 160};
	}
	void resize(tg::Viewport viewport) {
		this->viewport = viewport;
	}
	void render() {
		app->tg->set_render_target(app->tg->back_buffer);

		gui_panel(middle_color);

		s32 const button_height = 16;
		s32 const button_padding = 2;
		v2s next_pos = v2s{viewport.min.x, viewport.max.y} + v2s{button_padding, -(button_padding + button_height)};

		for_each(app->entities, [&](Entity &entity) {
			if (is_editor_entity(entity)) {
				return;
			}

			tg::Viewport button_viewport;
			button_viewport.min = next_pos;
			button_viewport.max = button_viewport.min + v2s{viewport.size().x - button_padding * 2, button_height};


			ButtonTheme theme = default_button_theme;
			if (selection.kind == Selection_entity && &entity == selection.entity) {
				theme.color             *= selection_color;
				theme.hover_enter_color *= selection_color;
				theme.hover_stay_color  *= selection_color;
				theme.press_color       *= selection_color;
			}

			if (button(button_viewport, entity.name, (umm)&entity, theme)) {
				selection.set(&entity);
			}

			next_pos.y -= button_height + button_padding;
		});

		if (mouse_click(0)) {
			selection.unset();
		}
	}
};

HierarchyView *create_hierarchy_view() {
	auto result = create_editor_window<HierarchyView>(EditorWindow_hierarchy_view);
	result->name = u8"Hierarchy"s;
	return result;
}
