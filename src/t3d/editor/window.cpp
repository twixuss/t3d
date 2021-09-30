#include "window.h"
#include <t3d/editor.h>

v2u EditorWindow::get_min_size() {
	return _get_min_size(this);
}
void EditorWindow::resize(tg::Viewport viewport) {
	this->viewport = viewport;
	return _resize(this, viewport);
}
void EditorWindow::render() {
	push_viewport(viewport) {
		return _render(this);
	};
}
void EditorWindow::free() {
	return _free(this);
}
void EditorWindow::debug_print() {
	return _debug_print(this);
}
void EditorWindow::serialize(StringBuilder &builder) {
	append_bytes(builder, kind);
	append_bytes(builder, id);
	append_bytes(builder, parent ? parent->id : (u32)-1);
	return _serialize(this, builder);
}
bool EditorWindow::deserialize(Stream &stream) {
	return _deserialize(this, stream);
}

void insert_editor_window(EditorWindowId id, EditorWindow *result) {
	editor->editor_windows.get_or_insert(id) = result;
}

EditorWindowId get_new_editor_window_id() {
	return editor->editor_window_id_counter += 1;
}

void debug_print_editor_window_tabs() {
	for (u32 i = 0; i < editor->debug_print_editor_window_hierarchy_tab; ++i)
		print("  "s);
}

void push_debug_print_editor_window_tab() { ++editor->debug_print_editor_window_hierarchy_tab; }
void pop_debug_print_editor_window_tab() { --editor->debug_print_editor_window_hierarchy_tab; }
