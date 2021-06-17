//
//  SkyApp.cpp
//
//  Created by srm, April 2020
//

#include "SkyApp.hpp"

#include <nanogui/window.h>
#include <nanogui/glcanvas.h>
#include <nanogui/layout.h>
#include <nanogui/slider.h>
#include <nanogui/label.h>

#include <cpplocate/cpplocate.h>



SkyApp::SkyApp() :
nanogui::Screen(Eigen::Vector2i(600, 600), "Sky Demo", false),
sky(30 * M_PI/180, 4) {

  const std::string resourcePath =
        cpplocate::locatePath("resources/Common", "", nullptr) + "resources/";

  // Set up a shader program to visualize the sky
  prog.reset(new GLWrap::Program("program", { 
      { GL_VERTEX_SHADER, resourcePath + "Common/shaders/fsq.vert" },
      { GL_FRAGMENT_SHADER, resourcePath + "Common/shaders/sunsky.fs" },
      { GL_FRAGMENT_SHADER, resourcePath + "Common/shaders/skyvis.fs" }
  }));

  // Create a two-triangle mesh for drawing a full screen quad
  Eigen::MatrixXf vertices(5, 4);
  vertices.col(0) << -1.0f, -1.0f, 0.0f, 0.0f, 0.0f;
  vertices.col(1) << 1.0f, -1.0f, 0.0f, 1.0f, 0.0f;
  vertices.col(2) << 1.0f, 1.0f, 0.0f, 1.0f, 1.0f;
  vertices.col(3) << -1.0f, 1.0f, 0.0f, 0.0f, 1.0f;

  Eigen::Matrix<float, 3, Eigen::Dynamic> positions = vertices.topRows<3>();
  Eigen::Matrix<float, 2, Eigen::Dynamic> texCoords = vertices.bottomRows<2>();

  fsqMesh.reset(new GLWrap::Mesh());

  fsqMesh->setAttribute(0, positions);
  fsqMesh->setAttribute(1, texCoords);

  // Build the control panel
  buildGUI();
  // Arrange windows in the layout we have described
  performLayout();
  // Make the window visible and start the application's main loop
  setVisible(true);
}



void SkyApp::buildGUI() {

  // Creates a window that has this screen as the parent.
  // NB: even though this is a raw pointer, nanogui manages this resource internally.
  // Do not delete it!
  controlPanel = new nanogui::Window(this, "Control Panel");
  controlPanel->setFixedWidth(220);
  controlPanel->setPosition(Eigen::Vector2i(15, 15));
  controlPanel->setLayout(new nanogui::BoxLayout(nanogui::Orientation::Vertical,
                                                 nanogui::Alignment::Middle,
                                                 5, 5));

  // Create a slider widget that adjusts the sun angle parameter
  nanogui::Widget* elevWidget = new nanogui::Widget(controlPanel);
  elevWidget->setLayout(new nanogui::BoxLayout(nanogui::Orientation::Horizontal,
                                               nanogui::Alignment::Middle,
                                               0, 5));
  new nanogui::Label(elevWidget, "Sun Theta:");
  nanogui::Slider* elevSlider = new nanogui::Slider(elevWidget);
  elevSlider->setRange(std::make_pair(0.0f, 90.0f));
  elevSlider->setValue(30.0f);
  elevSlider->setCallback([this] (float value) {
    sky.setThetaSun(value * M_PI / 180);
  });

  // Create a slider that adjusts the turbidity
  nanogui::Widget* turbWidget = new nanogui::Widget(controlPanel);
  turbWidget->setLayout(new nanogui::BoxLayout(nanogui::Orientation::Horizontal,
                                               nanogui::Alignment::Middle,
                                               0, 5));
  new nanogui::Label(turbWidget, "Turbidity:");
  nanogui::Slider* turbSlider = new nanogui::Slider(turbWidget);
  turbSlider->setRange(std::make_pair(2.0f, 6.0f));
  turbSlider->setValue(4.0f);
  turbSlider->setCallback([this] (float value) {
    sky.setTurbidity(value);
  });
}



bool SkyApp::keyboardEvent(int key, int scancode, int action, int modifiers) {
  // First, see if the parent method accepts the keyboard event.
  // If so, the event is already handled.
  if (Screen::keyboardEvent(key, scancode, action, modifiers))
    return true;

  // If the user presses the escape key...
  if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
    // ...exit the application.
    setVisible(false);
    return true;
  }

  // Space to show/hide control panel
  if (key == GLFW_KEY_SPACE && action == GLFW_PRESS) {
    controlPanel->setVisible(!controlPanel->visible());
    return true;
  }

  // Up and down arrows to adjust exposure
  if (key == GLFW_KEY_UP && action == GLFW_PRESS) {
    exposure *= 2.0;
    return true;
  }
  if (key == GLFW_KEY_DOWN && action == GLFW_PRESS) {
    exposure /= 2.0;
    return true;
  }

  // The event is not handled here.
  return false;
}


void SkyApp::drawContents() {

  glClearColor(0, 0, 0, 0);
  glClear(GL_COLOR_BUFFER_BIT);

  prog->use();
  sky.setUniforms(*prog);
  prog->uniform("exposure", exposure);
  fsqMesh->drawArrays(GL_TRIANGLE_FAN, 0, 4);
  prog->unuse();

}
