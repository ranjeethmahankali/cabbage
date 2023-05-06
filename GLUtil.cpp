#include <GLUtil.h>
#include <Game.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

namespace view {

spdlog::logger& logger()
{
  static auto sLogger = spdlog::stdout_color_mt("viewer");
  return *sLogger;
}

bool log_errors(const char* function, const char* file, uint line)
{
  static bool found_error = false;
  while (GLenum error = glGetError()) {
    logger().error("OpenGL Error 0x{:x} in {} at {}:{}", error, function, file, line);
    found_error = true;
  }
  if (found_error) {
    found_error = false;
    return true;
  }
  else {
    return false;
  }
}

void clear_errors()
{
  // Just loop over and consume all pending errors.
  GLenum error = glGetError();
  while (error) {
    error = glGetError();
  }
}

static std::string vertShaderSrc()
{
  static constexpr char sTemplate[] = R"(

#version 330 core

layout(location = 0) in vec2 position;
layout(location = 1) in int data;
layout(location = 2) in int type;

out int Data;
out int Type;
out vec2 ObjPos;

void main()
{{
  Data = data;
  Type = type;
  ObjPos = position;
  vec2 pos = position;
  pos.x = 2. * (position.x / {width:.8f}) - 1.;
  pos.y = 2. * (position.y / {height:.8f}) - 1.;
  gl_Position = vec4(pos.xy, 0., 1.);
}}

)";
  return fmt::format(
    sTemplate, fmt::arg("width", Arena::Width), fmt::arg("height", Arena::Height));
}

static std::string geoShader()
{
  static constexpr char sTemplate[] = R"(
#version 330 core
layout (points) in;
layout (triangle_strip, max_vertices = 4) out;

const int NOSQUARE       = {nosq};
const int SQUARE         = {sq};
const int BALL_SPWN      = {bspwn};
const int USED_BALL_SPWN = {ubspwn};
const int NOBALL         = {nbl};
const int BALL           = {bl};

const vec2 x = vec2({xx:.8f}, 0.);
const vec2 y = vec2(0., {yy:.8f});

in int Data[];
in int Type[];
in vec2 ObjPos[];
flat out int FData;
flat out int FType;
flat out vec2 FObjPos;

void main() {{
  FData = Data[0];
  FType = Type[0];
  FObjPos = ObjPos[0];
  if (Type[0] == SQUARE) {{
    vec2 pos = gl_in[0].gl_Position.xy;
    gl_Position = vec4(pos - x - y, 0., 1.); 
    EmitVertex();
    gl_Position = vec4(pos + x - y, 0., 1.); 
    EmitVertex();
    gl_Position = vec4(pos - x + y, 0., 1.); 
    EmitVertex();
    gl_Position = vec4(pos + x + y, 0., 1.); 
    EmitVertex();
    EndPrimitive();
  }}
}}

)";
  return fmt::format(sTemplate,
                     fmt::arg("nosq", int(NOSQUARE)),
                     fmt::arg("sq", int(SQUARE)),
                     fmt::arg("bspwn", int(BALL_SPWN)),
                     fmt::arg("ubspwn", int(USED_BALL_SPWN)),
                     fmt::arg("nbl", int(NOBALL)),
                     fmt::arg("bl", int(BALL)),
                     fmt::arg("xx", Arena::SquareSize / Arena::Width),
                     fmt::arg("yy", Arena::SquareSize / Arena::Height));
}

static std::string fragShaderSrc()
{
  static constexpr char sTemplate[] = R"(
#version 330 core

out vec4 FragColor;

flat in int FData;
flat in int FType;
flat in vec2 FObjPos;

const vec3 Colors[7] = vec3[](
  vec3(1, 1, 0),
  vec3(0, 1, 0),
  vec3(0, 0, 1),
  vec3(0.29, 0, 0.51),
  vec3(0.93, 0.51, 0.93),
  vec3(1, 0, 0),
  vec3(1, 0.5, 0)
);

const float BallRadius = {brad:.8f};
const int MaxData = 50;

const int NOSQUARE       = {nosq};
const int SQUARE         = {sq};
const int BALL_SPWN      = {bspwn};
const int USED_BALL_SPWN = {ubspwn};
const int NOBALL         = {nbl};
const int BALL           = {bl};

void main()
{{
  if (FType == SQUARE) {{
    float r = 7. * min(1., float(FData - 1) / float(MaxData - 1));
    int rt = int(ceil(r));
    int lt = int(floor(r));
    r = fract(r);
    FragColor = vec4(Colors[lt] * (1. - r) + Colors[rt] * r, 1.);
  }}
  else if (FType == BALL) {{
    float dist = distance(gl_FragCoord.xy, FObjPos);
    if (dist < BallRadius) {{
      FragColor = vec4(1., 1., 1., 1.);
    }}
  }}
}}

)";
  return fmt::format(sTemplate,
                     fmt::arg("nosq", int(NOSQUARE)),
                     fmt::arg("sq", int(SQUARE)),
                     fmt::arg("bspwn", int(BALL_SPWN)),
                     fmt::arg("ubspwn", int(USED_BALL_SPWN)),
                     fmt::arg("nbl", int(NOBALL)),
                     fmt::arg("bl", int(BALL)),
                     fmt::arg("brad", float(2.f * Arena::BallRadius)));
}

static void checkShaderCompilation(uint32_t id, uint32_t type)
{
  int result;
  glGetShaderiv(id, GL_COMPILE_STATUS, &result);
  if (result == GL_FALSE) {
    int length;
    glGetShaderiv(id, GL_INFO_LOG_LENGTH, &length);
    std::string shaderType = "unknown";
    switch (type) {
    case GL_VERTEX_SHADER:
      shaderType = "vertex";
      break;
    case GL_FRAGMENT_SHADER:
      shaderType = "fragment";
      break;
    case GL_COMPUTE_SHADER:
      shaderType = "compute";
      break;
    case GL_GEOMETRY_SHADER:
      shaderType = "geometry";
      break;
    default:
      break;
    }
    std::string message(length + 1, '\0');
    glGetShaderInfoLog(id, length, &length, message.data());
    logger().error("Failed to compile {} shader:\n{}", shaderType, message);
    GL_CALL(glDeleteShader(id));
  }
}

static void checkShaderLinking(uint32_t progId)
{
  int success;
  glGetProgramiv(progId, GL_LINK_STATUS, &success);
  if (!success) {
    char infoLog[1024];
    glGetProgramInfoLog(progId, 1024, NULL, infoLog);
    logger().error("Error linking shader program:\n{}", infoLog);
  }
}

Shader::Shader()
{
  uint32_t vsId = 0;
  {  // Compile vertex shader.
    std::string src  = vertShaderSrc();
    vsId             = glCreateShader(GL_VERTEX_SHADER);
    const char* cstr = src.c_str();
    GL_CALL(glShaderSource(vsId, 1, &cstr, nullptr));
    GL_CALL(glCompileShader(vsId));
    checkShaderCompilation(vsId, GL_VERTEX_SHADER);
  }
  uint32_t gsId = 0;
  {  // Compile geometry shader.
    std::string src  = geoShader();
    gsId             = glCreateShader(GL_GEOMETRY_SHADER);
    const char* cstr = src.c_str();
    GL_CALL(glShaderSource(gsId, 1, &cstr, nullptr));
    GL_CALL(glCompileShader(gsId));
    checkShaderCompilation(gsId, GL_GEOMETRY_SHADER);
  }
  uint32_t fsId = 0;
  {  // Compile fragment shader.
    std::string src  = fragShaderSrc();
    fsId             = glCreateShader(GL_FRAGMENT_SHADER);
    const char* cstr = src.c_str();
    GL_CALL(glShaderSource(fsId, 1, &cstr, nullptr));
    GL_CALL(glCompileShader(fsId));
    checkShaderCompilation(fsId, GL_FRAGMENT_SHADER);
  }
  // Link
  mId = glCreateProgram();
  GL_CALL(glAttachShader(mId, vsId));
  GL_CALL(glAttachShader(mId, gsId));
  GL_CALL(glAttachShader(mId, fsId));
  GL_CALL(glLinkProgram(mId));
  checkShaderLinking(mId);
  // Delete shaders.
  GL_CALL(glDeleteShader(vsId));
  GL_CALL(glDeleteShader(gsId));
  GL_CALL(glDeleteShader(fsId));
}

void Shader::use() const
{
  GL_CALL(glUseProgram(mId));
}

void Shader::free()
{
  if (mId) {
    GL_CALL(glDeleteProgram(mId));
    mId = 0;
  }
}

Shader::~Shader()
{
  free();
}

}  // namespace view
