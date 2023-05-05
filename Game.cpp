#include <Game.h>
#include <box2d/b2_body.h>
#include <box2d/b2_math.h>
#include <box2d/b2_polygon_shape.h>
#include <box2d/box2d.h>
#include <fmt/core.h>
#include <algorithm>

Object::Object(Type type)
    : mType(type)
{}

static glm::vec2 calcSquareShape(int si, std::array<b2Vec2, 4>& verts)
{
  glm::vec2 center =
    Arena::CellSize *
    (glm::vec2 {0.5f, 0.5f} + glm::vec2 {float(si % Arena::NX), float(si / Arena::NX)});
  center.y                   = Arena::Height - center.y;
  glm::vec2                y = {0.f, 0.5f * Arena::SquareSize};
  glm::vec2                x = {0.5f * Arena::SquareSize, 0.f};
  std::array<glm::vec2, 4> temp;
  temp[0] = center - x - y;
  temp[1] = center + x - y;
  temp[2] = center + x + y;
  temp[3] = center - x + y;
  std::transform(temp.begin(), temp.end(), verts.begin(), [](glm::vec2 v) {
    return b2Vec2(v[0], v[1]);
  });
  return center;
}

Arena::Arena(b2World& world)
    : mWorld(world)
{
  auto squares = getSquares();
  std::fill(squares.begin(), squares.end(), Object(NOSQUARE));
  auto balls = getBalls();
  balls[0]   = Object(BALL);
  std::fill(balls.begin() + 1, balls.end(), Object(NOBALL));
  initGridBody();
  auto& grid = *mGrid;
  for (size_t i = 0; i < squares.size(); ++i) {
    auto&                 dst = squares[i];
    b2PolygonShape        shape;
    std::array<b2Vec2, 4> verts;
    dst.mPos = calcSquareShape(i, verts);
    shape.Set(verts.data(), int(verts.size()));
    dst.mFixture                        = grid.CreateFixture(&shape, 0.f);
    dst.mFixture->GetUserData().pointer = reinterpret_cast<uintptr_t>(&dst);
  }
  for (size_t i = 0; i < balls.size(); ++i) {
    auto&     dst = balls[i];
    b2BodyDef def;
    def.type   = b2_dynamicBody;
    def.bullet = true;
    def.position.Set(0.f, 0.f);
    def.userData.pointer = reinterpret_cast<uintptr_t>(&dst);
    dst.mBody            = mWorld.CreateBody(&def);
    b2CircleShape shape;
    shape.m_p.Set(0.f, 0.f);
    shape.m_radius                      = BallRadius;
    dst.mFixture                        = dst.mBody->CreateFixture(&shape, 0.f);
    dst.mFixture->GetUserData().pointer = reinterpret_cast<uintptr_t>(&dst);
  }
}

int Arena::advance(uint32_t seed)
{
  auto squares = getSquares();
  auto begin   = squares.begin();
  auto end     = squares.begin() + NX;
  if (std::any_of(begin, end, [](const Object& sq) { return sq.mType == SQUARE; })) {
    return 1;
  }
  std::srand(seed);
  while (begin != end) {
    auto& sq = *(begin++);
    // TODO: Weighted sampling.
    sq.mType = Type(std::rand() % 3);
    // TODO: Properly assign mData.
    if (sq.mType == SQUARE) {
      sq.mData = 1;
    }
  }
  std::rotate(squares.begin(), end, squares.end());
  return 0;
}

void Arena::initGridBody()
{
  b2BodyDef def;
  def.type = b2_staticBody;
  def.position.Set(0.f, 0.f);
  mGrid = mWorld.CreateBody(&def);
}

std::span<Object> Arena::getSquares()
{
  return std::span<Object>(mObjects.begin(), NGrid);
}

std::span<Object> Arena::getBalls()
{
  return std::span<Object>(mObjects.begin() + NGrid, NMaxBalls);
}

std::string vertShader()
{
  static constexpr char sTemplate[] = R"(

#version 330 core

layout(location = 0) in vec2 position;
// layout(location = 1) in vec3 colOrTex;

// out vec3 ColorOrTex;

void main()
{{
  position.x = 2. * (position.x / {width:.8f}) - 1.;
  position.y = 2. * (position.y / {height:.8f}) - 1.;
  gl_Position = vec4(position, 0.0, 1.0);
  // ColorOrTex   = colOrTex;
}}

)";
  return fmt::format(
    sTemplate, fmt::arg("width", Arena::Width), fmt::arg("height", Arena::Height));
}
