#include <GLUtil.h>
#include <Game.h>
#include <freetype/freetype.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>
#include <array>
#include <iomanip>
#include <numeric>
#include <sstream>
#include <string_view>

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
                        std::array<uint32_t, NChars>&   advances,
                        uint32_t&                       charHeight)
{
  static constexpr std::string_view sErr       = "Unable to load font data";
  static constexpr uint32_t         FontHeight = uint32_t(0.25f * Arena::SquareSize);
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
    sizes[i]    = glm::ivec2 {int(width), int(height)};
    bearings[i] = glm::ivec2(face->glyph->bitmap_left, face->glyph->bitmap_top);
    offsets[i]  = textureData.size();
    advances[i] = face->glyph->advance.x;
    std::copy_n(reinterpret_cast<uint8_t*>(face->glyph->bitmap.buffer),
                width * height,
                std::back_inserter(textureData));
  }
  FT_Done_Face(face);
  FT_Done_FreeType(ftlib);
}

class CharAtlas
{
  std::array<glm::ivec2, NChars> mBearings;
  std::array<glm::ivec2, NChars> mSizes;
  std::array<uint32_t, NChars>   mAdvances;
  std::array<glm::vec4, NChars>  mTexCoords;
  uint32_t                       mCharHeight;
  std::vector<uint8_t>           mTexture;
  uint32_t                       mTexId = 0;

  CharAtlas()
  {
    std::vector<uint8_t>       textureData;
    std::array<size_t, NChars> offsets;
    getFontData(textureData, offsets, mSizes, mBearings, mAdvances, mCharHeight);
    uint32_t tileX = uint32_t(
      std::max_element(mSizes.begin(), mSizes.end(), [](glm::ivec2 a, glm::ivec2 b) {
        return a[0] < b[0];
      })->x);
    uint32_t texWidth = tileX * NChars;
    // make it a multiple of 4 for alignment.
    if (texWidth % 4) {
      texWidth += 4 - (texWidth % 4);
    }
    mTexture.resize(mCharHeight * texWidth, 0);
    float    wf        = float(texWidth);
    float    hf        = float(mCharHeight);
    uint8_t* tilestart = mTexture.data();
    uint32_t tx        = 0;
    for (int i = 0; i < NChars; ++i) {
      size_t   offset = offsets[i];
      uint8_t* src    = textureData.data() + offset;
      auto     dims   = mSizes[i];
      uint8_t* dst    = tilestart;
      tilestart += tileX;
      for (uint32_t r = 0; r < dims.y; r++) {
        std::copy_n(src, dims.x, dst);
        src += dims.x;
        dst += texWidth;
      }
      mTexCoords[i] = {float(tx) / wf, 0.f, float(tx + dims.x) / wf, 1.f};
      tx += tileX;
    }
    // Init OpenGL texture.
    GLint pixformat = GL_RED;
    GL_CALL(glGenTextures(1, &mTexId));
    GL_CALL(glBindTexture(GL_TEXTURE_2D, mTexId));
    GL_CALL(glTexImage2D(GL_TEXTURE_2D,
                         0,
                         GL_RED,
                         texWidth,
                         mCharHeight,
                         0,
                         GL_RED,
                         GL_UNSIGNED_BYTE,
                         mTexture.data()));
    GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
    GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));
    GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
    GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR));
    GL_CALL(glBindTexture(GL_TEXTURE_2D, 0));
  }

  ~CharAtlas()
  {
    if (mTexId) {
      GL_CALL(glDeleteTextures(1, &mTexId));
      mTexId = 0;
    }
  }

public:
  void bind() const { GL_CALL(glBindTexture(GL_TEXTURE_2D, mTexId)); }

  static CharAtlas& get()
  {
    static CharAtlas sAtlas;
    return sAtlas;
  }

  const glm::vec4& textureCoords(int digit) { return mTexCoords[digit]; }

  std::string glslConstants() const
  {
    std::stringstream ss;
    ss << "const vec2 CharSizes[" << NChars << "] = vec2[](\n";
    for (int i = 0; i < NChars; ++i) {
      glm::vec2 s = 2.f * glm::vec2 {float(mSizes[i].x) / Arena::Width,
                                     float(mSizes[i].y / Arena::Height)};
      ss << "  vec2(" << s.x << ", " << s.y << ")" << (i + 1 < NChars ? "," : "") << "\n";
    }
    ss << ");\n";
    ss << "const vec2 CharBearings[" << NChars << "] = vec2[](\n";
    for (int i = 0; i < NChars; ++i) {
      glm::vec2 s = 2.f * glm::vec2 {float(mBearings[i].x) / Arena::Width,
                                     float(mBearings[i].y / Arena::Height)};
      ss << "  vec2(" << s.x << ", " << s.y << ")" << (i + 1 < NChars ? "," : "") << "\n";
    }
    ss << ");\n";
    ss << "const float CharAdvances[" << NChars << "] = float[](";
    ss << std::setprecision(8);
    for (int i = 0; i < NChars; ++i) {
      // Advances need to be divided by 64 to get the number of pixels.
      float a = 2.f * float(mAdvances[i] >> 6) / Arena::Width;
      ss << a << (i + 1 < NChars ? ", " : "");
    }
    ss << ");\n";
    ss << "const vec4 CharTxCoords[" << NChars << "] = vec4[](\n";
    for (int i = 0; i < NChars; ++i) {
      const auto& t = mTexCoords[i];
      ss << "  vec4(" << t[0] << ", " << t[1] << ", " << t[2] << ", " << t[3] << ")"
         << (i + 1 < NChars ? "," : "") << "\n";
    }
    ss << ");\n";
    return ss.str();
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
                     fmt::arg("nosq", int(T_NOSQUARE)),
                     fmt::arg("sq", int(T_SQUARE)),
                     fmt::arg("bspwn", int(T_BALL_SPWN)),
                     fmt::arg("nbl", int(T_NOBALL)),
                     fmt::arg("bl", int(T_BALL)),
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

{CharConstants}

const float SqSizeX = {xx:.8f};
const float SqSizeY = {yy:.8f};
const float BallSizeX = {bsizex:.8f};
const float BallSizeY = {bsizey:.8f};
const float Width = {ww:.8f};
const float Height = {hh:.8f};
const int MaxData = 50;
const vec4 White = vec4(1,1,1,1);
const vec4 Invisible = vec4(0, 0, 0, 0);
const float BallFeather = 0.75;
const float BallSpawnSize = {bspsize};

const int NOSQUARE       = {nosq};
const int SQUARE         = {sq};
const int BALL_SPWN      = {bspwn};
const int NOBALL         = {nbl};
const int BALL           = {bl};

uniform sampler2D CharTexture;

vec4 sampleFont(vec2 fc) {{
  int digits[4] = int[](-1, -1, -1, -1);
  int data = FData;
  int base = 10;
  int nDigits = max(1, int(ceil(log(float(FData)) / log(10.))));
  for (int i = nDigits - 1; i > -1; --i) {{
    digits[i] = data % base;
    data = (data - digits[i]) / base;
    base *= 10;
  }}
  vec2 bmin = vec2(1, 1);
  vec2 bmax = vec2(-1,-1);
  vec2 cur = vec2(0, 0);
  for (int i = 0; i < nDigits; ++i) {{
    int d = digits[i];
    vec2 bearing = CharBearings[d];
    vec2 size = CharSizes[d];
    vec2 pos = vec2(cur.x + bearing.x, cur.y - (size.y - bearing.y));
    bmin.y = min(bmin.y, pos.y);
    bmin.x = min(bmin.x, pos.x);
    bmax.y = max(bmax.y, pos.y);
    bmax.x = max(bmax.x, pos.x);
    pos += size;
    bmin.y = min(bmin.y, pos.y);
    bmin.x = min(bmin.x, pos.x);
    bmax.y = max(bmax.y, pos.y);
    bmax.x = max(bmax.x, pos.x);
    cur.x += CharAdvances[d];
  }}
  cur = ObjPos + 0.5 * (bmin - bmax);
  for (int i = 0; i < nDigits; ++i) {{
    int d = digits[i];
    vec2 bearing = CharBearings[d];
    vec2 size = CharSizes[d];
    vec2 p1 = vec2(cur.x + bearing.x, cur.y - (size.y - bearing.y));
    vec2 p2 = p1 + size;
    if (p1.x <= fc.x && p1.y <= fc.y && fc.x <= p2.x && fc.y <= p2.y) {{
      fc = fc - p1;
      fc.x /= p2.x - p1.x;
      fc.y /= p2.y - p1.y;
      vec4 tc = CharTxCoords[d];
      fc.x = tc.x + fc.x * (tc.z - tc.x);
      fc.y = 1 - (tc.y + fc.y * (tc.w - tc.y));
      float t = 1 - texture(CharTexture, fc).r;
      return vec4(t, t, t, 1 - t);
    }}
    cur.x += CharAdvances[d];
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
    vec4 fontColor = sampleFont(fc);
    FragColor = fontColor.a * fontColor + (1. - fontColor.a) * baseColor;
  }} else if (FType == BALL) {{
    vec2 d = fc - ObjPos;
    d.x /= BallSizeX;
    d.y /= BallSizeY;
    float r = sqrt(dot(d, d));
    r = min(1, max(0, (r - BallFeather) / (1. - BallFeather)));
    FragColor = (1. - r) * White + r * Invisible;
  }} else if (FType == BALL_SPWN) {{
    vec2 d = fc - ObjPos;
    const float s1 = BallSpawnSize - 0.3;
    const float s2 = BallSpawnSize - 0.1;
    const float s3 = BallSpawnSize;
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
                     fmt::arg("nosq", int(T_NOSQUARE)),
                     fmt::arg("sq", int(T_SQUARE)),
                     fmt::arg("bspwn", int(T_BALL_SPWN)),
                     fmt::arg("nbl", int(T_NOBALL)),
                     fmt::arg("bl", int(T_BALL)),
                     fmt::arg("bsizex", GlslBallDim.x),
                     fmt::arg("bsizey", GlslBallDim.y),
                     fmt::arg("ww", Arena::Width),
                     fmt::arg("hh", Arena::Height),
                     fmt::arg("xx", Arena::SquareSize / Arena::Width),
                     fmt::arg("yy", Arena::SquareSize / Arena::Height),
                     fmt::arg("CharConstants", CharAtlas::get().glslConstants()),
                     fmt::arg("bspsize", Arena::BallSpawnRelSize));
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
  // Bind texture for text rendering.
  CharAtlas::get().bind();
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
