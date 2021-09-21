#pragma once
#include "common.h"
#include <tl/mesh.h>

struct Mesh {
	tg::VertexBuffer *vertex_buffer;
	tg::IndexBuffer *index_buffer;
	u32 index_count;
	List<utf8> name;
	List<v3f> positions;
	List<u32> indices;
};

inline void draw_mesh(Mesh *mesh) {
	if (!mesh) {
		return;
	}
	tg::set_vertex_buffer(mesh->vertex_buffer);
	tg::set_index_buffer(mesh->index_buffer);
	tg::draw_indexed(mesh->index_count);
}
