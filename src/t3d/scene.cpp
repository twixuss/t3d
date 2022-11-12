#include "scene.h"

Entity &Scene::create_entity() {
	auto entity = entities.add();
	entity.pointer->name = format(u8"Entity {}", entity.index);
	entity.pointer->scene = this;
	return *entity.pointer;
}
