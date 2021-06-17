#pragma once

#include <string.h>
#include <iostream>
#include <nanogui/screen.h>

#include <GLWrap/Program.hpp>
#include <GLWrap/Mesh.hpp>


#include <RTUtil/Camera.hpp>
#include <RTUtil/CameraController.hpp>
#include <RTUtil/sceneinfo.hpp>
using namespace RTUtil;

#include <../ext/assimp/include/assimp/Importer.hpp>
#include <../ext/assimp/include/assimp/scene.h>
#include <../ext/assimp/include/assimp/light.h>
#include <../ext/assimp/include/assimp/anim.h>

#include "Node.hpp"
#include "Animation.hpp"

class Scene {
public:
    int windowWidth;
    int windowHeight;
    RTUtil::SceneInfo sceneInfo;
    std::shared_ptr<RTUtil::PerspectiveCamera> camera;
    std::shared_ptr<RTUtil::PerspectiveCamera> defaultCamera;
    Node* rootNode;
    int numLights;
    int mNumAnimations;
    Animation* mAnimation;


    Scene(int wWidth, int wHeight);
    void getSceneInfo(const std::string info_file);
    void readInptFile(const std::string input_file);
    void importNodeInfo(const std::string input_file);
    std::shared_ptr<RTUtil::LightInfo> getDefaultLight();
    std::shared_ptr<RTUtil::LightInfo> getFirstPointLight();
    aiMatrix4x4 getLightTransformation(std::string name);
    std::shared_ptr<RTUtil::PerspectiveCamera> getDefaultCamera();
    std::shared_ptr<RTUtil::PerspectiveCamera> getBuiltInCamera(aiCamera* aiC, aiMatrix4x4 transformation);
    static void printTransformation(std::string name, aiMatrix4x4 t);
    static Eigen::Vector3f transformVector(aiMatrix4x4 t, Eigen::Vector3f v);
};
