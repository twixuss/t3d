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
		blit(background_color);
		
		current_viewport.x += 2;
		current_viewport.y += 2;
		current_viewport.w -= 4;
		current_viewport.h -= 4;
		
		current_property_y = 0;
			
		push_current_viewport(current_viewport) {
			if (selected_entity) {
				draw_property(u8"Name"s,     selected_entity->name);
				draw_property(u8"Position"s, selected_entity->position);
				draw_property(u8"Rotation"s, selected_entity->rotation);
				draw_property(u8"Scale"s,    selected_entity->scale);
				property_separator();

				for (auto component : selected_entity->components) {
					header(component_names[component.type]);
					component_property_drawers[component.type](component_storages[component.type].get(component.index));
					property_separator();
				}
			}
		};
	}
};

PropertyViewWindow *create_property_view() {
	return create_editor_window<PropertyViewWindow>(EditorWindow_property_view);
}
