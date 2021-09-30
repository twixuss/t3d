#pragma once
#include <t3d/entity.h>
#include <t3d/editor/window.h>
#include <t3d/gui.h>
#include <t3d/selection.h>

struct HierarchyView : EditorWindow {
	v2u get_min_size() {
		return {160, 160};
	}
	void resize(tg::Viewport viewport) {
		this->viewport = viewport;
	}
	void render() {
		gui_panel(middle_color);

		s32 const button_height = 16;
		tg::Viewport button_viewport;
		button_viewport.min.x = viewport.min.x;
		button_viewport.min.y = viewport.max.y - button_height;
		button_viewport.max.x = viewport.max.x;
		button_viewport.max.y = viewport.max.y;

		for_each(app->current_scene->entities, [&](Entity &entity) {
			if (is_editor_entity(entity)) {
				return;
			}

			push_button_theme {
				editor->button_theme.top_padding = 2;
				editor->button_theme.left_padding = 2;
				editor->button_theme.right_padding = 2;
				if (selection.kind == Selection_entity && &entity == selection.entity) {
					editor->button_theme.color             *= selection_color;
					editor->button_theme.hover_enter_color *= selection_color;
					editor->button_theme.hover_stay_color  *= selection_color;
					editor->button_theme.press_color       *= selection_color;
				}

				push_viewport(button_viewport) {
					if (button(entity.name, (umm)&entity)) {
						selection.set(&entity);
					}
				}
			}

			button_viewport.min.y -= button_height;
			button_viewport.max.y -= button_height;
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
