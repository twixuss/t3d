#pragma once
#include <t3d/common.h>

struct ViewportPusher {
	tg::Viewport old_viewport;
	tg::Viewport old_scissor;
	bool has_area;
	ViewportPusher(tg::Viewport new_viewport);
	~ViewportPusher();
	operator bool();
};
#define push_current_viewport(new_viewport) tl_push(ViewportPusher, new_viewport)
