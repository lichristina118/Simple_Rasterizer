//
//  TetraApp.cpp
//
//  Created by srm, March 2020
//

#include "TetraApp.hpp"
#include <nanogui/window.h>
#include <nanogui/glcanvas.h>
#include <nanogui/layout.h>

#include <cpplocate/cpplocate.h>

// Fixed screen size is awfully convenient, but you can also
// call Screen::setSize to set the size after the Screen base
// class is constructed.
const int TetraApp::windowWidth = 800;
const int TetraApp::windowHeight = 600;


// Constructor runs after nanogui is initialized and the OpenGL context is current.
TetraApp::TetraApp()
: nanogui::Screen(Eigen::Vector2i(windowWidth, windowHeight), "Tetrahedron Demo", false),
  backgroundColor(0.4f, 0.4f, 0.7f, 1.0f) {

    const std::string resourcePath =
        cpplocate::locatePath("resources/Common", "", nullptr) + "resources/";

    // Set up a simple shader program by passing the shader filenames to the convenience constructor
    prog.reset(new GLWrap::Program("program", { 
        { GL_VERTEX_SHADER, resourcePath + "Common/shaders/min.vs" },
        { GL_GEOMETRY_SHADER, resourcePath + "Common/shaders/flat.gs" },
        { GL_FRAGMENT_SHADER, resourcePath + "Common/shaders/lambert.fs" }
    }));

    // Create a camera in a default position, respecting the aspect ratio of the window.
    cam = std::make_shared<RTUtil::PerspectiveCamera>(
        Eigen::Vector3f(6,2,10), // eye
        Eigen::Vector3f(0,0,0), // target
        Eigen::Vector3f(0,1,0), // up
        windowWidth / (float) windowHeight, // aspect
        0.1, 50.0, // near, far
        20.0 * M_PI/180 // fov
    );

    cc.reset(new RTUtil::DefaultCC(cam));

    mesh.reset(new GLWrap::Mesh());

    // Mesh was default constructed, but needs to be set up with buffer data
    // These are the vertices of a tetrahedron that fits in the canonical view volume
    Eigen::Matrix<float, 3, Eigen::Dynamic> positions(3,4);
    positions.col(0) << 1, 1/sqrt(2), 0;
    positions.col(1) << -1, 1/sqrt(2), 0;
    positions.col(2) << 0, -1/sqrt(2), 1;
    positions.col(3) << 0, -1/sqrt(2), -1;
    mesh->setAttribute(0, positions);

    Eigen::VectorXi indices(3*4);
    indices << 0, 1, 2,
               0, 2, 3,
               0, 3, 1,
               1, 3, 2;
    mesh->setIndices(indices, GL_TRIANGLES);

    // NanoGUI boilerplate
    performLayout();
    setVisible(true);
}


bool TetraApp::keyboardEvent(int key, int scancode, int action, int modifiers) {

    if (Screen::keyboardEvent(key, scancode, action, modifiers))
       return true;

    // If the user presses the escape key...
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
        // ...exit the application.
        setVisible(false);
        return true;
    }

    return cc->keyboardEvent(key, scancode, action, modifiers);
}

bool TetraApp::mouseButtonEvent(const Eigen::Vector2i &p, int button, bool down, int modifiers) {
    return Screen::mouseButtonEvent(p, button, down, modifiers) ||
           cc->mouseButtonEvent(p, button, down, modifiers);
}

bool TetraApp::mouseMotionEvent(const Eigen::Vector2i &p, const Eigen::Vector2i &rel, int button, int modifiers) {
    return Screen::mouseMotionEvent(p, rel, button, modifiers) ||
           cc->mouseMotionEvent(p, rel, button, modifiers);
}

bool TetraApp::scrollEvent(const Eigen::Vector2i &p, const Eigen::Vector2f &rel) {
    return Screen::scrollEvent(p, rel) ||
           cc->scrollEvent(p, rel);
}



void TetraApp::drawContents() {
    GLWrap::checkGLError("drawContents start");
    glClearColor(backgroundColor.r(), backgroundColor.g(), backgroundColor.b(), backgroundColor.w());
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glEnable(GL_DEPTH_TEST);

    prog->use();
    prog->uniform("mM", Eigen::Affine3f::Identity().matrix());
    prog->uniform("mV", cam->getViewMatrix().matrix());
    prog->uniform("mP", cam->getProjectionMatrix().matrix());
    prog->uniform("k_a", Eigen::Vector3f(0.1, 0.1, 0.1));
    prog->uniform("k_d", Eigen::Vector3f(0.9, 0.9, 0.9));
    prog->uniform("lightDir", Eigen::Vector3f(1.0, 1.0, 1.0).normalized());

    mesh->drawElements();
    prog->unuse();
}


