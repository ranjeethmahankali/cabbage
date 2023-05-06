#include <GLUtil.h>
#include <Game.h>
#include <box2d/b2_body.h>
#include <box2d/b2_math.h>
#include <box2d/b2_polygon_shape.h>
#include <box2d/box2d.h>
#include <fmt/core.h>
#include <algorithm>
#include <cstdint>

Object::Object(Type type)
    : mType(type)
{}

static glm::vec2 calcSquareShape(int si, std::array<b2Vec2, 4>& verts)
{
  glm::vec2 center =
    Arena::CellSize *
    (glm::vec2 {0.5f, 0.5f} + glm::vec2 {float(si % Arena::NX), float(si / Arena::NX)});
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

Object::Object() {}

Arena::Arena(b2World& world)
    : mWorld(world)
{
  auto squares = getSquares();
  std::fill(squares.begin(), squares.end(), Object(NOSQUARE));
  auto balls = getBalls();
  std::fill(balls.begin(), balls.end(), Object(NOBALL));
  initGridBody();
  auto& grid = *mGrid;
  for (uint32_t i = 0; i < squares.size(); ++i) {
    auto&                 dst = squares[i];
    b2PolygonShape        shape;
    std::array<b2Vec2, 4> verts;
    dst.mPos = calcSquareShape(i, verts);
    shape.Set(verts.data(), int(verts.size()));
    dst.mFixture                        = grid.CreateFixture(&shape, 0.f);
    dst.mFixture->GetUserData().pointer = reinterpret_cast<uintptr_t>(&dst);
  }
  for (uint32_t i = 0; i < balls.size(); ++i) {
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
  addBall();
  // Prep for rendering.
  initGL();
}

void Arena::draw() const
{
  bindGL();
  GL_CALL(glDrawArrays(GL_POINTS, 0, mObjects.size()));
}

static void initAttributes()
{
  static constexpr size_t stride     = sizeof(Object);
  static const void*      posOffset  = (void*)(&(((Object*)nullptr)->mPos));
  static const void*      dataOffset = (void*)(&(((Object*)nullptr)->mData));
  static const void*      typeOffset = (void*)(&(((Object*)nullptr)->mType));
  GL_CALL(glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, stride, posOffset));
  GL_CALL(glEnableVertexAttribArray(0));
  GL_CALL(glVertexAttribIPointer(1, 1, GL_INT, stride, dataOffset));
  GL_CALL(glEnableVertexAttribArray(1));
  GL_CALL(glVertexAttribIPointer(2, 1, GL_INT, stride, typeOffset));
  GL_CALL(glEnableVertexAttribArray(2));
}

void Arena::initGL()
{
  // Create and bind the vertex array.
  GL_CALL(glGenVertexArrays(1, &mVao));
  GL_CALL(glGenBuffers(1, &mVbo));
  bindGL();
  // Copy data.
  GL_CALL(glBufferData(
    GL_ARRAY_BUFFER, sizeof(Object) * mObjects.size(), mObjects.data(), GL_DYNAMIC_DRAW));
  // Initialize the attributes.
  initAttributes();
  unbindGL();
}

void Arena::freeGL()
{
  if (mVao) {
    GL_CALL(glDeleteVertexArrays(1, &mVao));
    mVao = 0;
  }
  if (mVbo) {
    GL_CALL(glDeleteBuffers(1, &mVbo));
    mVbo = 0;
  }
}

Arena::~Arena()
{
  freeGL();
}

int Arena::advance(uint32_t seed)
{
  auto squares = getSquares();
  if (std::any_of(squares.begin(), squares.begin() + NX, [](const Object& sq) {
        return sq.mType == SQUARE;
      })) {
    return 1;
  }
  {
    // Equivalent to doing an std::rotate on the rows, but we're only swapping the
    // attributes.
    uint32_t first  = 0;
    uint32_t middle = 1;
    uint32_t last   = NY;
    uint32_t next   = middle;
    while (first != next) {
      // Swap rows.
      uint32_t fi = (first++) * NX;
      uint32_t ni = (next++) * NX;
      for (uint32_t i = 0; i < NX; ++i) {
        std::swap(squares[fi++].mAttributes, squares[ni++].mAttributes);
      }
      if (next == last) {
        next = middle;
      }
      else if (first == middle) {
        middle = next;
      }
    }
  }
  std::srand(seed);
  for (auto& sq : getRow(NY - 1)) {
    // TODO: Weighted sampling.
    sq.mType = Type(std::rand() % 3);
    // TODO: Properly assign mData.
    if (sq.mType == SQUARE) {
      sq.mData = mCounter;
    }
  }
  ++mCounter;
  // Update GL buffers accordingly.
  bindGL();
  GL_CALL(glBufferSubData(
    GL_ARRAY_BUFFER, 0, sizeof(Object) * mObjects.size(), mObjects.data()));
  unbindGL();
  return 0;
}

void Arena::bindGL() const
{
  GL_CALL(glBindVertexArray(mVao));
  GL_CALL(glBindBuffer(GL_ARRAY_BUFFER, mVbo));
}

void Arena::unbindGL() const
{
  GL_CALL(glBindBuffer(GL_ARRAY_BUFFER, 0));
  GL_CALL(glBindVertexArray(0));
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

std::span<Object> Arena::getRow(uint32_t i)
{
  return std::span<Object>(mObjects.begin() + i * NX, NX);
}

std::span<Object> Arena::getBalls()
{
  return std::span<Object>(mObjects.begin() + NGrid, NMaxBalls);
}

void Arena::addBall()
{
  auto& ball = getBalls()[mNumBalls++];
  ball.mPos  = glm::vec2(mBallX, 0.f);
  ball.mType = BALL;
}
