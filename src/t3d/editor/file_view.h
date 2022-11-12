#pragma once
#include "window.h"
#include <t3d/entity.h>
#include <t3d/assets.h>
#include "../gui.h"
#include "../selection.h"

struct FileView : EditorWindow {

	struct Entry {
		List<utf8> path;
		Span<utf8> name;
		bool is_directory;
		List<Entry> entries;
	};
	Entry root;

	s32 const button_height = 16;
	s32 const button_padding = 2;
	v2s next_pos;
	s32 tab = 0;

	void add_files(Entry &parent, bool append_directory) {
		auto children = get_items_in_directory(with(temporary_allocator, to_pathchars(parent.path, true)));
		for (auto &child : children) {
			Entry result;
			result.is_directory = child.kind == FileItem_directory;
			if (append_directory) {
				result.path = concatenate(parent.path, u8'/', to_utf8(child.name));
			} else {
				result.path = to_utf8(child.name);
			}
			result.name = parse_path(result.path).name;

			if (result.is_directory) {
				add_files(result, true);
			}

			parent.entries.add(result);
		}
	}

	v2u get_min_size() {
		return {160, 160};
	}
	void resize(tg::Rect viewport) {
		this->viewport = viewport;
	}
	void render() {
		gui_panel(middle_color);

		next_pos = v2s{viewport.min.x, viewport.max.y} + v2s{button_padding, -(button_padding + button_height)};
		tab = 0;

		render_entries(root.entries);
	}

	void render_entries(List<Entry> entries) {
		for (auto &entry : entries) {
			tg::Rect button_viewport;
			button_viewport.min = next_pos;
			button_viewport.min.x += tab * button_height;
			button_viewport.max = v2s{viewport.max.x - button_padding, button_viewport.min.y + button_height};

			push_viewport(button_viewport) {
				if (button(entry.name, (umm)&entry)) {
					auto found = app->assets.textures_2d_by_path.find(entry.path);

					Texture2D *texture = 0;
					if (found) {
						texture = *found;
					} else {
						texture = app->assets.get_texture_2d(entry.path);
					}

					if (texture) {
						selection.set(texture);
					}
				}

				if (begin_drag_and_drop(DragAndDrop_file)) {
					editor->drag_and_drop_data.set(as_bytes(entry.path));
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
	result->root.name = to_list(app->assets.directory);
	result->root.path = to_list(app->assets.directory);
	result->add_files(result->root, false);
	result->name = u8"Files"s;
	return result;
}
