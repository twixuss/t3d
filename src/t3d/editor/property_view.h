#pragma once
#include "window.h"
#include <t3d/entity.h>
#include "../gui.h"
#include "../selection.h"

struct PropertyView : EditorWindow {
	bool adding_component = false;

	v2u get_min_size() {
		return {160, 160};
	}
	void resize(tg::Viewport viewport) {
		this->viewport = viewport;
	}
	void render() {
		tg::set_render_target(tg::back_buffer);
		gui_panel(middle_color);



		current_property_y = 0;

		switch (selection.kind) {
			case Selection_entity: {
				s32 const add_component_button_height = 16;

				auto add_component_button_viewport = current_viewport;
				add_component_button_viewport.min.y = add_component_button_viewport.max.y - add_component_button_height;
				if (button(add_component_button_viewport, adding_component ? u8"Back to Properties"s : u8"Add Component"s)) {
					adding_component = !adding_component;
				}

				auto content_viewport = current_viewport;
				content_viewport.max.y -= add_component_button_height;
				push_current_viewport(pad(content_viewport)) {
					if (adding_component) {
						aabb<v2s> button_viewport = current_viewport;
						button_viewport.min.y = button_viewport.max.y - 16;

						s32 button_height_plus_padding = 16 + 2;

						for_each(shared_data->component_infos, [&](ComponentUID component_type, ComponentInfo &info) {
							if (button(button_viewport, info.name, component_type)) {
								adding_component = false;
								add_component(*selection.entity, component_type);
							}
							button_viewport.min.y -= button_height_plus_padding;
							button_viewport.max.y -= button_height_plus_padding;
						});
					} else {
						begin_scrollbar((umm)this);
						defer { end_scrollbar((umm)this); };

						draw_property(u8"Name"s,     selection.entity->name);
						draw_property(u8"Position"s, selection.entity->position);
						draw_property(u8"Rotation"s, selection.entity->rotation);
						draw_property(u8"Scale"s,    selection.entity->scale);
						property_separator();

						u32 component_index_in_entity = 0;
						for (auto component : selection.entity->components) {
							defer { ++component_index_in_entity; };

							auto &info = get_component_info(component.type);

							auto x_viewport = current_viewport;
							x_viewport.min.y = current_viewport.max.y - line_height - current_property_y;
							x_viewport.max.y = x_viewport.min.y + line_height;
							x_viewport.min.x = x_viewport.max.x - x_viewport.size().y;

							header(info.name);

							info.draw_properties(info.storage.get(component.index));
							property_separator();

							if (button(x_viewport, u8"X"s, component_index_in_entity)) {
								remove_component(*selection.entity, component);
							}
						}
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
