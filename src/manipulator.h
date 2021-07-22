#pragma once
#include "../current.h"
#include "../debug.h"
#include "../input.h"

enum ManipulateKind {
	Manipulate_position,
	Manipulate_rotation,
	Manipulate_scale,
};

struct ManipulatedTransform {
	v3f position;
	quaternion rotation;
	v3f scale;
};

struct ManipulatorDrawRequest {
	ManipulateKind kind;
	u8 highlighted_part_index;
	v3f position;
	quaternion rotation;
	v3f scale;
	f32 size;
};

List<ManipulatorDrawRequest> manipulator_draw_requests;

inline static constexpr u8 null_manipulator_part = -1;

struct ManipulatorState {
	u8 dragging_part_index = null_manipulator_part;
	v3f drag_offset;
	v3f original_click_position;
	f32 previous_angle;
	v3f rotation_axis;
	ray<v2f> tangent;
	v2f start_mouse_position;
	v2f accumulated_mouse_delta;
	ManipulatedTransform original_transform;
};

struct ManipulatorStateKey {
	u32 id;
	Camera *camera;
	std::source_location location;
};

umm get_hash(ManipulatorStateKey const &key) {
	return key.id * (umm)954277 + key.location.column() * (umm)152753 + key.location.line() * (umm)57238693 + (umm)key.location.file_name();
}
bool operator==(ManipulatorStateKey const &a, ManipulatorStateKey const &b) {
	return a.id == b.id && a.camera == b.camera && a.location == b.location;
}

StaticHashMap<ManipulatorStateKey, ManipulatorState, 256> manipulator_states;

ManipulatedTransform manipulate_transform(v3f position, quaternion rotation, v3f scale, ManipulateKind kind, u32 id = 0, std::source_location source_location = std::source_location::current()) {
	ManipulatedTransform manipulated_transform = {
		.position = position,
		.rotation = rotation,
		.scale = scale,
	};

	ManipulatorDrawRequest draw_request = {};
	draw_request.scale = {1,1,1};

	auto &state = manipulator_states.get_or_insert({.id = id, .camera = current_camera, .location = source_location});

	f32 const handle_size_scale = 0.25f;
	f32 const handle_grab_thickness = 0.05f;
	f32 handle_size = handle_size_scale * current_camera->fov / (pi * 0.5f);
	
	auto camera_forward = current_camera_entity->rotation * v3f{0,0,-1};
	auto camera_forward_dot_handle_direction = dot(position - current_camera_entity->position, camera_forward);
	f32 handle_size_scaled_by_distance = handle_size * camera_forward_dot_handle_direction;

	auto mouse_position = get_mouse_position_in_current_viewport();
	
	u8 closest_element = null_manipulator_part;
	
	auto save_transform = [&]() {
		state.original_transform.scale = scale;
		state.original_transform.position = position;
		state.original_transform.rotation = rotation;
	};
	
	m4 camera_to_world_matrix = inverse(current_camera->world_to_camera_matrix);

	switch (kind) {
		case Manipulate_position:
		case Manipulate_scale: {
			//m4 handle_matrix = m4::translation(position) * m4::rotation_r_zxy(rotation) * m4::scale(handle_size * dot(position - current_camera_entity.position, m4::rotation_r_zxy(current_camera_entity.rotation) * v3f{0,0,-1}));
			m4 handle_matrix = m4::translation(position) * (m4)rotation * m4::scale(handle_size_scaled_by_distance);

			v3f handle_viewport_position = world_to_viewport(handle_matrix * v4f{0,0,0,1});
	
			Array<v3f, 3> handle_world_axis_tips = {
				(handle_matrix * v4f{1,0,0,1}).xyz,
				(handle_matrix * v4f{0,1,0,1}).xyz,
				(handle_matrix * v4f{0,0,1,1}).xyz
			};

			Array<v2f, 3> handle_viewport_axis_tips = {
				world_to_viewport(handle_world_axis_tips[0]).xy,
				world_to_viewport(handle_world_axis_tips[1]).xy,
				world_to_viewport(handle_world_axis_tips[2]).xy
			};

			Array<v2f, 12> handle_viewport_plane_points = {
				world_to_viewport((handle_matrix * v4f{0,   0.4f,0.4f,1}).xyz).xy,
				world_to_viewport((handle_matrix * v4f{0,   0.4f,0.8f,1}).xyz).xy,
				world_to_viewport((handle_matrix * v4f{0,   0.8f,0.4f,1}).xyz).xy,
				world_to_viewport((handle_matrix * v4f{0,   0.8f,0.8f,1}).xyz).xy,
				world_to_viewport((handle_matrix * v4f{0.4f,0,   0.4f,1}).xyz).xy,
				world_to_viewport((handle_matrix * v4f{0.4f,0,   0.8f,1}).xyz).xy,
				world_to_viewport((handle_matrix * v4f{0.8f,0,   0.4f,1}).xyz).xy,
				world_to_viewport((handle_matrix * v4f{0.8f,0,   0.8f,1}).xyz).xy,
				world_to_viewport((handle_matrix * v4f{0.4f,0.4f,0,   1}).xyz).xy,
				world_to_viewport((handle_matrix * v4f{0.4f,0.8f,0,   1}).xyz).xy,
				world_to_viewport((handle_matrix * v4f{0.8f,0.4f,0,   1}).xyz).xy,
				world_to_viewport((handle_matrix * v4f{0.8f,0.8f,0,   1}).xyz).xy,
			};

			f32 closest_dist = max_value<f32>;
			for (u32 axis_index = 0; axis_index != 3; ++axis_index) {
				f32 dist = distance(line_segment_begin_end(handle_viewport_axis_tips[axis_index], handle_viewport_position.xy), (v2f)mouse_position);
				if (dist < closest_dist) {
					closest_dist = dist;
					closest_element = axis_index;
				}

				Array<line_segment<v2f>, 4> plane_lines = {
					line_segment_begin_end(handle_viewport_plane_points[axis_index*4 + 0], handle_viewport_plane_points[axis_index*4 + 1]),
					line_segment_begin_end(handle_viewport_plane_points[axis_index*4 + 2], handle_viewport_plane_points[axis_index*4 + 3]),
					line_segment_begin_end(handle_viewport_plane_points[axis_index*4 + 0], handle_viewport_plane_points[axis_index*4 + 2]),
					line_segment_begin_end(handle_viewport_plane_points[axis_index*4 + 1], handle_viewport_plane_points[axis_index*4 + 3]),
				};
				for (u32 line_index = 0; line_index != 4; ++line_index) {
					dist = min(dist, distance(plane_lines[line_index], (v2f)mouse_position));
				}
				if (dist < closest_dist) {
					closest_dist = dist;
					closest_element = axis_index + 3;
				}
			}
			if (closest_dist > window->client_size.y * handle_grab_thickness) {
				closest_element = null_manipulator_part;
			}

			bool begin_drag = false;
			if (camera_forward_dot_handle_direction > 0) {
				if (closest_element != null_manipulator_part && mouse_down(0)) {
					begin_drag = true;
					state.dragging_part_index = closest_element;
					save_transform();
				}

			}

			if (state.dragging_part_index != null_manipulator_part) {
				v3f new_position;
				if (state.dragging_part_index < 3) {
					v2f closest_in_viewport = closest_point(line_begin_end(handle_viewport_position.xy, handle_viewport_axis_tips[state.dragging_part_index]), (v2f)mouse_position);

					v4f end = camera_to_world_matrix * V4f(map(closest_in_viewport, {}, (v2f)current_viewport.size, {-1,-1}, {1,1}), 1, 1);
					ray<v3f> cursor_ray = ray_origin_end(current_camera_entity->position, end.xyz / end.w);

					cursor_ray.direction = normalize(cursor_ray.direction);
					new_position = closest_point(line_begin_end(position, handle_world_axis_tips[state.dragging_part_index]), as_line(cursor_ray));
				} else {
					v3f plane_normal = normalize(handle_world_axis_tips[state.dragging_part_index - 3] - position);

					v4f end = camera_to_world_matrix * V4f(map((v2f)mouse_position, {}, (v2f)current_viewport.size, {-1,-1}, {1,1}), 1, 1);
					ray<v3f> cursor_ray = ray_origin_end(current_camera_entity->position, end.xyz / end.w);
	
					new_position = intersect(cursor_ray, plane_point_normal(position, plane_normal));
				}
				if (begin_drag) {
					state.original_click_position = new_position;
					state.drag_offset = manipulated_transform.position - new_position;
				} else {
					if (kind == Manipulate_position) {
						manipulated_transform.position = new_position + state.drag_offset * (dot(new_position - current_camera_entity->position, camera_forward) / dot(state.original_click_position - current_camera_entity->position, camera_forward));
					} else {
						m4 cancel_rotation = inverse((m4)rotation);
						v3f old_offset = (cancel_rotation * V4f(state.original_click_position - position, 1)).xyz;
						v3f new_offset = (cancel_rotation * V4f(new_position                  - position, 1)).xyz;
						v3f scale = new_offset / old_offset;
						if (state.dragging_part_index == 0) scale.y = scale.z = 1;
						if (state.dragging_part_index == 1) scale.x = scale.z = 1;
						if (state.dragging_part_index == 2) scale.x = scale.y = 1;
						if (state.dragging_part_index == 3) scale.x = 1;
						if (state.dragging_part_index == 4) scale.y = 1;
						if (state.dragging_part_index == 5) scale.z = 1;

						manipulated_transform.scale = state.original_transform.scale * scale;
						draw_request.scale = scale;
					}
				}
			}

			if (mouse_up(0)) {
				state.dragging_part_index = null_manipulator_part;
			}
			break;
		}
		case Manipulate_rotation: {
			v3f global_normals[] = {
				{1, 0, 0},
				{0, 1, 0},
				{0, 0, 1}
			};
			
			f32 closest_dist = max_value<f32>;
			//v3f closest_intersect_position;

			for (u32 part_index = 0; part_index < 3; ++part_index) {
				v4f end = camera_to_world_matrix * V4f(map((v2f)mouse_position, {}, (v2f)current_viewport.size, {-1,-1}, {1,1}), 1, 1);
				ray<v3f> cursor_ray = ray_origin_end(current_camera_entity->position, end.xyz / end.w);
				cursor_ray.direction = normalize(cursor_ray.direction);

				// TODO: this could be better
				u32 const point_count = 4 * 12;
				f32 dist = max_value<f32>;
				for (u32 point_index = 0; point_index < point_count; ++point_index) {
					v3f point = position + -rotation * quaternion_from_axis_angle(global_normals[part_index], point_index * (2 * pi / point_count)) * global_normals[part_index].zxy() * handle_size_scaled_by_distance;
					
					// Ignore the back of the circle
					if (dot(point - position, normalize(current_camera_entity->position - position)) < -0.0) 
						continue;

					if (key_down('F') && part_index == 0) {
						print("%\n", distance((v2f)mouse_position, world_to_viewport(point).xy));
					}
					dist = min(dist, distance((v2f)mouse_position, world_to_viewport(point).xy));
				}

				if (dist < closest_dist) {
					closest_dist = dist;
					closest_element = part_index;
					//closest_intersect_position = intersect_position;
				}
			}

			//debug_line(closest_intersect_position, closest_intersect_position + -rotation * global_normals[closest_element], {1, 0, 0});
			
			if (closest_dist > handle_grab_thickness * current_viewport.h) {
				closest_element = null_manipulator_part;
			}

			bool begin_drag = false;
			if (camera_forward_dot_handle_direction > 0) {
				if (closest_element != null_manipulator_part && mouse_down(0)) {
					begin_drag = true;
					state.dragging_part_index = closest_element;
					save_transform();
				}

			}
			if (state.dragging_part_index != null_manipulator_part) {
				v4f end = camera_to_world_matrix * V4f(map((v2f)mouse_position, {}, (v2f)current_viewport.size, {-1,-1}, {1,1}), 1, 1);
				ray<v3f> cursor_ray = ray_origin_end(current_camera_entity->position, end.xyz / end.w);
	
				if (begin_drag) {
					state.rotation_axis = -rotation * global_normals[state.dragging_part_index];
				}

				v3f intersect_position = intersect(cursor_ray, plane_point_normal(position, state.rotation_axis));
				
				v2f aspect_correction = {1, (f32)current_viewport.w / current_viewport.h};

				
				if (begin_drag) {
					v2f intersect_position_normalized = map((v2f)mouse_position, {}, (v2f)current_viewport.size, {-1,-1}, {1,1}) * aspect_correction;
					v3f tip = intersect_position + normalize(cross(intersect_position - position, state.rotation_axis));
					debug_line(1.0f, intersect_position, tip, {0, 1, 0});
					state.tangent = ray_origin_direction(
						intersect_position_normalized,
						normalize(world_to_camera(tip).xy - intersect_position_normalized * aspect_correction)
					);
					state.start_mouse_position = (v2f)mouse_position;
					state.accumulated_mouse_delta = {};
					print("%\n", state.tangent);
				}

				state.accumulated_mouse_delta += (v2f)window->mouse_delta * v2f{1,-1};


				f32 dist = 2 * pi * dot(
					map(
						state.start_mouse_position + state.accumulated_mouse_delta, 
						{}, 
						(v2f)current_viewport.size,
						{-1,-1},
						{1,1}
					) * aspect_correction - state.tangent.origin,
					state.tangent.direction
				);

				//print("%\n", dist);

				manipulated_transform.rotation = state.original_transform.rotation * quaternion_from_axis_angle(state.rotation_axis, dist);
			}
			if (mouse_up(0)) {
				state.dragging_part_index = null_manipulator_part;
			}
			break;
		}
	}
	
	if (state.dragging_part_index != null_manipulator_part) {
		if (mouse_down(1)) {
			manipulated_transform = state.original_transform;
			state.dragging_part_index = null_manipulator_part;
		}
	}

	draw_request.kind = kind,
	draw_request.highlighted_part_index = (state.dragging_part_index != null_manipulator_part) ? state.dragging_part_index : closest_element,
	draw_request.position = position,
	draw_request.rotation = rotation,
	draw_request.size = handle_size,

	manipulator_draw_requests.add(draw_request);

	return manipulated_transform;
}
