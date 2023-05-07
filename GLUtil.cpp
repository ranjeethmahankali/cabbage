#include <GLUtil.h>
#include <Game.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>
#include <array>
#include <numeric>

#include <freetype2/ft2build.h>
#include <string_view>
#include FT_FREETYPE_H

#include <Font.h>

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

static constexpr size_t NChars = 10;  // Just the numerical characters.

static void getFontData(std::vector<uint8_t>&           textureData,
                        std::array<size_t, NChars>&     offsets,
                        std::array<glm::ivec2, NChars>& sizes,
                        std::array<glm::ivec2, NChars>& bearings,
                        std::array<uint32_t, NChars>    advances,
                        uint32_t&                       charHeight)
{
  static constexpr std::string_view sErr       = "Unable to load font data";
  static constexpr uint32_t         FontHeight = uint32_t(0.3f * Arena::SquareSize);
  logger().info("Loading font...");
  FT_Library ftlib;
  if (FT_Init_FreeType(&ftlib)) {
    logger().error(sErr);
    return;
  }
  FT_Face face;
  if (FT_New_Memory_Face(
        ftlib, CascadiaMono_ttf.data(), CascadiaMono_ttf.size(), 0, &face)) {
    logger().error(sErr);
    return;
  }
  if (FT_Set_Pixel_Sizes(face, 0, FontHeight)) {
    logger().error(sErr);
    return;
  }
  char ch[2];
  ch[1] = '\0';
  for (int i = 0; i < 10; ++i) {
    std::sprintf(ch, "%d", i);
    if (FT_Load_Char(face, ch[0], FT_LOAD_RENDER)) {
      logger().error(sErr);
      return;
    }
    uint32_t width  = face->glyph->bitmap.width;
    uint32_t height = face->glyph->bitmap.rows;
    if (i == 0) {
      charHeight = height;
    }
    else if (charHeight != height) {
      logger().error(
        "The font face does not provide uniform height numerical characters.");
      return;
    }
    sizes[i]    = {int(width), int(height)};
    bearings[i] = glm::ivec2(face->glyph->bitmap_left, face->glyph->bitmap_top);
    offsets[i]  = textureData.size();
    advances[i] = face->glyph->advance.x;
    std::copy_n(reinterpret_cast<uint8_t*>(face->glyph->bitmap.buffer),
                width * height,
                std::back_inserter(textureData));
  }
}

class CharAtlas
{
  std::array<glm::ivec2, NChars> mBearings;
  std::array<glm::ivec2, NChars> mSizes;
  std::array<uint32_t, NChars>   mAdvances;
  std::array<glm::vec4, NChars>  mTexCoords;
  uint32_t                       mCharHeight;
  std::vector<uint8_t>           mTexture;

  CharAtlas()
  {
    std::vector<uint8_t>       textureData;
    std::array<size_t, NChars> offsets;
    getFontData(textureData, offsets, mSizes, mBearings, mAdvances, mCharHeight);
    uint32_t texWidth = std::accumulate(
      mSizes.begin(),
      mSizes.end(),
      uint32_t(0),
      [](uint32_t total, glm::ivec2 s) -> uint32_t { return total + uint32_t(s[0]); });
    mTexture.resize(mCharHeight * texWidth);
    float    wf        = float(mCharHeight);
    float    hf        = float(texWidth);
    uint8_t* tilestart = mTexture.data();
    for (int i = 0; i < NChars; ++i) {
      size_t   offset = offsets[i];
      uint8_t* src    = textureData.data() + offset;
      auto     dims   = mSizes[i];
      uint8_t* dst    = tilestart;
      tilestart += dims[0];
      for (uint32_t r = 0; r < dims.y; r++) {
        std::copy_n(src, dims.x, dst);
        src += dims.x;
        dst += texWidth;
      }
    }
  }
};

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
  vec2 x = vec2(0,0);
  vec2 y = vec2(0,0);
  bool emit = false;
  if (Type[0] == SQUARE) {{
    x = sqx;
    y = sqy;
    emit = true;
  }} else if (Type[0] == BALL) {{
    x = vec2(BallSizeX, 0.);
    y = vec2(0., BallSizeY);
    emit = true;
  }}
  else if (Type[0] == BALL_SPWN) {{
    x = sqx * 0.75;
    y = sqy * 0.75;
    emit = true;
  }}
  if (emit) {{
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

vec4 sampleFont() {{
  int digits[4] = int[](-1, -1, -1, -1);
  int data = FData;
  int base = 1000;
  int nDigits = 0;
  while (data > 0 && nDigits < 4) {{
    digits[nDigits++] = data / base;
    base /= 10;
  }}
  return vec4(0, 0, 0, 0);
}}

void main()
{{
  vec2 fc = gl_FragCoord.xy;
  fc.x /= Width;
  fc.y /= Height;
  fc = 2 * fc - vec2(1, 1);
  if (FType == SQUARE) {{
    float r = 7. * min(1., float(FData - 1) / float(MaxData - 1));
    int rt = int(ceil(r));
    int lt = int(floor(r));
    r = fract(r);
    vec4 baseColor = vec4(Colors[lt] * (1. - r) + Colors[rt] * r, 1.);
    vec4 fontColor = sampleFont();
    FragColor = fontColor.a * fontColor + (1. - fontColor.a) * baseColor;
  }} else if (FType == BALL) {{
    vec2 d = fc - ObjPos;
    d.x /= BallSizeX;
    d.y /= BallSizeY;
    float r = min(1, max(0, 1 - sqrt(dot(d, d))));
    r = 1 - pow(1 - r, 5);
    if (r > 0) FragColor = vec4(r, r, r, 1);
    else FragColor = Invisible;
  }} else if (FType == BALL_SPWN) {{
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
  getFontData();
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
