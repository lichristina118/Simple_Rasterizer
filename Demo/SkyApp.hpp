//
//  SkyApp.hpp
//  Demo of usage of sky modal and of Nanogui controls.
//
//  Created by srm, April 2020
//

#pragma once

#include <nanogui/screen.h>

#include <GLWrap/Program.hpp>
#include <GLWrap/Mesh.hpp>

#include <RTUtil/Sky.hpp>


class SkyApp : public nanogui::Screen {
public:

    SkyApp();

    virtual bool keyboardEvent(int key, int scancode, int action, int modifiers) override;

    virtual void drawContents() override;

private:

    std::unique_ptr<GLWrap::Program> prog;
    std::unique_ptr<GLWrap::Mesh> fsqMesh;

    RTUtil::Sky sky;

    // Exposure for HDR -> LDR conversion
    float exposure = 1.0f;

    nanogui::Window* controlPanel = nullptr;

    void buildGUI();

public:
    EIGEN_MAKE_ALIGNED_OPERATOR_NEW
};


