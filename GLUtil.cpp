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

void main()
{{
  Data = data;
  Type = type;
  vec2 pos = position;
  pos.x = 2. * (position.x / {width:.8f}) - 1.;
  pos.y = 2. * (position.y / {height:.8f}) - 1.;
  gl_Position = vec4(pos.xy, 0., 1.);
}}

)";
  return fmt::format(
    sTemplate, fmt::arg("width", Arena::Width), fmt::arg("height", Arena::Height));
}

static constexpr glm::vec2 GlslBallDim =
  2.f * glm::vec2 {Arena::BallRadius / Arena::Width, Arena::BallRadius / Arena::Height};

static std::string geoShaderSrc()
{
  static constexpr char sTemplate[] = R"(
#version 330 core
layout (points) in;
layout (triangle_strip, max_vertices = 4) out;

const int NOSQUARE       = {nosq};
const int SQUARE         = {sq};
const int BALL_SPWN      = {bspwn};
const int NOBALL         = {nbl};
const int BALL           = {bl};

const vec2 sqx = vec2({xx:.8f}, 0.);
const vec2 sqy = vec2(0., {yy:.8f});
const float BallSizeX = {bsizex:.8f};
const float BallSizeY = {bsizey:.8f};

in int Data[];
in int Type[];
flat out int FData;
flat out int FType;
flat out vec2 ObjPos;

void main() {{
  FData = Data[0];
  FType = Type[0];
  ObjPos = gl_in[0].gl_Position.xy;
  vec2 x;
  vec2 y;
  if (Type[0] == SQUARE) {{
    x = sqx;
    y = sqy;
  }} else if (Type[0] == BALL) {{
    x = vec2(BallSizeX, 0.);
    y = vec2(0., BallSizeY);
  }}
  else if (Type[0] == BALL_SPWN) {{
    x = sqx * 0.75;
    y = sqy * 0.75;
  }}
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

)";
  return fmt::format(sTemplate,
                     fmt::arg("nosq", int(NOSQUARE)),
                     fmt::arg("sq", int(SQUARE)),
                     fmt::arg("bspwn", int(BALL_SPWN)),
                     fmt::arg("nbl", int(NOBALL)),
                     fmt::arg("bl", int(BALL)),
                     fmt::arg("xx", Arena::SquareSize / Arena::Width),
                     fmt::arg("yy", Arena::SquareSize / Arena::Height),
                     fmt::arg("bsizex", GlslBallDim.x),
                     fmt::arg("bsizey", GlslBallDim.y));
}

static std::string fragShaderSrc()
{
  static constexpr char sTemplate[] = R"(
#version 330 core

out vec4 FragColor;

flat in int FData;
flat in int FType;
flat in vec2 ObjPos;

const vec3 Colors[7] = vec3[](
  vec3(1, 1, 0),
  vec3(0, 1, 0),
  vec3(0, 0, 1),
  vec3(0.29, 0, 0.51),
  vec3(0.93, 0.51, 0.93),
  vec3(1, 0, 0),
  vec3(1, 0.5, 0)
);

const float SqSizeX = {xx:.8f};
const float SqSizeY = {yy:.8f};
const float BallSizeX = {bsizex:.8f};
const float BallSizeY = {bsizey:.8f};
const float Width = {ww:.8f};
const float Height = {hh:.8f};
const int MaxData = 50;
const vec4 White = vec4(1,1,1,1);
const vec4 Invisible = vec4(0, 0, 0, 0);
const float BallFeather = 0.95;

const int NOSQUARE       = {nosq};
const int SQUARE         = {sq};
const int BALL_SPWN      = {bspwn};
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
    vec2 fc = gl_FragCoord.xy;
    fc.x /= Width;
    fc.y /= Height;
    fc = 2 * fc - vec2(1, 1);
    vec2 d = fc - ObjPos;
    d.x /= BallSizeX;
    d.y /= BallSizeY;
    float r = min(1, max(0, 1 - sqrt(dot(d, d))));
    r = 1 - pow(1 - r, 5);
    if (r > 0) FragColor = vec4(r, r, r, 1);
    else FragColor = Invisible;
  }} else if (FType == BALL_SPWN) {{
    vec2 fc = gl_FragCoord.xy;
    fc.x /= Width;
    fc.y /= Height;
    fc = 2 * fc - vec2(1, 1);
    vec2 d = fc - ObjPos;
    const float s1 = 0.25;
    const float s2 = 0.45;
    const float s3 = 0.55;
    bool b1 = abs(d.x) < SqSizeX * s1 && abs(d.y) < SqSizeY * s1;
    bool b2 = abs(d.x) < SqSizeX * s2 && abs(d.y) < SqSizeY * s2;
    bool b3 = abs(d.x) < SqSizeX * s3 && abs(d.y) < SqSizeY * s3;
    if (b1) FragColor = White;
    else if (b2) FragColor = Invisible;
    else if (b3) FragColor = White;
    else FragColor = Invisible;
  }}
}}

)";
  return fmt::format(sTemplate,
                     fmt::arg("nosq", int(NOSQUARE)),
                     fmt::arg("sq", int(SQUARE)),
                     fmt::arg("bspwn", int(BALL_SPWN)),
                     fmt::arg("nbl", int(NOBALL)),
                     fmt::arg("bl", int(BALL)),
                     fmt::arg("bsizex", GlslBallDim.x),
                     fmt::arg("bsizey", GlslBallDim.y),
                     fmt::arg("ww", Arena::Width),
                     fmt::arg("hh", Arena::Height),
                     fmt::arg("xx", Arena::SquareSize / Arena::Width),
                     fmt::arg("yy", Arena::SquareSize / Arena::Height));
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
    std::string src  = geoShaderSrc();
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
