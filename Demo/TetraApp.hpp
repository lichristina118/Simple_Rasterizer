//
//  TetraApp.hpp
//  Demo of basic usage of nanogui and GLWrap to get a simple object on the screen.
//
//  Created by srm, March 2020
//

#pragma once

#include <nanogui/screen.h>

#include <GLWrap/Program.hpp>
#include <GLWrap/Mesh.hpp>
#include <RTUtil/Camera.hpp>
#include <RTUtil/CameraController.hpp>


class TetraApp : public nanogui::Screen {
public:

    TetraApp();

    virtual bool keyboardEvent(int key, int scancode, int action, int modifiers) override;
    virtual bool mouseButtonEvent(const Eigen::Vector2i &p, int button, bool down, int modifiers) override;
    virtual bool mouseMotionEvent(const Eigen::Vector2i &p, const Eigen::Vector2i &rel, int button, int modifiers) override;
    virtual bool scrollEvent(const Eigen::Vector2i &p, const Eigen::Vector2f &rel) override;

    virtual void drawContents() override;

private:

    static const int windowWidth;
    static const int windowHeight;

    std::unique_ptr<GLWrap::Program> prog;
    std::unique_ptr<GLWrap::Mesh> mesh;

    std::shared_ptr<RTUtil::PerspectiveCamera> cam;
    std::unique_ptr<RTUtil::DefaultCC> cc;

    nanogui::Color backgroundColor;

public:
    EIGEN_MAKE_ALIGNED_OPERATOR_NEW
};


