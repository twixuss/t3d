#pragma once
#include "window.h"
#include "input.h"
#include "current.h"

struct SplitView : EditorWindow {
	bool is_sizing;
	bool axis_is_x;
	t3d::Viewport viewport;
	f32 split_t = 0.5f;
	EditorWindow *part1;
	EditorWindow *part2;
	void resize(t3d::Viewport viewport) {
		this->viewport = viewport;
		resize_children();
	}
	void resize_children() {
		t3d::Viewport viewport1 = viewport;
		if (axis_is_x) {
			viewport1.h *= split_t;
			viewport1.h -= 1;
		} else {
			viewport1.w *= split_t;
			viewport1.w -= 1;
		}

		part1->resize(viewport1);

		t3d::Viewport viewport2 = viewport;
		if (axis_is_x) {
			viewport2.y = viewport1.y + viewport1.h + 2;
			viewport2.h = viewport.h - viewport1.h - 2;
		} else {
			viewport2.x = viewport1.x + viewport1.w + 2;
			viewport2.w = viewport.w - viewport1.w - 2;
		}
		part2->resize(viewport2);
	}
	void render() {
		if (mouse_down(0)) {
			f32 const grab_distance = 4;
			if (axis_is_x) {
				s32 bar_position = viewport.y + viewport.h * split_t;
				if (distance((v2f)current_mouse_position, (line_segment<v2f>)line_segment_begin_end(v2s{viewport.x, bar_position}, v2s{viewport.x + (s32)viewport.w, bar_position})) <= grab_distance) {
					is_sizing = true;
				}
			} else {
				s32 bar_position = viewport.x + viewport.w * split_t;
				if (distance((v2f)current_mouse_position, (line_segment<v2f>)line_segment_begin_end(v2s{bar_position, viewport.y}, v2s{bar_position, viewport.y + (s32)viewport.h})) <= grab_distance) {
					is_sizing = true;
				}
			}
			if (is_sizing) {
				lock_input();
			}
		}
		if (mouse_up(0)) {
			is_sizing = false;
			unlock_input();
		}

		if (is_sizing) {
			v2s mouse_position = {::window->mouse_position.x, (s32)::window->client_size.y - ::window->mouse_position.y};
			if (axis_is_x) {
				split_t = clamp(map<f32>(mouse_position.y, viewport.y, viewport.y + viewport.h, 0, 1), 0.1f, 0.9f);
			} else {
				split_t = clamp(map<f32>(mouse_position.x, viewport.x, viewport.x + viewport.w, 0, 1), 0.1f, 0.9f);
			}
			resize_children();
		}

		part1->render();
		part2->render();
	}
};

SplitView *create_split_view(EditorWindow *left, EditorWindow *right, f32 split_t = 0.5f) {
	auto result = create_editor_window<SplitView>();
	result->part1 = left;
	result->part2 = right;
	result->split_t = split_t;
	return result;
}
