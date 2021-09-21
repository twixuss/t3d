#pragma once
#include <t3d/entity.h>

enum SelectionKind {
	Selection_none,
	Selection_entity,
	Selection_texture,
};

struct {
	SelectionKind kind;
	union {
		Entity *entity;
		Texture2D *texture;
	};

	void unset() {
		kind = Selection_none;
	}

	void set(Entity *entity) {
		kind = Selection_entity;
		this->entity = entity;
	}

	void set(Texture2D *texture) {
		kind = Selection_texture;
		this->texture = texture;
	}

} selection;
