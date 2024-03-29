#pragma once
#include "window.h"

struct TabView;

void move(TabView *tab_view, u32 tab_index, TabView *to);

struct DragDropTabInfo {
	TabView *tab_view;
	u32 tab_index;
};

struct TabMove {
	TabView *from;
	TabView *to;
	u32 tab_index;
	u32 direction;
};
List<TabMove> tab_moves;

struct TabView : EditorWindow {
	static constexpr s32 tab_height = 16;

	struct Tab {
		EditorWindow *window;
		bool needs_resize;
	};

	List<Tab> tabs;
	u32 selected_tab;
	v2u get_min_size() {
		return tabs[selected_tab].window->get_min_size() + v2u{0, (u32)tab_height};
	}
	void resize(tg::Rect viewport) {
		this->viewport = viewport;

		for (auto &tab : tabs) {
			tab.needs_resize = true;
		}

		resize_selected_tab();
	}
	void resize_selected_tab() {
		auto tab_viewport = viewport;
		tab_viewport.max.y -= tab_height;
		tabs[selected_tab].window->resize(tab_viewport);
		tabs[selected_tab].needs_resize = false;
	}
	void render() {
		bool highlight_dnd = false;

		auto bar_viewport = viewport;
		bar_viewport.min.y = bar_viewport.max.y - tab_height;
		push_viewport(bar_viewport) {
			gui_panel(background_color);

			if (editor->drag_and_drop_kind == DragAndDrop_tab) {
				highlight_dnd = true;

				if (accept_drag_and_drop(DragAndDrop_tab)) {
					assert(editor->drag_and_drop_data.count == sizeof(DragDropTabInfo));
					auto data = *(DragDropTabInfo *)editor->drag_and_drop_data.data;

					if (data.tab_view != this) {
						tab_moves.add({
							.from = data.tab_view,
							.to = this,
							.tab_index = data.tab_index,
							.direction = (u32)-1,
						});
					}
				}
			}

			s32 tab_start_x = 2;

			auto font = get_font_at_size(app->font_collection, font_size);
			for (u32 tab_index = 0; tab_index < tabs.count; tab_index += 1) {
				auto &tab = tabs[tab_index];
				ensure_all_chars_present(tab.window->name, font);
				auto placed_chars = with(temporary_allocator, place_text(tab.window->name, font));

				auto tab_viewport = bar_viewport;
				tab_viewport.min.x += tab_start_x;
				tab_viewport.max.x = tab_viewport.min.x + placed_chars.back().position.max.x + 4;
				tab_start_x += tab_viewport.size().x + 2;

				push_viewport(tab_viewport) {

					push_button_theme {
						editor->button_theme.color = middle_color;
						editor->button_theme.press_color = default_button_theme.press_color / default_button_theme.color * editor->button_theme.color;
						if (tab_index != selected_tab) {
							editor->button_theme.color             = background_color / default_button_theme.color * default_button_theme.color;
							editor->button_theme.hover_enter_color = background_color / default_button_theme.color * default_button_theme.hover_enter_color;
							editor->button_theme.hover_stay_color  = background_color / default_button_theme.color * default_button_theme.hover_stay_color;
							editor->button_theme.press_color       = background_color / default_button_theme.color * default_button_theme.press_color;
						}
						if (button(tab.window->name, (umm)this + tab_index)) {
							selected_tab = tab_index;
						}
					}

					if (begin_drag_and_drop(DragAndDrop_tab)) {
						DragDropTabInfo data;
						data.tab_view = this;
						data.tab_index = tab_index;
						editor->drag_and_drop_data.set({(u8 *)&data, sizeof(data)});
					}
				}
			}
			if (highlight_dnd) {
				assert(editor->drag_and_drop_data.count == sizeof(DragDropTabInfo));
				auto data = *(DragDropTabInfo *)editor->drag_and_drop_data.data;

				if (data.tab_view != this) {
					gui_panel({.1,1,.1,.2});
				}
			}
		}

		if (tabs[selected_tab].needs_resize) {
			resize_selected_tab();
		}
		tabs[selected_tab].window->render();


		if (highlight_dnd) {

			auto base_viewport = viewport;
			base_viewport.max.y -= tab_height;
			if (in_bounds(app->current_mouse_position, base_viewport)) {

				v2f normalized_mouse_pos = (v2f)(app->current_mouse_position - base_viewport.min) / (v2f)base_viewport.size() * 2 - 1;

				f32 const k = 0.44721359549995793928183473374626f;

				u32 direction;

				if (max(absolute(normalized_mouse_pos)) < k) {
					direction = -1;
				} else {
					switch ((normalized_mouse_pos.x - normalized_mouse_pos.y > 0) + (normalized_mouse_pos.x + normalized_mouse_pos.y > 0) * 2) {
						case 0: direction = 2; break;
						case 1: direction = 3; break;
						case 2: direction = 1; break;
						case 3: direction = 0; break;
					}
				}

				v4f color = {.1,1,.1,.2};
				tg::Rect v = base_viewport;
				switch (direction) {
					case 0: v.min.x = v.center().x; break;
					case 1: v.min.y = v.center().y; break;
					case 2: v.max.x = v.center().x; break;
					case 3: v.max.y = v.center().y; break;
				}

				assert(editor->drag_and_drop_data.count == sizeof(DragDropTabInfo));
				auto data = *(DragDropTabInfo *)editor->drag_and_drop_data.data;

				if (data.tab_view == this && tabs.count == 1) {
				} else {
					push_viewport(v) {
						gui_panel({.1,1,.1,.2});

						if (accept_drag_and_drop(DragAndDrop_tab)) {

							tab_moves.add({
								.from = data.tab_view,
								.to = this,
								.tab_index = data.tab_index,
								.direction = direction,
							});
						}
					}
				}
			}
		}
	}
	void debug_print() {
		for (auto &tab : tabs) {
			tab.window->debug_print();
		}
	}
};

TabView *create_tab_view(EditorWindow *child) {
	auto result = create_editor_window<TabView>(EditorWindow_tab_view);
	result->tabs.add({.window = child});
	child->parent = result;
	result->name = u8"Tab view"s;
	return result;
}
