#pragma once
#include "entity.h"

enum SelectionKind {
	Selection_none,
	Selection_entity,
	Selection_texture,
};

struct {
	SelectionKind kind;
	union {
		Entity *entity;
		Texture *texture;
	};
	
	void unset() {
		kind = Selection_none;
	}

	void set(Entity *entity) {
		kind = Selection_entity;
		this->entity = entity;
	}

	void set(Texture *texture) {
		kind = Selection_texture;
		this->texture = texture;
	}

} selection;
