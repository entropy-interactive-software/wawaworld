#pragma once
#include <gfx/engine.hpp>
#include <glm/glm.hpp>
namespace rdm {
class QuadNode {
  QuadNode* children[4];
  bool leaf;

 public:
  QuadNode();

  void split();
  void merge();

  std::vector<QuadNode*> getChildren();

  enum Quarter {
    LeftUp,
    LeftDown,
    RightUp,
    RightDown,
    Unknown,
  };

  bool isLeaf() { return leaf; }
};

class QuadSphere {
  enum Face { XPos, XNeg, YPos, YNeg, ZPos, ZNeg, _Max };

  QuadNode faces[_Max];

  void node(QuadNode* node, int level, QuadSphere::Face face,
            QuadNode::Quarter quarter);

 public:
  void build(gfx::Engine* engine);
};

class PlanetMap {
  gfx::Engine* engine;
  Game* game;
  World* world;

  QuadSphere sphere;

 public:
  PlanetMap(Game* game, World* world, gfx::Engine* engine);

  void render();
};
};  // namespace rdm
