#pragma once

#include <box2d/box2d.h>
#include <stdint.h>
#include <array>
#include <cstddef>
#include <cstdint>
#include <glm/glm.hpp>
#include <span>
#include <string>

class b2Body;
class b2World;
class b2Fixture;

enum GroupIndex : int16_t
{
  G_NOSQUARE  = 1,
  G_SQUARE    = 2,
  G_BALL_SPWN = 3,
  G_NOBALL    = 4,
  G_BALL      = 5,
  G_WALL      = 6,
};

enum Type : uint16_t
{
  T_NOSQUARE  = 1,
  T_SQUARE    = 2,
  T_BALL_SPWN = 4,
  T_NOBALL    = 8,
  T_BALL      = 16,
  T_WALL      = 32,
};

constexpr inline Type operator|(Type a, Type b)
{
  return Type(uint16_t(a) | uint16_t(b));
}

enum CollisionMask : uint16_t
{
  M_NOSQUARE  = 0,
  M_SQUARE    = T_BALL,
  M_BALL_SPWN = T_BALL,
  M_NOBALL    = 0,
  M_BALL      = T_SQUARE | T_BALL_SPWN | T_WALL,
  M_WALL      = T_BALL,
};

struct Object
{
  b2Fixture* mFixture = nullptr;
  b2Body*    mBody    = nullptr;
  glm::vec2  mPos     = {0.f, 0.f};
  int        mIndex   = -1;
  union
  {
    struct
    {
      int  mData;
      Type mType;
    };
    uint64_t mAttributes = 0;
  };

  Object();
  explicit Object(Type type);
};

class Arena
{
public:
  static constexpr uint32_t NX               = 7;
  static constexpr uint32_t NY               = 8;
  static constexpr uint32_t NGrid            = NX * NY;
  static constexpr uint32_t NMaxBalls        = 2048;
  static constexpr float    CellSize         = 100.f;
  static constexpr float    SquareSize       = 85.f;
  static constexpr float    Height           = float(NY) * CellSize;
  static constexpr float    Width            = float(NX) * CellSize;
  static constexpr float    BallRadius       = CellSize * 0.1f;
  static constexpr float    BallSpawnRelSize = 0.55;
  static constexpr float    BallSpawnSize    = BallSpawnRelSize * SquareSize;
  static constexpr float    BallVelocity     = 500.f;

  explicit Arena(b2World& world);
  ~Arena();
  void              draw() const;
  int               advance(uint32_t seed);
  void              shoot(float angle);
  void              step();
  std::span<Object> getSquares();
  std::span<Object> getBalls();

private:
  std::array<Object, NGrid + NMaxBalls> mObjects;
  b2Body*                               mGrid = nullptr;
  b2World&                              mWorld;
  uint32_t                              mCounter  = 1;
  uint32_t                              mNumBalls = 0;
  uint32_t                              mVao      = 0;
  uint32_t                              mVbo      = 0;
  float                                 mBallX    = 3.5f * CellSize;

private:
  void              initGridBody();
  void              initGL();
  void              freeGL();
  void              bindGL() const;
  void              copyGLData() const;
  void              unbindGL() const;
  std::span<Object> getRow(uint32_t i);
  void              addBall();
};

class ContactListener : public b2ContactListener
{
  Object* getA(b2Contact* contact) const;
  Object* getB(b2Contact* contact) const;
  void    BeginContact(b2Contact* contact);
};
