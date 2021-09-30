#include "editor.h"
#include <t3d/app.h>

v2s EditorData::get_mouse_position_in_current_viewport() {
	return v2s{app->window->mouse_position.x, (s32)app->window->client_size.y - app->window->mouse_position.y} - current_viewport.min;
}
