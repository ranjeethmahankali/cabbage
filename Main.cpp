#include <iostream>

#include <GLUtil.h>
#include <Game.h>
#include <box2d/box2d.h>

static void glfw_error_cb(int error, const char* desc)
{
  view::logger().error("GLFW Error {}: {}", error, desc);
}

static void onMouseButton(GLFWwindow* window, int button, int action, int mods) {}

void onMouseMove(GLFWwindow* window, double xpos, double ypos) {}

int initGL(GLFWwindow*& window)
{
  glfwSetErrorCallback(glfw_error_cb);
  if (!glfwInit()) {
    view::logger().error("Failed to initialize GLFW.");
    return 1;
  }
  view::logger().info("Initialized GLFW.");
  // Window setup
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
  glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
  std::string title = "Cabbage";
  window = glfwCreateWindow(Arena::Width, Arena::Height, title.c_str(), nullptr, nullptr);
  if (window == nullptr) {
    return 1;
  }
  glfwMakeContextCurrent(window);
  // OpenGL bindings
  int err = GLEW_OK;
  if ((err = glewInit()) != GLEW_OK) {
    view::logger().error("Failed to initialize OpenGL bindings: {}", err);
    return 1;
  }
  view::logger().info("OpenGL bindings are ready.");
  int W, H;
  GL_CALL(glfwGetFramebufferSize(window, &W, &H));
  GL_CALL(glViewport(0, 0, W, H));
  GL_CALL(glEnable(GL_DEPTH_TEST));
  GL_CALL(glEnable(GL_BLEND));
  GL_CALL(glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA));
  GL_CALL(glEnable(GL_LINE_SMOOTH));
  GL_CALL(glEnable(GL_PROGRAM_POINT_SIZE));
  GL_CALL(glPointSize(3.0f));
  GL_CALL(glLineWidth(1.0f));
  GL_CALL(glPolygonMode(GL_FRONT_AND_BACK, GL_FILL));
  glfwSetMouseButtonCallback(window, &onMouseButton);
  glfwSetCursorPosCallback(window, onMouseMove);
  return 0;
}

static int game()
{
  GLFWwindow* window = nullptr;
  try {
    int err = 0;
    if ((err = initGL(window))) {
      view::logger().error("Failed to initialize the viewier. Error code {}.", err);
      return err;
    }
    {
      b2World world(b2Vec2(0.f, 0.f));
      Arena   arena(world);
      arena.advance(42);
      arena.advance(23);
      view::Shader shader;
      shader.use();
      // debug
      arena.shoot(M_PI / 4.f);
      // debug
      // TODO: Initialize and use shader
      while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        glClearColor(0.1f, 0.1f, 0.1f, 1.f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        // Draw stuff.
        arena.draw();
        world.Step(1, 3, 3);
        arena.step();
        glfwSwapBuffers(window);
      }
    }
    view::logger().info("Closing window...\n");
    glfwDestroyWindow(window);
    // TODO: Free shader and other resources.
    glfwTerminate();
  }
  catch (const std::exception& e) {
    view::logger().critical("Fatal Error: {}", e.what());
    return 1;
  }
  return 0;
}

int main(int argc, char** argv)
{
  return game();
}
