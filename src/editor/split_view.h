#pragma once
#include "window.h"
#include "input.h"
#include "current.h"

struct SplitView : EditorWindow {
	bool is_sizing;
	bool horizontal;
	f32 split_t = 0.5f;
	f32 clamped_split_t;
	EditorWindow *part1;
	EditorWindow *part2;
	f32 grab_offset;

	v2u get_min_size() {
		v2u result;

		auto size1 = part1->get_min_size();
		auto size2 = part2->get_min_size();

		if(horizontal) {
			result.x = max(size1.x, size2.x);
			result.y = size1.y + size2.y;
		} else {
			result.x = size1.x + size2.x;
			result.y = max(size1.y, size2.y);
		}
		return result;
	}
	void resize(t3d::Viewport viewport) {
		resize_children();
	}
	void resize_children() {
		// add 1 to account for half of the split bar
		auto size1 = part1->get_min_size() + 1;
		auto size2 = part2->get_min_size() + 1;
		
		if(horizontal) {
			clamped_split_t = clamp(split_t, (f32)size1.y / viewport.h, 1 - (f32)size2.y / viewport.h);
		} else {
			clamped_split_t = clamp(split_t, (f32)size1.x / viewport.w, 1 - (f32)size2.x / viewport.w);
		}

		t3d::Viewport viewport1 = viewport;
		if (horizontal) {
			viewport1.h *= clamped_split_t;
			viewport1.h -= 1;
		} else {
			viewport1.w *= clamped_split_t;
			viewport1.w -= 1;
		}

		part1->resize(viewport1);

		t3d::Viewport viewport2 = viewport;
		if (horizontal) {
			viewport2.y = viewport1.y + viewport1.h + 2;
			viewport2.h = viewport.h - viewport1.h - 2;
		} else {
			viewport2.x = viewport1.x + viewport1.w + 2;
			viewport2.w = viewport.w - viewport1.w - 2;
		}
		part2->resize(viewport2);
	}
	void render() {
		line_segment<v2f> bar_line;
		s32 bar_position;
		if (horizontal) {
			bar_position = viewport.y + viewport.h * clamped_split_t;
			bar_line = (line_segment<v2f>)line_segment_begin_end(v2s{viewport.x, bar_position}, v2s{viewport.x + (s32)viewport.w, bar_position});
		} else {
			bar_position = viewport.x + viewport.w * clamped_split_t;
			bar_line = (line_segment<v2f>)line_segment_begin_end(v2s{bar_position, viewport.y}, v2s{bar_position, viewport.y + (s32)viewport.h});
		}
		f32 const grab_distance = 4;
		Cursor cursor = horizontal ? Cursor_vertical : Cursor_horizontal;
		if (distance((v2f)current_mouse_position, bar_line) <= grab_distance) {
			if (mouse_down(0, {.anywhere = true})) {
				is_sizing = true;
				lock_input();
				if (horizontal) {
					grab_offset = bar_position - current_mouse_position.y;
				} else {
					grab_offset = bar_position - current_mouse_position.x;
				}
			}
		} else if (!is_sizing) {
			cursor = Cursor_default;
		}
		//if ((cursor == Cursor_vertical && current_cursor == Cursor_horizontal) || (cursor == Cursor_horizontal && current_cursor == Cursor_vertical)) {
		//	current_cursor = Cursor_horizontal_and_vertical;
		//} else 
		if (current_cursor == Cursor_default) {
			current_cursor = cursor;
		}

		if (mouse_up_no_lock(0, {.anywhere = true})) {
			is_sizing = false;
			unlock_input();
		}

		if (is_sizing) {
			v2s mouse_position = {::window->mouse_position.x, (s32)::window->client_size.y - ::window->mouse_position.y};
			if (horizontal) {
				split_t = map<f32>(mouse_position.y + grab_offset, viewport.y, viewport.y + viewport.h, 0, 1);
			} else {
				split_t = map<f32>(mouse_position.x + grab_offset, viewport.x, viewport.x + viewport.w, 0, 1);
			}
			resize_children();
		}

		push_current_viewport(part1->viewport) part1->render();
		push_current_viewport(part2->viewport) part2->render();
	}
};

struct CreateSplitViewParams {
	f32 split_t = 0.5f;
	bool horizontal = false;

};

SplitView *create_split_view(EditorWindow *left, EditorWindow *right, CreateSplitViewParams params = {}) {
	auto result = create_editor_window<SplitView>(EditorWindow_split_view);
	result->part1 = left;
	result->part2 = right;
	result->split_t = params.split_t;
	result->clamped_split_t = params.split_t;
	result->horizontal = params.horizontal;
	return result;
}
