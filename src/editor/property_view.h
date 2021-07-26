#pragma once
#include "window.h"
#include "../blit.h"
#include "../entity.h"
#include "../gui.h"
#include "../selection.h"

struct PropertyViewWindow : EditorWindow {
	v2u get_min_size() {
		return {160, 160};
	}
	void resize(t3d::Viewport viewport) {
		this->viewport = viewport;
	}
	void render() {
		t3d::set_render_target(t3d::back_buffer);
		t3d::set_viewport(viewport);
		blit(V4f(.1));
		
		current_viewport.x += 2;
		current_viewport.y += 2;
		current_viewport.w -= 4;
		current_viewport.h -= 4;
		
		current_property_y = 0;
			
		push_current_viewport(current_viewport) {
			if (selected_entity) {
				header("Name");
				draw_property(selected_entity->name);

				header("Position");
				draw_property(selected_entity->position);
				
				header("Rotation");
				//draw_property(selected_entity->rotation);
				
				header("Scale");
				draw_property(selected_entity->scale);
			}
		};
	}
};

PropertyViewWindow *create_property_view() {
	return create_editor_window<PropertyViewWindow>(EditorWindow_property_view);
}
