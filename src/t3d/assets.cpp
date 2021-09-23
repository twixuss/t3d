#include "assets.h"
#include <t3d/shared.h>

Span<u8> Assets::get_asset_data(Span<utf8> path) {
	if (shared->is_editor) {
		return with(temporary_allocator, read_entire_file(to_pathchars(concatenate(directory, '/', path), true)));
	} else {
		auto found = asset_path_to_data.find(path);
		assert_always(found, "Asset '%' was not found", path);
		return *found;
	}
}

Texture2D *Assets::get_texture_2d(Span<utf8> path) {
	auto found = textures_2d_by_path.find(path);
	if (found) {
		return *found;
	}

	print(Print_info, "Loading texture %.\n", path);
	auto result = shared->tg->load_texture_2d(get_asset_data(path), {.generate_mipmaps = true});

	if (!result) {
		return 0;
	}
	result->serializable = true;
	result->name.set(path);

	textures_2d_by_path.get_or_insert(result->name) = result;
	return result;
}

Mesh *Assets::create_mesh(tl::CommonMesh &mesh) {
	Mesh result = {};
	result.vertex_buffer = shared->tg->create_vertex_buffer(
		as_bytes(mesh.vertices),
		{
			tg::Element_f32x3, // position
			tg::Element_f32x3, // normal
			tg::Element_f32x4, // color
			tg::Element_f32x2, // uv
		}
	);

	result.index_buffer = shared->tg->create_index_buffer(as_bytes(mesh.indices), sizeof(u32));

	result.index_count = mesh.indices.size;

	result.positions.reserve(mesh.vertices.size);
	for (auto &vertex : mesh.vertices) {
		result.positions.add(vertex.position);
	}

	result.indices = copy(mesh.indices);

	auto pointer = &meshes.add();
	*pointer = result;
	return pointer;
}
