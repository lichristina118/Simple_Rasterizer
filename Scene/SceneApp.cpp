
#include <unistd.h>

#include <nanogui/window.h>
#include <nanogui/glcanvas.h>
#include <nanogui/combobox.h>
#include <nanogui/layout.h>

#include <cpplocate/cpplocate.h>

#include <../ext/assimp/include/assimp/Importer.hpp>
#include <../ext/assimp/include/assimp/postprocess.h>
#include <../ext/assimp/include/assimp/material.h>
#include <../ext/assimp/include/assimp/material.h>
#include <../ext/assimp/include/assimp/matrix4x4.h>

#include <RTUtil/conversions.hpp>
#include <RTUtil/microfacet.hpp>

#include <../ext/stb/stb_image.h>

#include "Node.hpp"
#include "Scene.hpp"
#include "SceneApp.hpp"

//# define DEBUG 1

const int SceneApp::windowHeight = 600;
const int SceneApp::windowWidth = 800;

const int shadowWidth = 1024;
const int shadowHeight = 1024;


// Constructor runs after nanogui is initialized and the OpenGL context is current.
SceneApp::SceneApp(std::string inputFile, std::string infoFile, std::string skyboxName)
: nanogui::Screen(Eigen::Vector2i(windowWidth, windowHeight), "Christina Li's Scene App", false),
  backgroundColor(0.4f, 0.4f, 0.7f, 1.0f) {

    mScene = new Scene(windowWidth, windowHeight);

    // get scene info from the info file
    mScene->getSceneInfo(infoFile);
    #ifdef DEBUG
        for (std::shared_ptr<RTUtil::LightInfo> light: mScene->sceneInfo.lights) {
            printf("Light: %d\n", light->type);
            printf("      power=(%f, %f, %f), center=(%f, %f, %f)\n ", light->power(0), light->power(1), light->power(2), light->position(0), light->position(1), light->position(2));
            printf("      normal=(%f, %f, %f), up=(%f, %f, %f)\n ", light->normal(0), light->normal(1), light->normal(2), light->up(0), light->up(1), light->up(2));
            printf("      size=(%f, %f)\n ", light->size(0), light->size(1));
            printf("      radiance=(%f, %f, %f), range=%f\n ", light->radiance(0), light->radiance(1), light->radiance(2), light->range);
        }
    #endif


    // get node hierarchy from the input .obj file and setup the camera
    mScene->readInptFile(inputFile);

    #ifdef DEBUG
        Scene::printTransformation("Root", mScene->rootNode->mTransformation);
    #endif

    mUseFlatShader = false;
    mShowBlur = false;
    mShowSunSky = false;
    mShowGBuffers = false;
    mShowShadowMapBuffer = false;
    if (mScene->mAnimation == NULL){
        mUseDefaultCamera = false;
        mDeferredRendering = true;
    } else {
        mUseDefaultCamera = true;
        mDeferredRendering = false;
    }
    mShowSkybox = true;
    mShowMirrorRflt = true;
    mSkyboxName = skyboxName;

    setCamera();
    setShaders();

    // create a framebuffer for G-Buffers in geometry pass
    Eigen::Vector2i size(windowWidth, windowHeight);
    gBuffer = std::make_shared<GLWrap::Framebuffer>(size, 5);

    // create a framebuffer for the shadow map in the shadow pass
    Eigen::Vector2i shadowMapSize(shadowWidth, shadowHeight);
    shadowMapBuffer = std::make_shared<GLWrap::Framebuffer>(shadowMapSize, 0);

    // create a framebuffer for accumulating lightings in the lighting pass
    std::vector<std::pair<GLenum, GLenum>> c_format;
    c_format.emplace_back(std::make_pair(GL_RGBA32F, GL_RGBA));               
    accumulationBuffer = std::make_shared<GLWrap::Framebuffer>(size, c_format);
    accumulationBuffer->colorTexture(0).generateMipmap();

    // create 2 framebuffers holding the intermediate results of the gaussion blur
    // between the vertical and horizontal pass.
    tempBuffer1 = std::make_shared<GLWrap::Framebuffer>(size, c_format);
    tempBuffer2 = std::make_shared<GLWrap::Framebuffer>(size, c_format);
    
    // create buffer for merge pass
    mergeBuffer = std::make_shared<GLWrap::Framebuffer>(size, c_format);

    // create buffer for skybox pass to draw on, it contains the depth texture
    std::pair<GLenum, GLenum> d_format=std::make_pair(GL_DEPTH_COMPONENT24, GL_DEPTH_COMPONENT);
    skyboxBuffer = std::make_shared<GLWrap::Framebuffer>(size, c_format, d_format);

    mSky = std::make_shared<RTUtil::Sky>(85.0*M_PI/180.0 , 7.0);

    // add a combo box to select the animation speed
    if (mScene->mAnimation != NULL) {
        nanogui::Window *window = new nanogui::Window(this, "Animation Speed");
        window->setPosition(Eigen::Vector2i(5,5));
        window->setLayout(new nanogui::GroupLayout());
        nanogui::ComboBox* c = new nanogui::ComboBox(window, { "x1", "x2", "x4", "x8", "x16", "x32", "x64"});
        c->setFixedSize(Eigen::Vector2i(100,20));
        c->setCallback([this](int index) {mScene->mAnimation->updateAnimationSpeed(pow(2.0, (index + 1)));});
    }

    // sky box
    if (mShowSkybox == true) {
        // load skybox files into a texture of cubmaps
        mSkyboxTextureID = loadSkyBox();
        initSkyboxVertices();
    }

    printConfig();

    // NanoGUI boilerplate
    performLayout();
    setVisible(true);
}

/*
 * Set the camera for the camera controller
 */
void SceneApp::setCamera() {
    if (mUseDefaultCamera == false && mScene->camera != NULL) {
        cc.reset(new RTUtil::DefaultCC(mScene->camera));
    } else {
        cc.reset(new RTUtil::DefaultCC(mScene->defaultCamera));        
    }
}

/*
 * Set up shaders for different passes.
 */
void SceneApp::setShaders() {
    const std::string resourcePath =
        cpplocate::locatePath("resources/Common", "", nullptr) + "resources/";

    if (mShowSkybox == true) {
        skyboxPassProg.reset(new GLWrap::Program("skyboxprogram", { 
            { GL_VERTEX_SHADER,   "../Scene/skybox.vs" },
            { GL_FRAGMENT_SHADER, "../Scene/skybox.fs" }
        }));
    }

    if (mDeferredRendering == false) {
        if (mUseFlatShader == true) {
            forwardRenderProg.reset(new GLWrap::Program("program", { 
                { GL_VERTEX_SHADER, "../Scene/min_mod.vs" },
                { GL_GEOMETRY_SHADER, resourcePath + "Common/shaders/flat.gs" },
                { GL_FRAGMENT_SHADER, resourcePath + "Common/shaders/lambert.fs"}
            }));        
        } else {
            forwardRenderProg.reset(new GLWrap::Program("forwardprogram", { 
                { GL_VERTEX_SHADER,   "../Scene/forwardrender.vs" },
                { GL_FRAGMENT_SHADER, resourcePath + "Common/shaders/microfacet.fs" },
                { GL_FRAGMENT_SHADER, "../Scene/forwardrender.fs" }
            }));
        }
    } else {
        geoPassProg.reset(new GLWrap::Program("geopassprogram", { 
            { GL_VERTEX_SHADER,   "../Scene/geopass.vs" },
            { GL_FRAGMENT_SHADER, "../Scene/geopass.fs" }
        }));

        shadowPassProg.reset(new GLWrap::Program("shadowpassprogram", { 
            { GL_VERTEX_SHADER,   "../Scene/shadowpass.vs" },
            { GL_FRAGMENT_SHADER, "../Scene/shadowpass.fs" }
        }));

        pointLightPassProg.reset(new GLWrap::Program("pointlightpassprogram", { 
            { GL_VERTEX_SHADER,   "../Scene/passthrough.vs" },
            { GL_FRAGMENT_SHADER, resourcePath + "Common/shaders/microfacet.fs" },
            { GL_FRAGMENT_SHADER, "../Scene/pointlightpass.fs" }
        }));

        ambientLightPassProg.reset(new GLWrap::Program("ambientlightpassprogram", { 
            { GL_VERTEX_SHADER,   "../Scene/passthrough.vs" },
            { GL_FRAGMENT_SHADER, "../Scene/ambientlightpass.fs" }
            //{ GL_FRAGMENT_SHADER, "../Scene/lightpass_diffuse.fs" }
        }));

        sunSkyPassProg.reset(new GLWrap::Program("sunskypassprogram", { 
            { GL_VERTEX_SHADER,   "../Scene/passthrough.vs" },
            { GL_FRAGMENT_SHADER, resourcePath + "Common/shaders/sunsky.fs" },
            { GL_FRAGMENT_SHADER, "../Scene/sunskypass.fs" }
        }));

        blurPassProg.reset(new GLWrap::Program("blurpassprogram", { 
            { GL_VERTEX_SHADER,   "../Scene/passthrough.vs" },
            { GL_FRAGMENT_SHADER, resourcePath + "Common/shaders/blur.fs" }
        }));

        mergePassProg.reset(new GLWrap::Program("mergepassprogram", {
            { GL_VERTEX_SHADER, "../Scene/passthrough.vs"}, 
            { GL_FRAGMENT_SHADER, "../Scene/mergepass.fs" }
        }));

        srgbPassProg.reset(new GLWrap::Program("srgbpassprogram", {
            { GL_VERTEX_SHADER, resourcePath + "Common/shaders/fsq.vert" }, 
            { GL_FRAGMENT_SHADER, resourcePath + "Common/shaders/srgb.frag" }
        }));

        skyboxRflctPassProg.reset(new GLWrap::Program("skyboxreflectionprogram", {
            { GL_VERTEX_SHADER, "../Scene/passthrough.vs"}, 
            { GL_FRAGMENT_SHADER, "../Scene/skyboxreflection.fs" }
        }));

    }
}

void SceneApp::drawContents() {
    GLWrap::checkGLError("drawContents start");
    glClearColor(0.0, 0.0, 0.0, 0.0);

    if (mDeferredRendering == false) {

        // draw object in the scene
        forwardRendering();

        // add skybox
        if (mShowSkybox == true) {
            skyboxPass();
        }
    } else {
        // create g-buffers
        geometryPass();
        if (mShowGBuffers == true) {
            displayGBuffers();
            return;
        }

        // clear the accumulation buffer
        accumulationBuffer->bind(0);
        unsigned int attachment[1] = { GL_COLOR_ATTACHMENT0};
        glDrawBuffers(1, attachment);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        //accumulationBuffer->unbind();

        // go through each light, render light effect
        for (std::shared_ptr<RTUtil::LightInfo> light: mScene->sceneInfo.lights) {
            if (light->type == Point) { // only handle the point light

                // transform light position to world space coornidate
                aiMatrix4x4 t = mScene->getLightTransformation(light->nodeName);
                aiVector3D lp(light->position(0), light->position(1), light->position(2));
                Eigen::Vector3f lp_world = RTUtil::a2e(t * lp);

                // create a camera at the light position
                std::shared_ptr<RTUtil::PerspectiveCamera> lightCam = 
                    std::make_shared<RTUtil::PerspectiveCamera>(
                    lp_world, // eye
                    Eigen::Vector3f(0.0, 0.0, 0.0), // target
                    Eigen::Vector3f(0.0, 1.0, 0.0), // up
                    (float)windowWidth / (float) windowHeight, // aspect
                    0.1, 20.0, // near, far
                    1.0 // fov
                );

                shadowPass(light, lightCam);
                pointLightingPass(light, lightCam);
            } else if (light->type == Ambient) {
                ambientLightingPass(light);
            }
        }

        if (mShowSkybox == true) {
            if (mShowMirrorRflt == true) {
                skyboxMirrorReflectionPass();
            }
            skyboxPass();
            displayFBuffer(skyboxBuffer);
        } else if (mShowSunSky){
            sunSkyPass();

            if (mShowBlur){
                blurPass();
                mergePass();
                displayFBuffer(mergeBuffer);
            }
        } else {
            displayFBuffer(accumulationBuffer);
        }
    }
}

/*
 * Geometry pass: render geometry from camera view, write surface info
 * to g-buffers.
 */
void SceneApp::geometryPass() {
    gBuffer->bind(0);
    unsigned int attachments[5] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2, GL_COLOR_ATTACHMENT3, GL_COLOR_ATTACHMENT4};
    glDrawBuffers(5, attachments);

    glViewport(0, 0, windowWidth, windowHeight);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_BLEND); 
    glDepthMask(GL_TRUE);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST); 
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_NONE);

    geoPassProg->use();

    // set camera uniforms
    setCameraUniforms(geoPassProg, false);

    // set up and draw meshes for each node
    drawMeshes(mScene->rootNode, geoPassProg, true);
    geoPassProg->unuse();

    glDisable(GL_DEPTH_TEST);
}

/*
 * Shadow pass: render geometry from light view,
 * producing a depth buffer (shadow map).
 * lightCam -- the camera from the light view
 */
void SceneApp::shadowPass(std::shared_ptr<RTUtil::LightInfo> light, std::shared_ptr<RTUtil::PerspectiveCamera> lightCam) {

    shadowPassProg->use();

    shadowMapBuffer->bind(0);

    glDepthMask(GL_TRUE);
    glEnable(GL_DEPTH_TEST);
    glClear(GL_DEPTH_BUFFER_BIT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST); 
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_NONE);

    glViewport(0, 0, shadowWidth, shadowHeight);

    shadowPassProg->uniform("mV", lightCam->getViewMatrix().matrix());
    shadowPassProg->uniform("mP", lightCam->getProjectionMatrix().matrix());
 
    drawMeshes(mScene->rootNode, shadowPassProg, false);

    shadowPassProg->unuse();
    glDisable(GL_DEPTH_TEST);
}

/*
 * Lighting pass for point light: render full screen quad. compute lighting 
 * effect for a point light and add to the accumulation buffer.
 * lightCam -- the camera from the light view.
 */
 void SceneApp::pointLightingPass(std::shared_ptr<RTUtil::LightInfo> light, std::shared_ptr<RTUtil::PerspectiveCamera> lightCam) {
    // bind accumulationBuffer for writing
    accumulationBuffer->bind(0);

    glViewport(0, 0, windowWidth, windowHeight);
    glEnable(GL_BLEND);
    glBlendEquation(GL_FUNC_ADD);
    glBlendFunc(GL_ONE, GL_ONE);

    // bind the G-Buffers for reading
    pointLightPassProg->use();
    glBindFramebuffer(GL_READ_FRAMEBUFFER, gBuffer->id());
    for (int i = 0; i < 5; i++) {
        gBuffer->colorTexture(i).bindToTextureUnit(i);
    }

    // bind the depth texture from the camera direction for reading
    gBuffer->depthTexture().bindToTextureUnit(5);

    // bind the depth map from the light direction for reading. (shadowmap)  
    shadowMapBuffer->depthTexture().bindToTextureUnit(6);

    pointLightPassProg->uniform("gNormal", 0);
    pointLightPassProg->uniform("gDiffuse_r", 1);
    pointLightPassProg->uniform("gAlpha", 2);
    pointLightPassProg->uniform("gConvert", 3);
    pointLightPassProg->uniform("gDepth", 5);
    pointLightPassProg->uniform("shadowMap", 6);

    // set window, camera and light related uniforms
    setWindowUniforms(pointLightPassProg);
    setCameraUniforms(pointLightPassProg, true);
    setLightUniforms(pointLightPassProg, light);
    pointLightPassProg->uniform("mV_l", lightCam->getViewMatrix().matrix());
    pointLightPassProg->uniform("mP_l", lightCam->getProjectionMatrix().matrix());

    /*
    printf("lightCam mV:\n");
    std::cout << lightCam->getViewMatrix().matrix() << std::endl;
    printf("lightCam mP:\n");
    std::cout << lightCam->getProjectionMatrix().matrix() << std::endl;
    */

    // go through each pixel, let the shader handle lighting
    renderQuad(pointLightPassProg);

    pointLightPassProg->unuse();
    glDisable(GL_BLEND);
}


/*
 * Lighting pass for ambient light: render full screen quad. compute lighting 
 * effect for an ambient light and add to the accumulation buffer.
 */
 void SceneApp::ambientLightingPass(std::shared_ptr<RTUtil::LightInfo> light) {
    // bind accumulationBuffer for writing
    accumulationBuffer->bind(0);

    glViewport(0, 0, windowWidth, windowHeight);
    glEnable(GL_BLEND);
    glBlendEquation(GL_FUNC_ADD);
    glBlendFunc(GL_ONE, GL_ONE);

    // bind the G-Buffers for reading
    ambientLightPassProg->use();
    glBindFramebuffer(GL_READ_FRAMEBUFFER, gBuffer->id());
    gBuffer->colorTexture(0).bindToTextureUnit(0); 
    ambientLightPassProg->uniform("gNormal", 0);
    gBuffer->colorTexture(1).bindToTextureUnit(1);
    ambientLightPassProg->uniform("gDiffuse_r", 1);
    gBuffer->depthTexture().bindToTextureUnit(5);
    ambientLightPassProg->uniform("gDepth", 5);

    // set window, camera and light related uniforms
    setWindowUniforms(ambientLightPassProg);
    setCameraUniforms(ambientLightPassProg, false);
    setLightUniforms(ambientLightPassProg, light);

    // go through each pixel, let the shader handle lighting
    renderQuad(ambientLightPassProg);

    ambientLightPassProg->unuse();
    glDisable(GL_BLEND);
}

/*
*  Lighting pass for sun sky model
*/
 void SceneApp::sunSkyPass() {
    // bind accumulationBuffer for writing
    accumulationBuffer->bind(0);

    glViewport(0, 0, windowWidth, windowHeight);
    glEnable(GL_BLEND);
    glBlendEquation(GL_FUNC_ADD);
    glBlendFunc(GL_ONE, GL_ONE);

    // bind the G-Buffers for reading
    sunSkyPassProg->use();
    glBindFramebuffer(GL_READ_FRAMEBUFFER, gBuffer->id());
    gBuffer->colorTexture(0).bindToTextureUnit(0); 
    sunSkyPassProg->uniform("gNormal", 0);
    gBuffer->depthTexture().bindToTextureUnit(5);
    sunSkyPassProg->uniform("gDepth", 5);

    // set window, camera and light related uniforms
    setWindowUniforms(sunSkyPassProg);
    setCameraUniforms(sunSkyPassProg, true);
    mSky->setUniforms(*sunSkyPassProg);

    renderQuad(sunSkyPassProg);

    sunSkyPassProg->unuse();
    glDisable(GL_BLEND);
}

/*
 * Blur pass: Use accumulationBuffer as input and apply the gaussian blur to it.
 * The result will be saved into tempBuffer2.
 */
void SceneApp::blurPass() {
    accumulationBuffer->colorTexture(0).generateMipmap();

    // bind tempBuffers for writing
    tempBuffer1->bind(0);
    unsigned int attachment[1] = {GL_COLOR_ATTACHMENT0};
    glDrawBuffers(1, attachment);
    tempBuffer1->colorTexture(0).generateMipmap();
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    tempBuffer1->unbind();

    tempBuffer2->bind(0);
    unsigned int attachment2[1] = {GL_COLOR_ATTACHMENT0};
    glDrawBuffers(1, attachment2);
    tempBuffer2->colorTexture(0).generateMipmap();
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    tempBuffer2->unbind();

    std::vector<float> blurStdev = {6.2, 24.9, 81.0, 263.0};
    std::vector<int> blurRadius = {24, 80, 243, 799};  

    glDisable(GL_BLEND); 
    glDisable(GL_DEPTH_TEST);

    for(int i = 0; i < 4; i++) {
        /****** begin horizontal blur pass ******/
        int mipMapLevel = i+1;
        int mipMapLevelWidth = windowWidth/pow(2, mipMapLevel);
        int mipMapLevelHeight = windowHeight/pow(2, mipMapLevel);

        blurPassProg->use();

        glViewport(0, 0, mipMapLevelWidth, mipMapLevelHeight);
        tempBuffer1->bind(mipMapLevel);
        
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);        
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);        
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

        // bind the accumulationBuffer for reading
        glBindFramebuffer(GL_READ_FRAMEBUFFER, accumulationBuffer->id());
        accumulationBuffer->colorTexture(0).bindToTextureUnit(0); 
        blurPassProg->uniform("image", 0);

        blurPassProg->uniform("dir", Eigen::Vector2f(1.0, 0.0));
        blurPassProg->uniform("stdev", blurStdev[i]);
        blurPassProg->uniform("radius", blurRadius[i]);
        blurPassProg->uniform("level", mipMapLevel);

        renderQuad(blurPassProg);
        blurPassProg->unuse();
        /****** end horizontal blur pass ******/

        /****** begin vertical blur pass ******/
        // bind tempBuffer2 for writing

        blurPassProg->use();

        glDisable(GL_BLEND);
        glDisable(GL_DEPTH_TEST);

        glViewport(0, 0, mipMapLevelWidth, mipMapLevelHeight);
        tempBuffer2->bind(mipMapLevel);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);        
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);        
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

        glBindFramebuffer(GL_READ_FRAMEBUFFER, tempBuffer1->id());
        tempBuffer1->colorTexture(0).bindToTextureUnit(0); 
        blurPassProg->uniform("image", 0);

        blurPassProg->uniform("dir", Eigen::Vector2f(0.0, 1.0));
        blurPassProg->uniform("stdev", blurStdev[i]);
        blurPassProg->uniform("radius", blurRadius[i]);
        blurPassProg->uniform("level", mipMapLevel);

        renderQuad(blurPassProg);
        blurPassProg->unuse(); 
        /*** end vertical blur pass ***/
    }

}

/*
* Merge Pass for blurred mipmaps 
*/
void SceneApp::mergePass() {
    mergePassProg->use();
    glViewport(0, 0, windowWidth, windowHeight);

    mergeBuffer->bind(0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    // bind tempBuffer2 for reading
    glBindFramebuffer(GL_READ_FRAMEBUFFER, tempBuffer2->id());
    tempBuffer2->colorTexture(0).bindToTextureUnit(0); 
    mergePassProg->uniform("image", 0);

    // bind original image for reading
    glBindFramebuffer(GL_READ_FRAMEBUFFER, accumulationBuffer->id());
    accumulationBuffer->colorTexture(0).bindToTextureUnit(1); 
    mergePassProg->uniform("originalImage", 1);

    renderQuad(mergePassProg);
    mergePassProg->unuse();
}

/*
 * Draw all the pixels on the window and for each pixel to go through
 * the lighting shaders.
 */
void SceneApp::renderQuad(std::unique_ptr<GLWrap::Program> &prog) {
    // Upload a two-triangle mesh for drawing a full screen quad
    Eigen::MatrixXf vertices(5, 4);
    vertices.col(0) << -1.0f, -1.0f, 0.0f, 0.0f, 0.0f;
    vertices.col(1) << 1.0f, -1.0f, 0.0f, 1.0f, 0.0f;
    vertices.col(2) << 1.0f, 1.0f, 0.0f, 1.0f, 1.0f;
    vertices.col(3) << -1.0f, 1.0f, 0.0f, 0.0f, 1.0f;

    Eigen::Matrix<float, 3, Eigen::Dynamic> positions = vertices.topRows<3>();
    Eigen::Matrix<float, 2, Eigen::Dynamic> texCoords = vertices.bottomRows<2>();

    fsqMesh.reset(new GLWrap::Mesh());

    fsqMesh->setAttribute(prog->getAttribLocation("vert_position"), positions);
    fsqMesh->setAttribute(prog->getAttribLocation("vert_texCoord"), texCoords);

    // Draw the full screen quad
    fsqMesh->drawArrays(GL_TRIANGLE_FAN, 0, 4);
}

/*
 * Use forward rendering (vs. deferred rendering/lighting) to render the image.
 */
void SceneApp::forwardRendering() {
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    glClearColor(0.0, 0.0, 0.0, 0.0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);

    forwardRenderProg->use();

    if (mUseFlatShader == true) { 
        // set uniforms for the flat shader
        forwardRenderProg->uniform("k_a", Eigen::Vector3f(0.1, 0.1, 0.1));
        forwardRenderProg->uniform("k_d", Eigen::Vector3f(0.9, 0.9, 0.9));
        forwardRenderProg->uniform("lightDir", Eigen::Vector3f(1.0, 1.0, 1.0).normalized());
        setCameraUniforms(forwardRenderProg, false);
    } else { 
        // for non-flat shader
        // set light, camera and window related uniforms
        std::shared_ptr<RTUtil::LightInfo> pointLight = mScene->getFirstPointLight();
        setLightUniforms(forwardRenderProg, pointLight);
        setWindowUniforms(forwardRenderProg);
        setCameraUniforms(forwardRenderProg, true);         

        // pass the skybox textures to the shader for mirror reflection
        if (mShowSkybox == true && mShowMirrorRflt == true) {
            glBindTexture(GL_TEXTURE_CUBE_MAP, mSkyboxTextureID);
            glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

            forwardRenderProg->uniform("skyboxReflection", 1);
        } else {
            forwardRenderProg->uniform("skyboxReflection", 0);
        }
    }

    // for animation
    if (mScene->mAnimation != NULL) {
        //update animation time
        mScene->mAnimation->updateAnimationTime();
        // update bone global transformation for each bone at current time
        aiMatrix4x4 tempT = aiMatrix4x4();
        mScene->mAnimation->updateBoneTranformations(mScene->rootNode, tempT);
    }

    // set up and draw meshes for each node, 
    if (mUseFlatShader == true) {
        drawMeshes(mScene->rootNode, forwardRenderProg, false);
    } else {
        if (mShowSkybox == true && mShowMirrorRflt == true) {
            // no need to setup the material related uniforms when showing the skybox mirror reflection
            drawMeshes(mScene->rootNode, forwardRenderProg, false);
        } else {
            drawMeshes(mScene->rootNode, forwardRenderProg, true);
        }
    }

    forwardRenderProg->unuse();
    //usleep(100);
}

/*
 * Display the first 4 textures in g-buffers.
 */
void SceneApp::displayGBuffers() {
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glViewport(0, 0, windowWidth, windowHeight);

    glBindFramebuffer(GL_READ_FRAMEBUFFER, gBuffer->id());
    glReadBuffer(GL_COLOR_ATTACHMENT0);
    glBlitFramebuffer(0, 0, windowWidth, windowHeight, 0, windowHeight/2, windowWidth/2, windowHeight, GL_COLOR_BUFFER_BIT, GL_LINEAR);

    glReadBuffer(GL_COLOR_ATTACHMENT1);
    glBlitFramebuffer(0, 0, windowWidth, windowHeight, windowWidth/2, windowHeight/2, windowWidth, windowHeight, GL_COLOR_BUFFER_BIT, GL_LINEAR);

    glReadBuffer(GL_COLOR_ATTACHMENT2);
    glBlitFramebuffer(0, 0, windowWidth, windowHeight, 0, 0, windowWidth/2, windowHeight/2, GL_COLOR_BUFFER_BIT, GL_LINEAR);

    glReadBuffer(GL_COLOR_ATTACHMENT3);
    glBlitFramebuffer(0, 0, windowWidth, windowHeight, windowWidth/2, 0, windowWidth, windowHeight/2, GL_COLOR_BUFFER_BIT, GL_LINEAR);
}

/*
 * Color correction pass: convert accumution values to sRGB.
 * Display the converted accumulation buffer on screen.
 */
void SceneApp::displayFBuffer(std::shared_ptr<GLWrap::Framebuffer> buffer) {
    // switch to default framebuffer (window)
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);

    // set viewport to framebuffer size of the window to handle 
    // high resolution display problem
    int w,h;
    glfwGetFramebufferSize(glfwWindow(), &w, &h);
    glViewport(0, 0, w, h);
    //glViewport(0, 0, windowWidth, windowHeight);

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);    
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

    //display the texture from buffer
    glBindFramebuffer(GL_READ_FRAMEBUFFER, buffer->id());
    buffer->colorTexture(0).bindToTextureUnit(0);
    srgbPassProg->use();
    srgbPassProg->uniform("image", 0);
    srgbPassProg->uniform("exposure", 1.0f);
    renderQuad(srgbPassProg);
    srgbPassProg->unuse();
}

/*
 * Set window width and height uniforms.
 */
void SceneApp::setWindowUniforms(std::unique_ptr<GLWrap::Program> &prog) {
    prog->uniform("windowWidth", (float)windowWidth);
    prog->uniform("windowHeight", (float)windowHeight);
}

/*
 * Set camera releated unifroms, including transformation mV and mP. 
 * The camera eye position will be included if bEye is true.
 */
void SceneApp::setCameraUniforms(std::unique_ptr<GLWrap::Program> &prog, bool bEye) {
    // set camera related uniforms
    std::shared_ptr<RTUtil::PerspectiveCamera> c;
    if (mUseDefaultCamera == false && mScene->camera != NULL) {
        c = mScene->camera;
    } else {
        c = mScene->defaultCamera;   
    }
    prog->uniform("mV", c->getViewMatrix().matrix());
    prog->uniform("mP", c->getProjectionMatrix().matrix());
    
    if (bEye == true) {
        // camera attributes are already in world space
        prog->uniform("cameraEye", c->getEye());
    }
}

/*
 * Set light releated unifroms, including light center position, and light power.
 */
void SceneApp::setLightUniforms(std::unique_ptr<GLWrap::Program> &prog, std::shared_ptr<RTUtil::LightInfo> light) {
    if (light->type == Point) {
        // get light positon in world space for point light
        aiMatrix4x4 t = mScene->getLightTransformation(light->nodeName);
        aiVector3D lp(light->position(0), light->position(1), light->position(2));
        prog->uniform("lightPosition", RTUtil::a2e(t * lp));
        prog->uniform("lightPower", light->power);
    }

    if (light->type == Ambient) {
        prog->uniform("lightRadiance", light->radiance);
        prog->uniform("lightRange", light->range);
    }
}

/*
 * Recursively for each mesh:
 * 1. set vertices, indices, normals as openGL attributes,
 * 2. set uniform for transformation mM
 * 3. set the material related uniforms if bMat is true.
 * 4. draw the mesh.
 * 
 */
void SceneApp::drawMeshes(Node* node, std::unique_ptr<GLWrap::Program> &prog, bool bMat)
{
   if (node->mNumMeshes > 0) {
        aiMatrix4x4 t = Node::getTransformation(node, node->mTransformation);
        prog->uniform("mM", RTUtil::a2e(t).matrix());
        #ifdef DEBUG
            printf("node name=%s, mNumMeshes=%d\n", node->mName.C_Str(), node->mNumMeshes);
            Scene::printTransformation(node->mName.C_Str(), t);
        #endif

        // animation: forward rendering only
        if (mDeferredRendering == false){
            if (mScene->mAnimation != NULL){
                if (mScene->mAnimation->mNumBones > 0) {
                    // has animation and bone weight info
                    prog->uniform("hasAnimation", 1);

                    // set bone global transformation matrix
                    Animation* anim = mScene->mAnimation;
                    aiMatrix4x4 rootInverseT(mScene->rootNode->mTransformation);
                    rootInverseT.Inverse();

                    //printf("Current animation time: %f\n", mScene->mAnimation->mAnimationTime);

                    for (int b = 0; b < anim->mNumBones; b++) {
                        aiMatrix4x4 t_bone = rootInverseT * anim->mBoneInfo[b].mTransformation;

                        std::string name = "boneTransform[" + std::to_string(b) + "]";
                        prog->uniform(name, RTUtil::a2e(t_bone).matrix());

                        //Scene::printTransformation(name, t_bone);
                    }
                } else {
                    // has animation but no bone weight, like BoxAnimated.dae
                    prog->uniform("hasAnimation", 2);

                    // replace node model transformation with animation transformation
                    aiMatrix4x4 t_local = mScene->mAnimation->getNodeLocalAnimationTranformation(node);
                    aiMatrix4x4 t_global = mScene->mAnimation->getNodeGlobalAnimationTranformation(node, t_local);
                    prog->uniform("mM", RTUtil::a2e(t_global).matrix());
                }
            } else {
                // no animation
                prog->uniform("hasAnimation", 0);
            }
        }

        for (int m = 0; m < node->mNumMeshes; m++) {
            // init mesh
            mesh.reset(new GLWrap::Mesh());

            // set vertices
            mesh->setAttribute(0, *(node->mVertices[m]));

            // set normals
            if (node->mMeshes[m]->mNormals != NULL) {
                mesh->setAttribute(1, *(node->mNormals[m]));
            }

            // set indices
            mesh->setIndices(*(node->mIndices[m]), GL_TRIANGLES);

            if (!mDeferredRendering){
                if (mScene->mAnimation != NULL && mScene->mAnimation->mNumBones > 0) {
                    // set bone IDs and weights
                    mesh->setAttribute(2, *(node->mBoneIDs[m]));
                    mesh->setAttribute(3, *(node->mBoneWeights[m]));

                    //printf("Vertex Id = %d,\t Bone ID = (%d,%d,%d,%d),\t Bone Weights = (%f,%f,%f,%f)\n", m,
                    //    *(node->mBoneIDs[m])(0), node->mBoneIDs[m](1), node->mBoneIDs[m](2), node->mBoneIDs[m](3),                                node->mBoneWeights[m](0), node->mBoneWeights[m](1), node->mBoneWeights[m](2), node->mBoneWeights[m](3));
                }
            }

            // add material factor
            if (bMat == true) {
                if (node->mMaterials != NULL && node->mMaterials[m] != NULL) {
                    // set material related uniforms
                    std::shared_ptr<nori::Microfacet>  mat = std::dynamic_pointer_cast<nori::Microfacet>(node->mMaterials[m]);
                    nori::Color3f d = mat->diffuseReflectance();
                    Eigen::Vector3f disffuse_r(d.x(), d.y(), d.z());
                    prog->uniform("diffuse_r", disffuse_r);
                    prog->uniform("eta", mat->eta()); 
                    prog->uniform("alpha", mat->alpha());
                    prog->uniform("k_s", mat->k_s());
                    #ifdef DEBUG
                        printf("material for %s: eta=%f, alpha=%f, k_s=%f, diffuse_r=(%f, %f, %f)\n", node->mName.C_Str(), 
                            mat->eta(), mat->alpha(), mat->k_s(),
                            disffuse_r(0), disffuse_r(1), disffuse_r(2));     
                    #endif
                }
            }

            // draw it
            mesh->drawElements();
        }
    }

    for (int i = 0; i < node->mNumChildren; i++) {
        drawMeshes(node->mChildren[i], prog, bMat);
    } 
}

/*
 * It loads the skybox files into a texture of cubmaps.
 * The files are stored under ../Scene/skybox_files/ directory.
 * The file names are: 
 * name_rt.png, name_lf.png, name_up.png, name_dn.png, name_ft.png and name_bk.png
 * where 'name' is the name of the skybox (mSkyboxName).
 */
unsigned int SceneApp::loadSkyBox() {
    SkyboxFiles sb;
    std::string dir = "../Scene/skybox_files/";
    sb.mRight = dir + mSkyboxName + "_rt.png";
    sb.mLeft = dir + mSkyboxName + "_lf.png";
    sb.mTop = dir + mSkyboxName + "_up.png";
    sb.mBottom = dir + mSkyboxName + "_dn.png";
    sb.mFront = dir + mSkyboxName + "_ft.png";
    sb.mBack = dir + mSkyboxName + "_bk.png";

    unsigned int textureID;
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_CUBE_MAP, textureID);

    int width, height, channels;
    unsigned char *dataRight = stbi_load(sb.mRight.c_str(), &width, &height, &channels, 0);
    glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, dataRight);
    stbi_image_free(dataRight);
 
    unsigned char *dataLeft = stbi_load(sb.mLeft.c_str(), &width, &height, &channels, 0);
    glTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_X, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, dataLeft);
    stbi_image_free(dataLeft);

    unsigned char *dataTop = stbi_load(sb.mTop.c_str(), &width, &height, &channels, 0);
    glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Y, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, dataTop);
    stbi_image_free(dataTop);

    unsigned char *dataBottom = stbi_load(sb.mBottom.c_str(), &width, &height, &channels, 0);
    glTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Y, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, dataBottom);
    stbi_image_free(dataBottom);

    unsigned char *dataFront = stbi_load(sb.mFront.c_str(), &width, &height, &channels, 0);
    glTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Z, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, dataFront);
    stbi_image_free(dataFront);

    unsigned char *dataBack = stbi_load(sb.mBack.c_str(), &width, &height, &channels, 0);
    glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Z, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, dataBack);
    stbi_image_free(dataBack);

    if (dataRight == NULL || dataLeft == NULL || dataTop == NULL || dataBottom == NULL ||dataFront == NULL || dataBack == NULL) {
        printf("Failed to load skybox image files %s.\n", (dir + mSkyboxName + "*.png").c_str());
        exit(1);
    } else {
        printf("Loaded skybox image files.\n");
    }

    return textureID;
}

void SceneApp::initSkyboxVertices() {
    Eigen::MatrixXf vertices(3, 36);
    vertices.col(0) << -1.0f,  1.0f, -1.0f;
    vertices.col(1) << -1.0f, -1.0f, -1.0f;
    vertices.col(2) <<  1.0f, -1.0f, -1.0f;
    vertices.col(3) <<  1.0f, -1.0f, -1.0f;
    vertices.col(4) <<  1.0f,  1.0f, -1.0f;
    vertices.col(5) << -1.0f,  1.0f, -1.0f;

    vertices.col(6) << -1.0f, -1.0f,  1.0f;
    vertices.col(7) << -1.0f, -1.0f, -1.0f;
    vertices.col(8) << -1.0f,  1.0f, -1.0f;
    vertices.col(9) << -1.0f,  1.0f, -1.0f;
    vertices.col(10) << -1.0f,  1.0f,  1.0f;
    vertices.col(11) << -1.0f, -1.0f,  1.0f;

    vertices.col(12) <<  1.0f, -1.0f, -1.0f;
    vertices.col(13) <<  1.0f, -1.0f,  1.0f;
    vertices.col(14) <<  1.0f,  1.0f,  1.0f;
    vertices.col(15) <<  1.0f,  1.0f,  1.0f;
    vertices.col(16) <<  1.0f,  1.0f, -1.0f;
    vertices.col(17) <<  1.0f, -1.0f, -1.0f;

    vertices.col(18) << -1.0f, -1.0f,  1.0f;
    vertices.col(19) << -1.0f,  1.0f,  1.0f;
    vertices.col(20) <<  1.0f,  1.0f,  1.0f;
    vertices.col(21) <<  1.0f,  1.0f,  1.0f;
    vertices.col(22) <<  1.0f, -1.0f,  1.0f;
    vertices.col(23) << -1.0f, -1.0f,  1.0f;

    vertices.col(24) << -1.0f,  1.0f, -1.0f;
    vertices.col(25) <<  1.0f,  1.0f, -1.0f;
    vertices.col(26) <<  1.0f,  1.0f,  1.0f;
    vertices.col(27) <<  1.0f,  1.0f,  1.0f;
    vertices.col(28) << -1.0f,  1.0f,  1.0f;
    vertices.col(29) << -1.0f,  1.0f, -1.0f;

    vertices.col(30) << -1.0f, -1.0f, -1.0f;
    vertices.col(31) << -1.0f, -1.0f,  1.0f;
    vertices.col(32) <<  1.0f, -1.0f, -1.0f;
    vertices.col(33) <<  1.0f, -1.0f, -1.0f;
    vertices.col(34) << -1.0f, -1.0f,  1.0f;
    vertices.col(35) <<  1.0f, -1.0f,  1.0f;

    mSkyboxVertices = vertices.topRows<3>();
}

/*
 * It renders the skybox.
 */
void SceneApp::skyboxPass() {

    if (mDeferredRendering == false) {
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
        int w,h;
        glfwGetFramebufferSize(glfwWindow(), &w, &h);
        glViewport(0, 0, w, h);

    } else {
        // bind skyboxBuffer for writing
        skyboxBuffer->bind(0);

        glViewport(0, 0, windowWidth, windowHeight);
        
        // copy the depth texture from gBuffer to skyboxBuffer
        glBindFramebuffer(GL_READ_FRAMEBUFFER, gBuffer->id());
        glBlitFramebuffer(0, 0, windowWidth, windowHeight,
            0, 0, windowWidth, windowHeight, GL_DEPTH_BUFFER_BIT, GL_NEAREST);

        // copy the color texture from accumulationBuffer to skyboxBuffer
        glBindFramebuffer(GL_READ_FRAMEBUFFER, accumulationBuffer->id());
        glBlitFramebuffer(0, 0, windowWidth, windowHeight,
                        0, 0, windowWidth, windowHeight,
                        GL_COLOR_BUFFER_BIT, GL_NEAREST);

    }

    glDepthMask(GL_TRUE);
    glEnable(GL_DEPTH_TEST);

    // this makes the skybox to draw on the background pixels only.
    glDepthFunc(GL_LEQUAL); 


    skyboxPassProg->use();

    // set camera uniforms
    setCameraUniforms(skyboxPassProg, false);

    // bind skybox tectures for reading
    glBindTexture(GL_TEXTURE_CUBE_MAP, mSkyboxTextureID);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

    // draw skybox
    skyboxMesh.reset(new GLWrap::Mesh());
    skyboxMesh->setAttribute(skyboxPassProg->getAttribLocation("vert_position"), mSkyboxVertices);
    skyboxMesh->drawArrays(GL_TRIANGLES, 0, 36);
 
    skyboxPassProg->unuse();

    glDepthMask(GL_TRUE);
    glDepthFunc(GL_LESS); 
    glDisable(GL_BLEND);
}

/*
 *  It rensers the skybox mirror reflection on objects.
 *  It is called in the deferred rendering process only.
 *  The normals and depth g-buffers are used in the shaders. 
*/
 void SceneApp::skyboxMirrorReflectionPass() {
    // bind accumulationBuffer for writing
    accumulationBuffer->bind(0);

    glViewport(0, 0, windowWidth, windowHeight);
    glEnable(GL_BLEND);
    glBlendEquation(GL_FUNC_ADD);
    glBlendFunc(GL_ONE, GL_ONE);

    skyboxRflctPassProg->use();

    // camera uniforms
    setCameraUniforms(skyboxRflctPassProg, true);
    setWindowUniforms(skyboxRflctPassProg);

    // bind the G-Buffers for reading
    glBindFramebuffer(GL_READ_FRAMEBUFFER, gBuffer->id());
    gBuffer->colorTexture(0).bindToTextureUnit(1); 
    skyboxRflctPassProg->uniform("gNormal", 1);
    gBuffer->depthTexture().bindToTextureUnit(5);
    skyboxRflctPassProg->uniform("gDepth", 5);

    // bind skybox tectures for reading
    glBindTexture(GL_TEXTURE_CUBE_MAP, mSkyboxTextureID);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

    renderQuad(skyboxRflctPassProg);

    skyboxRflctPassProg->unuse();
    glDisable(GL_BLEND);
}


void SceneApp::printConfig() {
    printf("Configuration:\n");
    printf("\t%s\n", mUseDefaultCamera ? "default camera": "built-in camera");
    printf("\t%s\n", mDeferredRendering ? "deferred rendering": "forward rendering");
    printf("Usage:\n");
    printf("\tPress d to toggle between deferred and forward rendering.\n"); 
    printf("\tPress e to toggle between showing skybox or not.\n"); 
    printf("\tPress m to toggle between showing skybox mirror reflection or not.\n"); 
    printf("\tPress c to toggle between default camera and built-in camera.\n"); 
    printf("\tFor forward rendering, Press f to toggle between flat shader and non-flat shader.\n"); 
    printf("\tFor deferred rendering, Press s to toggle between displaying sun-sky and not.\n"); 
    printf("\tFor deferred rendering, Press g to toggle between displaying g-buffers and scene.\n"); 
    printf("\tFor deferred rendering, Press b to toggle between displaying blurred image and original image.\n"); 
}


bool SceneApp::keyboardEvent(int key, int scancode, int action, int modifiers) {

    if (Screen::keyboardEvent(key, scancode, action, modifiers))
       return true;

    // If the user presses the escape key...
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
        // ...exit the application.
        setVisible(false);
        return true;
    }

    // toggle the flat shader vs and shader with attribute normal passed in.
    // it is only work for the forward rendering case.
    if (key == GLFW_KEY_F && action == GLFW_PRESS) {
        if (mDeferredRendering == false) {
            mUseFlatShader = !(mUseFlatShader);
            setShaders();
            printConfig();
        }
    }

    // toggle between the default camera and the built-in camera
    if (key == GLFW_KEY_C && action == GLFW_PRESS) {
        mUseDefaultCamera = !(mUseDefaultCamera);
        setCamera();
        printConfig();
    }

    // toggle between showing the g-buffer and the normal deffered rendering path
    if (key == GLFW_KEY_G && action == GLFW_PRESS) {
        if (mDeferredRendering == true) {
            mShowGBuffers = !(mShowGBuffers);
            printConfig();
        }
    }

    if (key == GLFW_KEY_D && action == GLFW_PRESS) {
        mDeferredRendering = !(mDeferredRendering);
        setShaders();
        printConfig();
    }
    
    if (key == GLFW_KEY_S && action == GLFW_PRESS) {
        if (mDeferredRendering){
            mShowSunSky = !(mShowSunSky);
            printConfig();
        }
    }

    if (key == GLFW_KEY_B && action == GLFW_PRESS) {
        if (mDeferredRendering){
            mShowBlur = !(mShowBlur);
            printConfig();
        } 
    }

    if (key == GLFW_KEY_E && action == GLFW_PRESS) {
        mShowSkybox = !(mShowSkybox);
        if (mShowSkybox == true) {
            // load skybox files into a texture of cubmaps
            mSkyboxTextureID = loadSkyBox();
            initSkyboxVertices();
        }

        printConfig(); 
    }

    if (key == GLFW_KEY_M && action == GLFW_PRESS) {
        if (mShowSkybox == true) {
            mShowMirrorRflt = !(mShowMirrorRflt);
        }
        printConfig(); 
    }

    return cc->keyboardEvent(key, scancode, action, modifiers);
}

bool SceneApp::mouseButtonEvent(const Eigen::Vector2i &p, int button, bool down, int modifiers) {
    return Screen::mouseButtonEvent(p, button, down, modifiers) ||
           cc->mouseButtonEvent(p, button, down, modifiers);
}

bool SceneApp::mouseMotionEvent(const Eigen::Vector2i &p, const Eigen::Vector2i &rel, int button, int modifiers) {
    return Screen::mouseMotionEvent(p, rel, button, modifiers) ||
           cc->mouseMotionEvent(p, rel, button, modifiers);
}

bool SceneApp::scrollEvent(const Eigen::Vector2i &p, const Eigen::Vector2f &rel) {
    return Screen::scrollEvent(p, rel) ||
           cc->scrollEvent(p, rel);
}

