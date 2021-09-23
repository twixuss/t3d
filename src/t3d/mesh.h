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

void draw_mesh(Mesh *mesh);
