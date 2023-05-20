#include <GLUtil.h>
#include <Game.h>
#include <box2d/b2_body.h>
#include <box2d/b2_math.h>
#include <box2d/b2_polygon_shape.h>
#include <fmt/core.h>
#include <algorithm>
#include <cstddef>
#include <cstdint>

namespace scale {
static constexpr float Factor    = 100.f;
static constexpr float InvFactor = 1.f / Factor;

inline b2Vec2 tob2(float x, float y)
{
  return b2Vec2(x * InvFactor, y * InvFactor);
}

inline float tob2(float x)
{
  return x * InvFactor;
}

inline float fromb2(float x)
{
  return x * Factor;
}

inline b2Vec2 tob2(glm::vec2 v)
{
  return tob2(v.x, v.y);
}

inline glm::vec2 fromb2(b2Vec2 v)
{
  return glm::vec2(v.x * Factor, v.y * Factor);
}

}  // namespace scale

static void setNoSquare(Object& sq)
{
  sq.mType = T_NOSQUARE;
  b2Filter filter;
  filter.categoryBits = T_NOSQUARE;
  filter.maskBits     = M_NOSQUARE;
  filter.groupIndex   = G_NOSQUARE;
  sq.mFixture->SetFilterData(filter);
}

static void setSquare(Object& sq)
{
  sq.mType = T_SQUARE;
  b2Filter filter;
  filter.categoryBits = T_SQUARE;
  filter.maskBits     = M_SQUARE;
  filter.groupIndex   = G_SQUARE;
  sq.mFixture->SetFilterData(filter);
}

static void setNoBall(Object& ball)
{
  ball.mType = T_NOBALL;
  b2Filter filter;
  filter.categoryBits = T_NOBALL;
  filter.maskBits     = M_NOBALL;
  filter.groupIndex   = G_NOBALL;
  ball.mFixture->SetFilterData(filter);
  ball.mBody->SetEnabled(false);
}

static void setBall(Object& ball)
{
  ball.mType = T_BALL;
  b2Filter filter;
  filter.categoryBits = T_BALL;
  filter.maskBits     = M_BALL;
  filter.groupIndex   = G_BALL;
  ball.mFixture->SetFilterData(filter);
  ball.mBody->SetEnabled(true);
}

Object* ContactListener::getA(b2Contact* contact) const
{
  return reinterpret_cast<Object*>(contact->GetFixtureA()->GetUserData().pointer);
}

Object* ContactListener::getB(b2Contact* contact) const
{
  return reinterpret_cast<Object*>(contact->GetFixtureB()->GetUserData().pointer);
}

void ContactListener::BeginContact(b2Contact* contact)
{
  Object* sq   = getA(contact);
  Object* ball = getB(contact);
  if (sq && ball) {
    if (sq->mType == T_BALL) {
      std::swap(sq, ball);
    }
    if (sq->mType == T_SQUARE) {
      if (--(sq->mData) == 0) {
        setNoSquare(*sq);
      }
    }
  }
}

Object::Object(Type type)
    : mType(type)
{}

static glm::vec2 calcSquareShape(int si, float size, std::array<b2Vec2, 4>& verts)
{
  glm::vec2 center =
    Arena::CellSize *
    (glm::vec2 {0.5f, 0.5f} + glm::vec2 {float(si % Arena::NX), float(si / Arena::NX)});
  glm::vec2                y = {0.f, 0.5f * size};
  glm::vec2                x = {0.5f * size, 0.f};
  std::array<glm::vec2, 4> temp;
  temp[0] = center - x - y;
  temp[1] = center + x - y;
  temp[2] = center + x + y;
  temp[3] = center - x + y;
  std::transform(
    temp.begin(), temp.end(), verts.begin(), [](glm::vec2 v) { return scale::tob2(v); });
  return center;
}

Object::Object() {}

Arena::Arena(b2World& world)
    : mWorld(world)
{
  auto squares = getSquares();
  auto balls   = getBalls();
  std::fill(balls.begin(), balls.end(), Object(T_NOBALL));
  initGridBody();
  auto& grid = *mGrid;
  for (uint32_t i = 0; i < squares.size(); ++i) {
    auto& dst  = squares[i];
    dst.mIndex = int(i);
    b2PolygonShape        shape;
    std::array<b2Vec2, 4> verts;
    dst.mPos = calcSquareShape(i, Arena::SquareSize, verts);
    shape.Set(verts.data(), int(verts.size()));
    dst.mFixture                        = grid.CreateFixture(&shape, 1.f);
    dst.mFixture->GetUserData().pointer = reinterpret_cast<uintptr_t>(&dst);
    dst.mFixture->SetFriction(0.f);
    dst.mFixture->SetRestitution(1.f);
    setNoSquare(dst);
  }
  for (uint32_t i = 0; i < balls.size(); ++i) {
    auto& dst  = balls[i];
    dst.mIndex = int(i);
    b2BodyDef def;
    def.type   = b2_dynamicBody;
    def.bullet = true;
    def.position.Set(0.f, 0.f);
    def.userData.pointer = reinterpret_cast<uintptr_t>(&dst);
    dst.mBody            = mWorld.CreateBody(&def);
    def.enabled          = false;
    b2CircleShape shape;
    shape.m_p.Set(0.f, 0.f);
    shape.m_radius = scale::tob2(BallRadius);
    dst.mFixture   = dst.mBody->CreateFixture(&shape, 0.1f);
    dst.mFixture->SetRestitution(1.f);
    dst.mFixture->SetFriction(0.f);
    dst.mFixture->GetUserData().pointer = reinterpret_cast<uintptr_t>(&dst);
    setNoBall(dst);
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
  GL_CALL(glVertexAttribIPointer(2, 1, GL_UNSIGNED_SHORT, stride, typeOffset));
  GL_CALL(glEnableVertexAttribArray(2));
}

void Arena::initGL()
{
  // Create and bind the vertex array.
  GL_CALL(glGenVertexArrays(1, &mVao));
  GL_CALL(glGenBuffers(1, &mVbo));
  bindGL();
  // Copy data.
  copyGLData();
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

static void updateSquareAttributes(Object& sq)
{
  if (sq.mType == T_BALL_SPWN) {
    std::array<b2Vec2, 4> verts;
    sq.mPos     = calcSquareShape(sq.mIndex, Arena::BallSpawnSize, verts);
    auto& shape = *(dynamic_cast<b2PolygonShape*>(sq.mFixture->GetShape()));
    shape.Set(verts.data(), int(verts.size()));
    sq.mFixture->SetSensor(true);
  }
  if (sq.mType == T_SQUARE) {
    std::array<b2Vec2, 4> verts;
    sq.mPos     = calcSquareShape(sq.mIndex, Arena::SquareSize, verts);
    auto& shape = *(dynamic_cast<b2PolygonShape*>(sq.mFixture->GetShape()));
    shape.Set(verts.data(), int(verts.size()));
    setSquare(sq);
  }
}

int Arena::advance(uint32_t seed)
{
  auto squares = getSquares();
  if (std::any_of(squares.begin(), squares.begin() + NX, [](const Object& sq) {
        return sq.mType == T_SQUARE;
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
        auto& left  = squares[fi++];
        auto& right = squares[ni++];
        std::swap(left.mAttributes, right.mAttributes);
        updateSquareAttributes(left);
        updateSquareAttributes(right);
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
    sq.mType = Type(1 << (std::rand() % 3));
    updateSquareAttributes(sq);
    // TODO: Properly assign mData with some randomness.
    if (sq.mType == T_SQUARE) {
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

void Arena::shoot(float angle)
{
  auto balls = getBalls();
  for (uint32_t i = 0; i < mNumBalls; ++i) {
    balls[i].mBody->SetLinearVelocity(
      scale::tob2(std::cos(angle) * BallVelocity, std::sin(angle) * BallVelocity));
  }
}

void Arena::step()
{
  auto balls = getBalls();
  for (uint32_t i = 0; i < mNumBalls; ++i) {
    auto& ball = balls[i];
    ball.mPos  = scale::fromb2(ball.mBody->GetPosition());
  }
  bindGL();
  copyGLData();
  unbindGL();
}

void Arena::bindGL() const
{
  GL_CALL(glBindVertexArray(mVao));
  GL_CALL(glBindBuffer(GL_ARRAY_BUFFER, mVbo));
}

void Arena::copyGLData() const
{
  GL_CALL(glBufferData(
    GL_ARRAY_BUFFER, sizeof(Object) * mObjects.size(), mObjects.data(), GL_DYNAMIC_DRAW));
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
  mGrid->SetAwake(true);
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
  auto  balls = getBalls();
  auto& last  = balls[mNumBalls++];
  setBall(last);
  for (size_t i = 0; i < mNumBalls; ++i) {
    auto& ball = balls[i];
    ball.mPos  = glm::vec2(mBallX, BallRadius);
    ball.mBody->SetTransform(scale::tob2(ball.mPos.x, ball.mPos.y), 0.f);
  }
}
