#pragma once
#include "window.h"
#include "../blit.h"
#include "../entity.h"
#include "../gui.h"
#include "../selection.h"

struct PropertyView : EditorWindow {
	v2u get_min_size() {
		return {160, 160};
	}
	void resize(tg::Viewport viewport) {
		this->viewport = viewport;
	}
	void render() {
		tg::set_render_target(tg::back_buffer);
		blit(middle_color);
		
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
						header(component_info[component.type].name);
						component_info[component.type].draw_properties(component_storages[component.type].get(component.index));
						property_separator();
					}
					break;
				}
				case Selection_texture: {
					header(selection.texture->name);
					auto viewport = current_viewport;
					viewport.max.y -= current_property_y;
					viewport.min.y = viewport.max.y - viewport.size().x * selection.texture->size.y / selection.texture->size.x;
					push_current_viewport(viewport) {
						blit(selection.texture);
					}
					break;
				}
			}
		};
	}
};

PropertyView *create_property_view() {
	auto result = create_editor_window<PropertyView>(EditorWindow_property_view);
	result->name = u8"Properties"s;
	return result;
}
