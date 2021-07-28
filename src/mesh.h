#pragma once
#include "../include/t3d.h"
#include <tl/mesh.h>

struct Mesh {
	t3d::VertexBuffer *vertex_buffer;
	t3d::IndexBuffer *index_buffer;
	u32 index_count;
	List<utf8> name;
};

MaskedBlockList<Mesh, 256> meshes;
HashMap<Span<utf8>, Mesh *> meshes_by_name;

MaskedBlockList<Scene3D, 256> scenes3d;
HashMap<Span<utf8>, Scene3D *> scenes3d_by_name;

Mesh *create_mesh(tl::CommonMesh mesh) {
	Mesh result = {};
	result.vertex_buffer = t3d::create_vertex_buffer(
		as_bytes(mesh.vertices),
		{
			t3d::Element_f32x3, // position
			t3d::Element_f32x3, // normal
			t3d::Element_f32x4, // color
			t3d::Element_f32x2, // uv
		}
	);

	result.index_buffer = t3d::create_index_buffer(as_bytes(mesh.indices), sizeof(u32));

	result.index_count = mesh.indices.size;

	auto pointer = &meshes.add();
	*pointer = result;
	return pointer;
}
Mesh *load_mesh(Span<utf8> path) {
	if (auto parse_result = parse_glb_from_file(path)) {
		auto result = create_mesh(parse_result.value.meshes[0]);
		meshes_by_name.get_or_insert(path) = result;
		return result;
	} else {
		print("Failed to parse mesh");
		invalid_code_path();
		return {};
	}
}

Mesh *get_submesh(Scene3D &scene, Span<utf8> name) {
	auto result = create_mesh(*scene.get_node(name)->mesh);

	result->name.reserve(scene.name.size + 1 + name.size);
	result->name.add(scene.name);
	result->name.add(':');
	result->name.add(name);
	meshes_by_name.get_or_insert(result->name) = result;
	return result;
}

void draw_mesh(Mesh *mesh) {
	if (!mesh) {
		return;
	}
	t3d::set_vertex_buffer(mesh->vertex_buffer);
	t3d::set_index_buffer(mesh->index_buffer);
	t3d::draw_indexed(mesh->index_count);
}
