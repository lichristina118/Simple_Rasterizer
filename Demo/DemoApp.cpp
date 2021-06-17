//
//  DemoApp.cpp
//  Demo
//
//  Created by eschweic on 1/23/19.
//

#include "DemoApp.hpp"
#include <nanogui/window.h>
#include <nanogui/glcanvas.h>
#include <nanogui/layout.h>
#include <nanogui/button.h>

const int DemoApp::windowWidth = 800;
const int DemoApp::windowHeight = 600;

DemoApp::DemoApp() :
nanogui::Screen(Eigen::Vector2i(windowWidth, windowHeight), "NanoGUI Demo", false),
backgroundColor(0.4f, 0.4f, 0.7f, 1.0f) {
  // Creates a window that has this screen as the parent.
  // NB: even though this is a raw pointer, nanogui manages this resource internally.
  // Do not delete it!
  nanogui::Window *window = new nanogui::Window(this, "Window Demo");
  window->setPosition(Eigen::Vector2i(15, 15));
  window->setLayout(new nanogui::GroupLayout());

  // Create a button with the window as its parent.
  // NB: even though this is a raw pointer, nanogui manages this resource internally.
  // Do not delete it!
  nanogui::Button *button = new nanogui::Button(window, "Random Color");
  button->setCallback([&]() {
    // Generate 3 uniformly random floats in the range [-1, 1]
    Eigen::Vector3f random = Eigen::Vector3f::Random();
    // Convert the random numbers to the range [0, 1]
    Eigen::Vector3f color =  0.5f * random + Eigen::Vector3f::Constant(0.5f);
    // Assign it to the background color.
    // nanogui::Color just wraps Eigen::Vector4f, so we can set the first 3 entries directly
    // with an Eigen::Vector3f.
    // Note this means that the alpha value of the color is unchanged.
    backgroundColor.head<3>() = color;
  });

  // Arrange windows in the layout we have described
  performLayout();
  // Make the window visible and start the application's main loop
  setVisible(true);
}

bool DemoApp::keyboardEvent(int key, int scancode, int action, int modifiers) {
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

  // Otherwise, the event is not handled here.
  return false;
}

void DemoApp::drawContents() {
  // All OpenGL code goes here!
  glClearColor(backgroundColor.r(), backgroundColor.g(), backgroundColor.b(), backgroundColor.w());
  glClear(GL_COLOR_BUFFER_BIT);
}
