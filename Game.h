#pragma once

#include <stdint.h>
#include <array>
#include <cstddef>
#include <glm/glm.hpp>
#include <span>
#include <string>

class b2Body;
class b2World;
class b2Fixture;

enum Type : int
{
  NOSQUARE  = 0,
  SQUARE    = 1,
  BALL_SPWN = 2,
  NOBALL    = 3,
  BALL      = 4,
};

struct Object
{
  b2Fixture* mFixture = nullptr;
  b2Body*    mBody    = nullptr;
  glm::vec2  mPos     = {0.f, 0.f};
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
  static constexpr uint32_t NX         = 7;
  static constexpr uint32_t NY         = 8;
  static constexpr uint32_t NGrid      = NX * NY;
  static constexpr uint32_t NMaxBalls  = 2048;
  static constexpr float    CellSize   = 100.f;
  static constexpr float    SquareSize = 85.f;
  static constexpr float    Height     = float(NY) * CellSize;
  static constexpr float    Width      = float(NX) * CellSize;
  static constexpr float    BallRadius = CellSize * 0.1f;

  explicit Arena(b2World& world);
  void draw() const;
  int  advance(uint32_t seed);
  ~Arena();

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
  void              unbindGL() const;
  std::span<Object> getSquares();
  std::span<Object> getRow(uint32_t i);
  std::span<Object> getBalls();
  void              addBall();
};
