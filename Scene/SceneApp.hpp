#pragma once

#include <string.h>
#include <iostream>
#include <nanogui/screen.h>

#include <GLWrap/Program.hpp>
#include <GLWrap/Mesh.hpp>
#include <GLWrap/Framebuffer.hpp>
#include <GLWrap/Shader.hpp>

#include <../ext/assimp/include/assimp/scene.h>
#include <../ext/assimp/include/assimp/Importer.hpp>

#include <RTUtil/Camera.hpp>
#include <RTUtil/CameraController.hpp>
#include <RTUtil/sceneinfo.hpp>
#include <RTUtil/Sky.hpp>
using namespace RTUtil;

#include "Scene.hpp"

// sky box files
struct SkyboxFiles {
    std::string mRight;
    std::string mLeft;
    std::string mTop;
    std::string mBottom;
    std::string mFront;
    std::string mBack;
};

class SceneApp : public nanogui::Screen {
public:

    SceneApp(std::string inputFile, std::string infoFile, std::string skyboxName);

    virtual bool keyboardEvent(int key, int scancode, int action, int modifiers) override;
    virtual bool mouseButtonEvent(const Eigen::Vector2i &p, int button, bool down, int modifiers) override;
    virtual bool mouseMotionEvent(const Eigen::Vector2i &p, const Eigen::Vector2i &rel, int button, int modifiers) override;
    virtual bool scrollEvent(const Eigen::Vector2i &p, const Eigen::Vector2f &rel) override;

    virtual void drawContents() override;

private:

    static const int windowWidth;
    static const int windowHeight;

    std::unique_ptr<RTUtil::DefaultCC> cc;
    //std::shared_ptr<RTUtil::PerspectiveCamera> cam;

    std::unique_ptr<GLWrap::Mesh> mesh;
    std::unique_ptr<GLWrap::Mesh> fsqMesh;
    std::unique_ptr<GLWrap::Mesh> skyboxMesh;

    std::unique_ptr<GLWrap::Program> forwardRenderProg;

    std::unique_ptr<GLWrap::Program> geoPassProg;
    std::unique_ptr<GLWrap::Program> shadowPassProg;
    std::unique_ptr<GLWrap::Program> pointLightPassProg;
    std::unique_ptr<GLWrap::Program> ambientLightPassProg;
    std::unique_ptr<GLWrap::Program> blurPassProg;
    std::unique_ptr<GLWrap::Program> srgbPassProg;
    std::unique_ptr<GLWrap::Program> sunSkyPassProg;
    std::unique_ptr<GLWrap::Program> mergePassProg;
    std::unique_ptr<GLWrap::Program> skyboxRflctPassProg;
    std::unique_ptr<GLWrap::Program> skyboxPassProg;

    std::shared_ptr<GLWrap::Framebuffer> gBuffer;
    std::shared_ptr<GLWrap::Framebuffer> accumulationBuffer;
    std::shared_ptr<GLWrap::Framebuffer> shadowMapBuffer;
    std::shared_ptr<GLWrap::Framebuffer> tempBuffer1;
    std::shared_ptr<GLWrap::Framebuffer> tempBuffer2;
    std::shared_ptr<GLWrap::Framebuffer> mergeBuffer;
    std::shared_ptr<GLWrap::Framebuffer> skyboxBuffer;

    std::shared_ptr<RTUtil::Sky> mSky;
    unsigned int mSkyboxTextureID;
    std::string mSkyboxName;
    Eigen::Matrix<float, 3, Eigen::Dynamic> mSkyboxVertices;

    nanogui::Color backgroundColor;

    Scene* mScene;
    bool mUseDefaultCamera;
    bool mUseFlatShader;
    bool mDeferredRendering;
    bool mShowGBuffers;
    bool mShowShadowMapBuffer;
    bool mShowSunSky;
    bool mShowBlur;
    bool mShowSkybox;
    bool mShowMirrorRflt; // show skybox mirror reflection

    void setCamera();
    void setShaders();

    void drawMeshes(Node* node, std::unique_ptr<GLWrap::Program> &prog, bool bMat);
    void forwardRendering();
    void renderQuad(std::unique_ptr<GLWrap::Program> &prog);

    void displayGBuffers();
    void displayFBuffer(std::shared_ptr<GLWrap::Framebuffer> buffer);

    void setCameraUniforms(std::unique_ptr<GLWrap::Program> &prog, bool bEye);
    void setWindowUniforms(std::unique_ptr<GLWrap::Program> &prog);
    void setLightUniforms(std::unique_ptr<GLWrap::Program> &prog, std::shared_ptr<RTUtil::LightInfo> pointLight);
    
    void geometryPass();
    void shadowPass(std::shared_ptr<RTUtil::LightInfo> light, std::shared_ptr<RTUtil::PerspectiveCamera> camera);
    void pointLightingPass(std::shared_ptr<RTUtil::LightInfo> light, std::shared_ptr<RTUtil::PerspectiveCamera> lightCam); 
    void ambientLightingPass(std::shared_ptr<RTUtil::LightInfo> light);
    void sunSkyPass();
    void blurPass(); 
    void mergePass();
    void skyboxPass();
    void skyboxMirrorReflectionPass();

    void printTransformation(std::string name, aiMatrix4x4 t);
    void printConfig();

    unsigned int loadSkyBox();
    void initSkyboxVertices(); 


public:
    EIGEN_MAKE_ALIGNED_OPERATOR_NEW
};


