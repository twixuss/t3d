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
	void resize(tg::Rect viewport) {
		this->viewport = viewport;
	}
	void render() {
		gui_panel(middle_color);

		editor->current_property_y = 0;

		switch (selection.kind) {
			case Selection_entity: {
				s32 const add_component_button_height = 16;

				auto add_component_button_viewport = editor->current_viewport;
				add_component_button_viewport.min.y = add_component_button_viewport.max.y - add_component_button_height;
				push_viewport(add_component_button_viewport) {
					if (button(adding_component ? u8"Back to Properties"s : u8"Add Component"s)) {
						adding_component = !adding_component;
					}
				}

				auto content_viewport = editor->current_viewport;
				content_viewport.max.y -= add_component_button_height;
				push_viewport(pad(content_viewport)) {
					if (adding_component) {
						aabb<v2s> button_viewport = editor->current_viewport;
						button_viewport.min.y = button_viewport.max.y - 16;

						s32 button_height_plus_padding = 16 + 2;

						for_each(app->component_infos, [&](Uid component_type, ComponentInfo &info) {
							push_viewport(button_viewport) {
								if (button(info.name, component_type.value)) {
									adding_component = false;
									add_component(*selection.entity, component_type);
								}
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

							auto &info = get_component_info(component.type_uid);

							auto x_viewport = editor->current_viewport;
							x_viewport.min.y = editor->current_viewport.max.y - line_height - editor->current_property_y;
							x_viewport.max.y = x_viewport.min.y + line_height;
							x_viewport.min.x = x_viewport.max.x - x_viewport.size().y;

							header(info.name);

							info.draw_properties(selection.entity->scene->get_component_data(component));
							property_separator();

							push_viewport(x_viewport) {
								if (button(u8"X"s, component_index_in_entity)) {
									remove_component(*selection.entity, component);
								}
							}
						}
					}
				}

				break;
			}
			case Selection_texture: {
				header(selection.texture->name);
				auto viewport = editor->current_viewport;
				viewport.max.y -= editor->current_property_y;
				viewport.min.y = viewport.max.y - viewport.size().x * selection.texture->size.y / selection.texture->size.x;
				push_viewport(viewport) {
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
