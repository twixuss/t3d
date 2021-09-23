#pragma once
#include "window.h"
#include <t3d/entity.h>
#include <t3d/assets.h>
#include "../gui.h"
#include "../selection.h"

struct FileView : EditorWindow {

	struct Entry {
		List<utf8> path;
		List<utf8> name;
		bool is_directory;
		List<Entry> entries;
	};
	Entry root;

	s32 const button_height = 16;
	s32 const button_padding = 2;
	v2s next_pos;
	s32 tab = 0;

	void add_files(Entry &parent) {
		auto children = get_items_in_directory(with(temporary_allocator, to_pathchars(parent.path, true)));
		for (auto &child : children) {
			Entry result;
			result.is_directory = child.kind == FileItem_directory;
			result.name = to_utf8(child.name);
			result.path = concatenate(parent.path, '/', result.name);

			if (result.is_directory) {
				add_files(result);
			}

			parent.entries.add(result);
		}
	}

	v2u get_min_size() {
		return {160, 160};
	}
	void resize(tg::Viewport viewport) {
		this->viewport = viewport;
	}
	void render() {
		shared->tg->set_render_target(shared->tg->back_buffer);
		gui_panel(middle_color);

		next_pos = v2s{viewport.min.x, viewport.max.y} + v2s{button_padding, -(button_padding + button_height)};
		tab = 0;

		render_entries(root.entries);
	}

	void render_entries(List<Entry> entries) {
		for (auto &entry : entries) {
			tg::Viewport button_viewport;
			button_viewport.min = next_pos;
			button_viewport.min.x += tab * button_height;
			button_viewport.max = v2s{viewport.max.x - button_padding, button_viewport.min.y + button_height};

			push_current_viewport(button_viewport) {
				if (button(entry.name, (umm)&entry)) {
					auto found = shared->assets.textures_2d_by_path.find(entry.path);

					Texture2D *texture = 0;
					if (found) {
						texture = *found;
					} else {
						texture = shared->assets.get_texture_2d(entry.path);
					}

					if (texture) {
						selection.set(texture);
					}
				}

				if (begin_drag_and_drop(DragAndDrop_file)) {
					shared->drag_and_drop_data.set(as_bytes(entry.path));
				}
			}

			next_pos.y -= button_height + button_padding;

			if (entry.is_directory) {
				tab += 1;
				render_entries(entry.entries);
				tab -= 1;
			}
		}
	}
};

FileView *create_file_view() {
	auto result = create_editor_window<FileView>(EditorWindow_file_view);
	result->root.is_directory = true;
	result->root.name = as_list(shared->assets.directory);
	result->root.path = as_list(shared->assets.directory);
	result->add_files(result->root);
	result->name = u8"Files"s;
	return result;
}