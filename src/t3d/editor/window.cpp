#include "window.h"
#include <t3d/shared.h>

void insert_editor_window(EditorWindowId id, EditorWindow *result) {
	shared->editor_windows.get_or_insert(id) = result;
}

EditorWindowId get_new_editor_window_id() {
	return shared->editor_window_id_counter += 1;
}

void debug_print_editor_window_tabs() {
	for (u32 i = 0; i < shared->debug_print_editor_window_hierarchy_tab; ++i)
		print("  "s);
}

void push_debug_print_editor_window_tab() { ++shared->debug_print_editor_window_hierarchy_tab; }
void pop_debug_print_editor_window_tab() { --shared->debug_print_editor_window_hierarchy_tab; }
