#pragma once
#include <t3d/common.h>
#include <t3d/component.h>
#include <t3d/mesh.h>

struct DataHeader {
	u64 asset_offset;
	u64 asset_size;
	u64 scene_offset;
	u64 scene_size;
};

void serialize_binary(StringBuilder &builder, f32 value);
void serialize_binary(StringBuilder &builder, v3f value);
void serialize_binary(StringBuilder &builder, Texture2D *value);
void serialize_binary(StringBuilder &builder, Mesh *value);

List<u8> serialize_scene_binary();

void escape_string(StringBuilder &builder, Span<utf8> string);
Optional<List<utf8>> unescape_string(Span<utf8> literal);

void serialize_text(StringBuilder &builder, f32 value);
void serialize_text(StringBuilder &builder, v3f value);
void serialize_text(StringBuilder &builder, Texture2D *value);
void serialize_text(StringBuilder &builder, Mesh *value);

List<u8> serialize_scene_text();

bool deserialize_scene_text(Span<utf8> path);

bool deserialize_text(f32 &value, Token *&from, Token *end);
bool deserialize_text(v3f &value, Token *&from, Token *end);
bool deserialize_text(Texture2D *&value, Token *&from, Token *end);
bool deserialize_text(Mesh *&value, Token *&from, Token *end);

bool deserialize_scene_binary(Span<u8> data);

bool deserialize_binary(f32 &value, u8 *&from, u8 *end);
bool deserialize_binary(v3f &value, u8 *&from, u8 *end);
bool deserialize_binary(Texture2D *&value, u8 *&from, u8 *end);
bool deserialize_binary(Mesh *&value, u8 *&from, u8 *end);

#if 0
EditorWindow *deserialize_editor_window(Stream &stream) {
#define read_bytes(value) if (!stream.read(value_as_bytes(value))) { print(Print_error, "Failed to deserialize editor window: no data for field '" #value "'\n"); return 0; }

	EditorWindowKind kind;
	read_bytes(kind);

	EditorWindowId window_id;
	read_bytes(window_id);

	// TODO: make this scalable, like components
	EditorWindow *window;
	switch (kind) {
		case EditorWindow_file_view:      window = create_editor_window<FileView>(kind, window_id); break;
		case EditorWindow_hierarchy_view: window = create_editor_window<HierarchyView>(kind, window_id); break;
		case EditorWindow_property_view:  window = create_editor_window<PropertyView>(kind, window_id); break;
		case EditorWindow_scene_view:     window = create_editor_window<SceneView>(kind, window_id); break;
		case EditorWindow_split_view:     window = create_editor_window<SplitView>(kind, window_id); break;
		case EditorWindow_tab_view:       window = create_editor_window<TabView>(kind, window_id); break;
		default: {
			print(Print_error, "Failed to deserialize editor window: invalid kind (%)\n", (u32)kind);
			return 0;
		}
	}

	EditorWindowId parent_id;
	read_bytes(parent_id);

	if (parent_id != (EditorWindowId)-1) {
		auto found = editor_windows.find(parent_id);
		if (!found) {
			print(Print_error, "Failed to deserialize editor window: parent_id is invalid (%)\n", parent_id);
			return 0;
		}
		window->parent = *found;
	}
#undef read_bytes

	if (!window->deserialize(stream)) {
		return 0;
	}

	return window;
}
constexpr auto window_layout_path = tl_file_string("window_layout"s);

void serialize_window_layout() {
	scoped_allocator(temporary_allocator);

	StringBuilder builder;

	main_window->serialize(builder);

	write_entire_file(window_layout_path, as_bytes(to_string(builder)));
}

bool deserialize_window_layout() {
	auto buffer = with(temporary_allocator, read_entire_file(window_layout_path));
	if (!buffer.data) {
		print(Print_error, "Failed to deserialize window layout: file % does not exist\n", window_layout_path);
		return false;
	}

	auto stream = create_memory_stream(buffer);

	main_window = deserialize_editor_window(*stream);

	return stream->remaining_bytes() == 0 && main_window != 0;
}
#endif
