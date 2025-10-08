#include "planetmap.hpp"

#include <btBulletCollisionCommon.h>

#include "BulletCollision/CollisionDispatch/btCollisionObject.h"
#include "world.hpp"

namespace rdm {

QuadNode::QuadNode() { leaf = true; }

std::vector<QuadNode*> QuadNode::getChildren() {
  if (leaf) throw std::runtime_error("getChildren on leaf");
  std::vector<QuadNode*> list;
  for (int i = 0; i < 4; i++) list.push_back(children[i]);
  return list;
}

void QuadNode::split() {
  leaf = false;
  for (int i = 0; i < 4; i++) children[i] = new QuadNode();
}

void QuadNode::merge() {
  if (leaf) return;
  for (int i = 0; i < 4; i++) delete children[i];
  leaf = true;
}

void QuadSphere::node(QuadNode* parent, int level, QuadSphere::Face face,
                      QuadNode::Quarter quarter) {
  if (parent->isLeaf()) {
  } else {
    int i = 0;
    for (auto child : parent->getChildren()) {
      node(child, level + 1, face, (QuadNode::Quarter)i);
      i++;
    }
  }
}

void QuadSphere::build(gfx::Engine* engine) {
  for (int i = 0; i < _Max; i++) {
    QuadNode nd = faces[i];
    node(&nd, 0, (QuadSphere::Face)i, QuadNode::Unknown);
  }
}

PlanetMap::PlanetMap(Game* game, World* world, gfx::Engine* engine) {
  btCollisionShape* shape = new btStaticPlaneShape(btVector3(0, 0, 1.f), 1.f);
  btCollisionObject* plane = new btCollisionObject();
  plane->setCollisionShape(shape);
  world->getPhysicsWorld()->getWorld()->addCollisionObject(plane);

  this->game = game;
  this->world = world;
  this->engine = engine;

  if (engine) {
    engine->renderStepped.listen(std::bind(&PlanetMap::render, this));
  }
}

void PlanetMap::render() { gfx::Camera& camera = engine->getCamera(); }
};  // namespace rdm
