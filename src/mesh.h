#pragma once
#include "../include/t3d.h"
#include <tl/mesh.h>

struct Mesh {
	t3d::VertexBuffer *vertex_buffer;
	t3d::IndexBuffer *index_buffer;
	u32 index_count;
};

Mesh *create_mesh(tl::CommonMesh mesh) {
	auto result = current_allocator.allocate<Mesh>();
	result->vertex_buffer = t3d::create_vertex_buffer(
		as_bytes(mesh.vertices),
		{
			t3d::Element_f32x3, // position
			t3d::Element_f32x3, // normal
			t3d::Element_f32x4, // color
			t3d::Element_f32x2, // uv
		}
	);

	result->index_buffer = t3d::create_index_buffer(as_bytes(mesh.indices), sizeof(u32));

	result->index_count = mesh.indices.size;

	return result;
}
Mesh *load_mesh(Span<filechar> path) {
	if (auto parse_result = parse_glb_from_file(path)) {
		return create_mesh(parse_result.value.meshes[0]);
	} else {
		print("Failed to parse mesh");
		invalid_code_path();
		return {};
	}
}

void draw_mesh(Mesh *mesh) {
	assert(mesh);
	t3d::set_vertex_buffer(mesh->vertex_buffer);
	t3d::set_index_buffer(mesh->index_buffer);
	t3d::draw_indexed(mesh->index_count);
}
