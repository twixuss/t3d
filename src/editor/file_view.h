#pragma once
#include "window.h"
#include "../blit.h"
#include "../entity.h"
#include "../gui.h"
#include "../selection.h"

struct FileViewWindow : EditorWindow {

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
	void resize(t3d::Viewport viewport) {
		this->viewport = viewport;
	}
	void render() {
		t3d::set_render_target(t3d::back_buffer);

		blit(background_color);

		next_pos = v2s{viewport.min.x, viewport.max.y} + v2s{button_padding, -(button_padding + button_height)};
		tab = 0;

		render_entries(root.entries);
	}

	void render_entries(List<Entry> entries) {
		for (auto &entry : entries) {
			t3d::Viewport button_viewport;
			button_viewport.min = next_pos;
			button_viewport.min.x += tab * button_height;
			button_viewport.max = v2s{viewport.max.x - button_padding, button_viewport.min.y + button_height};

			push_current_viewport(button_viewport) {
				if (button(entry.name)) {
					auto found = find_if(assets.textures.all, [&] (Texture &texture) {
						return texture.name == entry.path;
					});

					if (!found) {
						found = assets.textures.get(entry.path);
					}

					if (found) {
						selection.set(found);
					}
				}

				if (begin_drag_and_drop()) {
					print("begin_drag_and_drop\n");

					set_drag_and_drop_file(entry.path);
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

FileViewWindow *create_file_view() {
	auto result = create_editor_window<FileViewWindow>(EditorWindow_file_view);
	result->root.is_directory = true;
	result->root.name = as_list(u8"../data"s);
	result->root.path = as_list(u8"../data"s);
	result->add_files(result->root);
	return result;
}
