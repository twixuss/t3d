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

		

		current_property_y = 0;
			
		switch (selection.kind) {
			case Selection_entity: {
				s32 const bar_height = 16;

				auto bar_viewport = current_viewport;
				bar_viewport.min.y = bar_viewport.max.y - bar_height;
				if (button(bar_viewport, u8"Add Component"s)) {
			
				}

				auto component_viewport = current_viewport;
				component_viewport.max.y -= bar_height;
				push_current_viewport(pad(component_viewport)) {
					begin_scrollbar((umm)this);
					defer { end_scrollbar((umm)this); };

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
	}
};

PropertyView *create_property_view() {
	auto result = create_editor_window<PropertyView>(EditorWindow_property_view);
	result->name = u8"Properties"s;
	return result;
}
