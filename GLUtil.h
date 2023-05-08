#pragma once

#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <spdlog/spdlog.h>

#ifdef WIN32
#define DEBUG_BREAK __debugbreak()
#else
#define DEBUG_BREAK __builtin_trap()
#endif

// #define GAL_GL_LOG
#if defined GAL_GL_LOG
#define GL_CALL(fncall)                                \
  {                                                    \
    view::clear_errors();                              \
    fncall;                                            \
    if (view::log_errors(#fncall, __FILE__, __LINE__)) \
      DEBUG_BREAK;                                     \
    view::logger().debug("{}: {}", #fncall, __FILE__); \
  }
#elif defined NDEBUG
#define GL_CALL(fncall) fncall
#else
#define GL_CALL(fncall)                                \
  {                                                    \
    view::clear_errors();                              \
    fncall;                                            \
    if (view::log_errors(#fncall, __FILE__, __LINE__)) \
      DEBUG_BREAK;                                     \
  }
#endif  // DEBUG

using uint = GLuint;

namespace view {

spdlog::logger& logger();
bool            log_errors(const char* function, const char* file, uint line);
void            clear_errors();

class Shader
{
public:
  Shader();
  ~Shader();
  void use() const;
  void free();
  Shader(const Shader&) = delete;
  Shader(Shader&&)      = delete;

private:
  uint32_t mId = 0;
};

}  // namespace view
