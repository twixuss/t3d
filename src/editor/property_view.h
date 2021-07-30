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
		
		current_viewport.min += 2;
		current_viewport.max -= 2;
		
		current_property_y = 0;
			
		push_current_viewport(current_viewport) {
			switch (selection.kind) {
				case Selection_entity: {
					draw_property(u8"Name"s,     selection.entity->name);
					draw_property(u8"Position"s, selection.entity->position);
					draw_property(u8"Rotation"s, selection.entity->rotation);
					draw_property(u8"Scale"s,    selection.entity->scale);
					property_separator();

					for (auto component : selection.entity->components) {
						header(component_names[component.type]);
						component_functions[component.type].draw_properties(component_storages[component.type].get(component.index));
						property_separator();
					}
					break;
				}
				case Selection_texture: {
					header(selection.texture->name);
					auto viewport = current_viewport;
					viewport.max.y -= current_property_y;
					viewport.min.y = viewport.max.y - viewport.size().x * selection.texture->texture->size.y / selection.texture->texture->size.x;
					push_current_viewport(viewport) {
						blit(selection.texture->texture);
					}
					break;
				}
			}
		};
	}
};

PropertyViewWindow *create_property_view() {
	return create_editor_window<PropertyViewWindow>(EditorWindow_property_view);
}
