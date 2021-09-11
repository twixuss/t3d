#pragma once
#include "common.h"

PreciseTimer frame_timer;
f32 frame_time = 1 / 60.0f;
f32 max_frame_time = 0.1f;
f32 time;
u32 frame_index;

void update_time() {
	frame_time = min(max_frame_time, reset(frame_timer));
	time += frame_time;
	frame_index += 1;
}
