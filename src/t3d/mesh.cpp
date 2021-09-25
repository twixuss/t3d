#include "mesh.h"
#include <t3d/app.h>

void draw_mesh(Mesh *mesh) {
	if (!mesh) {
		return;
	}
	app->tg->set_vertex_buffer(mesh->vertex_buffer);
	app->tg->set_index_buffer(mesh->index_buffer);
	app->tg->draw_indexed(mesh->index_count);
}
