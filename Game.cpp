#include <Game.h>
#include <box2d/b2_polygon_shape.h>
#include <box2d/box2d.h>

static constexpr float SQ_SIZE = 0.45f;

Arena::Arena(b2World& world)
    : mWorld(world)
{
  mSquares.fill({EMPTY, nullptr});
  initGridBody();
  auto& grid = *mGrid;
  for (size_t i = 0; i < mSquares.size(); ++i) {
    auto&          dst = mSquares[i];
    b2PolygonShape square;
    square.SetAsBox(SQ_SIZE, SQ_SIZE);
    dst.mFixture = grid.CreateFixture(&square, 0.f);
  }
}

void Arena::initGridBody()
{
  b2BodyDef def;
  def.type = b2_staticBody;
  def.position.Set(0.f, 0.f);
  mGrid = mWorld.CreateBody(&def);
}
