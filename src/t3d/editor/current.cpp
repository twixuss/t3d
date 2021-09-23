#include "current.h"
#include <t3d/shared.h>

ViewportPusher::ViewportPusher(tg::Viewport new_viewport) {
	old_viewport = shared->current_viewport;
	old_scissor  = shared->current_scissor;

	shared->current_viewport = new_viewport;
	shared->current_scissor = intersection(new_viewport, shared->current_scissor);

	has_area = volume(shared->current_scissor) > 0;

	if (has_area) {
		shared->tg->set_viewport(new_viewport);
		shared->tg->set_scissor(shared->current_scissor);
	}
}
ViewportPusher::~ViewportPusher() {
	shared->current_viewport = old_viewport;
	shared->current_scissor = old_scissor;
	shared->tg->set_viewport(old_viewport);
	shared->tg->set_scissor(old_scissor);
}
ViewportPusher::operator bool(){ return has_area; }
