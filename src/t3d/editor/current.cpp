#include "current.h"
#include <t3d/app.h>

ViewportPusher::ViewportPusher(tg::Viewport new_viewport) {
	old_viewport = app->current_viewport;
	old_scissor  = app->current_scissor;

	app->current_viewport = new_viewport;
	app->current_scissor = intersection(new_viewport, app->current_scissor);

	has_area = volume(app->current_scissor) > 0;

	if (has_area) {
		app->tg->set_viewport(new_viewport);
		app->tg->set_scissor(app->current_scissor);
	}
}
ViewportPusher::~ViewportPusher() {
	app->current_viewport = old_viewport;
	app->current_scissor = old_scissor;
	app->tg->set_viewport(old_viewport);
	app->tg->set_scissor(old_scissor);
}
ViewportPusher::operator bool(){ return has_area; }
