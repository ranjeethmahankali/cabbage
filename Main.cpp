#include <Corrade/Containers/Pointer.h>
#include <Magnum/GL/DefaultFramebuffer.h>
#include <Magnum/GL/Renderer.h>
// #include <Magnum/ImGuiIntegration/Context.h>
#include <Magnum/Platform/Sdl2Application.h>
#include <Magnum/SceneGraph/Drawable.h>
#include <Magnum/SceneGraph/MatrixTransformation2D.h>
#include <Magnum/SceneGraph/Object.h>
#include <Magnum/SceneGraph/Scene.h>
#include <Magnum/SceneGraph/SceneGraph.h>
#include <Magnum/Tags.h>
#include <imgui.h>
#include <glm/glm.hpp>

namespace baller {

using namespace Magnum;
using Scene2D  = SceneGraph::Scene<SceneGraph::MatrixTransformation2D>;
using Object2D = SceneGraph::Object<SceneGraph::MatrixTransformation2D>;

class App : public Platform::Application
{
public:
  explicit App(const Arguments& args);

protected:
  // void viewportEvent(ViewportEvent& event) override;
  // void mousePressEvent(MouseEvent& event) override;
  // void mouseReleaseEvent(MouseEvent& event) override;
  // void mouseMoveEvent(MouseMoveEvent& event) override;
  void drawEvent() override;

  // ImGuiIntegration::Context                        mImGuiContext {NoCreate};
  Containers::Pointer<Scene2D>                     mScene;
  Containers::Pointer<SceneGraph::DrawableGroup2D> mDrawables;
  Containers::Pointer<Object2D>                    mObjCamera;
  Containers::Pointer<SceneGraph::Camera2D>        mCamera;
};

App::App(const Arguments& args)
    : Platform::Application(args, NoCreate)
{
  // Setup window.
  const Vector2 dpiScaling = this->dpiScaling({});
  Configuration conf;
  conf.setTitle("Baller")
    .setSize(conf.size(), dpiScaling)
    .setWindowFlags(Configuration::WindowFlag::Resizable);
  GLConfiguration glConf;
  glConf.setSampleCount(dpiScaling.max() < 2.f ? 8 : 2);
  if (!tryCreate(conf, glConf)) {
    create(conf, glConf.setSampleCount(0));
  }
  // Setup ImGui
  // ImGui::CreateContext();
  // ImGui::StyleColorsDark();
  // mImGuiContext = ImGuiIntegration::Context {*ImGui::GetCurrentContext(),
  //                                            Vector2 {windowSize()} / dpiScaling,
  //                                            windowSize(),
  //                                            framebufferSize()};
  GL::Renderer::setBlendFunction(GL::Renderer::BlendFunction::SourceAlpha,
                                 GL::Renderer::BlendFunction::OneMinusSourceAlpha);
  // Setup scene objects and camera.
  mScene.emplace();
  mDrawables.emplace();
  mObjCamera.emplace(mScene.get());
  // mObjCamera->setTransformation(Matrix3::translation(gridCenter()))
}

void App::drawEvent()
{
  GL::defaultFramebuffer.clear(GL::FramebufferClear::Color | GL::FramebufferClear::Depth);
  // mImGuiContext.newFrame();
  // Draw objects.
}

}  // namespace baller

MAGNUM_APPLICATION_MAIN(baller::App)
