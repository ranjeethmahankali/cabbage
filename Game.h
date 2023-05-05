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

enum Type : uint8_t
{
  NOSQUARE       = 0,
  SQUARE         = 1,
  BALL_SPWN      = 2,
  USED_BALL_SPWN = 3,
  NOBALL         = 4,
  BALL           = 5,
};

struct Object
{
  b2Fixture* mFixture = nullptr;
  b2Body*    mBody    = nullptr;
  glm::vec2  mPos     = {0.f, 0.f};
  int        mData    = 0;
  Type       mType    = NOSQUARE;

  Object() = default;
  explicit Object(Type type);
};

class Arena
{
public:
  static constexpr size_t NX         = 7;
  static constexpr size_t NY         = 8;
  static constexpr size_t NGrid      = NX * NY;
  static constexpr size_t NMaxBalls  = 2048;
  static constexpr float  CellSize   = 100.f;
  static constexpr float  SquareSize = 95.f;
  static constexpr float  Height     = float(NY) * CellSize;
  static constexpr float  Width      = float(NX) * CellSize;
  static constexpr float  BallRadius = CellSize * 0.1f;

  explicit Arena(b2World& world);
  int advance(uint32_t seed);

private:
  std::array<Object, NGrid + NMaxBalls> mObjects;
  b2Body*                               mGrid = nullptr;
  b2World&                              mWorld;

private:
  void              initGridBody();
  std::span<Object> getSquares();
  std::span<Object> getBalls();
};

std::string vertShader();
