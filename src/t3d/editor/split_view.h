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

		if (horizontal) {
			result.x = max(size1.x, size2.x);
			result.y = size1.y + size2.y + 2;
		} else {
			result.x = size1.x + size2.x + 2;
			result.y = max(size1.y, size2.y);
		}
		return result;
	}
	void resize(tg::Rect viewport) {
		resize_children();
	}
	void resize_children() {
		// add 1 to account for half of the split bar
		auto size1 = part1->get_min_size() + 1;
		auto size2 = part2->get_min_size() + 1;

		if(horizontal) {
			clamped_split_t = clamp(split_t, (f32)size1.y / viewport.size().y, 1 - (f32)size2.y / viewport.size().y);
		} else {
			clamped_split_t = clamp(split_t, (f32)size1.x / viewport.size().x, 1 - (f32)size2.x / viewport.size().x);
		}

		tg::Rect viewport1 = viewport;
		if (horizontal) {
			viewport1.max.y = lerp<f32>(viewport1.min.y, viewport1.max.y, clamped_split_t) - 1;
		} else {
			viewport1.max.x = lerp<f32>(viewport1.min.x, viewport1.max.x, clamped_split_t) - 1;
		}

		part1->resize(viewport1);

		tg::Rect viewport2 = viewport;
		if (horizontal) {
			viewport2.min.y = viewport1.max.y + 2;
			viewport2.max.y = viewport.max.y;
		} else {
			viewport2.min.x = viewport1.max.x + 2;
			viewport2.max.x = viewport.max.x;
		}
		part2->resize(viewport2);
	}
	void render() {
		begin_input_user();

		line_segment<v2f> bar_line;
		s32 bar_position;
		if (horizontal) {
			bar_position = viewport.min.y + viewport.size().y * clamped_split_t;
			bar_line = (line_segment<v2f>)line_segment_begin_end(v2s{viewport.min.x, bar_position}, v2s{viewport.min.x + (s32)viewport.size().x, bar_position});
		} else {
			bar_position = viewport.min.x + viewport.size().x * clamped_split_t;
			bar_line = (line_segment<v2f>)line_segment_begin_end(v2s{bar_position, viewport.min.y}, v2s{bar_position, viewport.min.y + (s32)viewport.size().y});
		}
		f32 const grab_distance = 4;
		Cursor cursor = horizontal ? Cursor_vertical : Cursor_horizontal;
		if (distance((v2f)app->current_mouse_position, bar_line) <= grab_distance) {
			if (mouse_down(0, {.anywhere = true})) {
				is_sizing = true;
				lock_input();
				if (horizontal) {
					grab_offset = bar_position - app->current_mouse_position.y;
				} else {
					grab_offset = bar_position - app->current_mouse_position.x;
				}
			}
		} else if (!is_sizing) {
			if (app->current_cursor != Cursor_none) {
				cursor = Cursor_default;
			}
		}
		//if ((cursor == Cursor_vertical && app->current_cursor == Cursor_horizontal) || (cursor == Cursor_horizontal && app->current_cursor == Cursor_vertical)) {
		//	app->current_cursor = Cursor_horizontal_and_vertical;
		//} else
		if (app->current_cursor == Cursor_default) {
			app->current_cursor = cursor;
		}

		if (is_sizing) {
			if (mouse_up(0)) {
				is_sizing = false;
				unlock_input();
			}
		}

		if (is_sizing) {
			v2s mouse_position = {app->window->mouse_position.x, (s32)app->window->client_size.y - app->window->mouse_position.y};
			if (horizontal) {
				split_t = map<f32>(mouse_position.y + grab_offset, viewport.min.y, viewport.min.y + viewport.size().y, 0, 1);
			} else {
				split_t = map<f32>(mouse_position.x + grab_offset, viewport.min.x, viewport.min.x + viewport.size().x, 0, 1);
			}
			resize_children();
		}

		push_viewport(part1->viewport) part1->render();
		push_viewport(part2->viewport) part2->render();
	}

	void debug_print() {
		debug_print_editor_window_tabs();
		print("part1:\n");
		push_debug_print_editor_window_tab();
		part1->debug_print();
		pop_debug_print_editor_window_tab();

		debug_print_editor_window_tabs();
		print("part2:\n");
		push_debug_print_editor_window_tab();
		part2->debug_print();
		pop_debug_print_editor_window_tab();
	}

	EditorWindow *&get_part(EditorWindow *window) {
		if (window == part1) {
			return part1;
		}
		assert(window == part2);
		return part2;
	}
	EditorWindow *&get_other_part(EditorWindow *window) {
		if (window == part1) {
			return part2;
		}
		assert(window == part2);
		return part1;
	}
	void replace_child(EditorWindow *old, EditorWindow *_new) {
		get_part(old) = _new;
		_new->parent = this;
	}
};

struct CreateSplitViewParams {
	f32 split_t = 0.5f;
	bool horizontal = false;

};

SplitView *create_split_view(EditorWindow *left, EditorWindow *right, CreateSplitViewParams params = {}) {
	auto result = create_editor_window<SplitView>(EditorWindow_split_view);
	left->parent = result;
	right->parent = result;
	result->part1 = left;
	result->part2 = right;
	result->split_t = params.split_t;
	result->clamped_split_t = params.split_t;
	result->horizontal = params.horizontal;
	result->name = u8"Split view"s;
	return result;
}
