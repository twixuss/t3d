#include "mesh.h"
#include <t3d/shared.h>

void draw_mesh(Mesh *mesh) {
	if (!mesh) {
		return;
	}
	shared->tg->set_vertex_buffer(mesh->vertex_buffer);
	shared->tg->set_index_buffer(mesh->index_buffer);
	shared->tg->draw_indexed(mesh->index_count);
}
