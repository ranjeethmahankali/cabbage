#pragma once

#include <stdint.h>
#include <array>
#include <cstddef>

class b2Body;
class b2World;
class b2Fixture;

struct Square
{
  int        mData;
  b2Fixture* mFixture;
};

class Arena
{
private:
  static constexpr size_t   NX         = 7;
  static constexpr size_t   NY         = 8;
  static constexpr size_t   NGrid      = NX * NY;
  static constexpr int      EMPTY      = -1;
  static constexpr int      MULTIPLIER = -2;
  std::array<Square, NGrid> mSquares;
  b2Body*                   mGrid = nullptr;
  b2World&                  mWorld;

public:
  explicit Arena(b2World& world);

  void initGridBody();
};
