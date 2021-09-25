#include "window.h"
#include <t3d/editor.h>

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
