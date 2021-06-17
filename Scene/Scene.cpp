
#include <nanogui/window.h>
#include <nanogui/glcanvas.h>
#include <nanogui/layout.h>

#include <cpplocate/cpplocate.h>

#include <../ext/assimp/include/assimp/Importer.hpp>
#include <../ext/assimp/include/assimp/postprocess.h>
#include <../ext/assimp/include/assimp/material.h>
#include <../ext/assimp/include/assimp/material.h>
#include <../ext/assimp/include/assimp/matrix4x4.h>
#include <../ext/assimp/include/assimp/scene.h>

#include <RTUtil/conversions.hpp>
#include <RTUtil/Camera.hpp>

#include "Scene.hpp"

//# define DEBUG 1

Scene::Scene(int wWidth, int wHeight) {
    windowWidth = wWidth;
    windowHeight = wHeight;
}

void Scene::getSceneInfo(const std::string info_file) {
    // handle scene info file
    if (RTUtil::readSceneInfo(info_file, sceneInfo) == false) {
        printf("Failed to read the scene info file.\n");
        exit(1);
    }
}

/*
 * Read the obj or dae file and parse it.
 * It sets the root node and the camera for the scene.
 **/
void Scene::readInptFile(const std::string input_file) {
    // Create an instance of the Importer class
    Assimp::Importer importer;

    // And have it read the given file with some example postprocessing
    const aiScene* sceneImport = importer.ReadFile(input_file,
        aiProcess_Triangulate            |
        aiProcess_SortByPType            |
        aiProcess_GenNormals             |
        aiProcess_LimitBoneWeights       |
        aiProcess_RemoveComponent
        ); 

    if (sceneImport == NULL) {
        printf("The importer failed to open file: %s\n", input_file.c_str());
        exit(1);
    }

    rootNode = new Node();
    rootNode->mParent = NULL;
    // recursivly copy the nodes because sceneImport will be desctroyed
    // at the end of this function. 
    rootNode->copyNodes(sceneImport->mRootNode, sceneImport->mMeshes, sceneInfo);
    rootNode->loadNormalsForAll();
    rootNode->loadVerticesForAll();
    rootNode->loadIndicesForAll();

    // Use built-in camera if any
   if (sceneImport->HasCameras()) {
        Node* cameraNode = rootNode->findNode(sceneImport->mCameras[0]->mName);
        if (cameraNode == NULL) {
            printf("Failed to find the camera node\n");
            exit(1);
        }
        aiMatrix4x4 t = Node::getTransformation(cameraNode, cameraNode->mTransformation);
        aiCamera* c = sceneImport->mCameras[0];
        #ifdef DEBUG
            printf("Built-in number of Cameras: %d\n", sceneImport->mNumCameras); 
            printf("   mPosition=(%f, %f, %f)\n", c->mPosition.x, c->mPosition.y, c->mPosition.z);
            printf("   mUp=(%f, %f, %f)\n", c->mUp.x, c->mUp.y, c->mUp.z);
            printf("   mLookAt=(%f, %f, %f)\n", c->mLookAt.x, c->mLookAt.y, c->mLookAt.z);
            printf("   mHorizontalFOV, mAspect, mClipPlaneNear=(%f, %f, %f)\n", c->mHorizontalFOV, c->mAspect, c->mClipPlaneNear);
            printTransformation(cameraNode->mName.C_Str(), t);
        #endif
        camera = getBuiltInCamera(c, t);
    } else {
        camera = NULL;
    }
    // crete a default camera
    defaultCamera = getDefaultCamera();

    numLights = sceneImport->mNumLights;
    
    // initialization for animation
    mAnimation = NULL;
    mNumAnimations = sceneImport->mNumAnimations;

    if (mNumAnimations > 0) {
        mAnimation = new Animation(sceneImport->mAnimations[0]);
        mAnimation->loadBones(rootNode);
        mAnimation->loadVertexBones(rootNode);
        rootNode->loadBonIDsForAll();
        rootNode->loadBonWeightsForAll();
        mAnimation->printDebugInfo();
    }
} 

/*
 * Read the obj or dae file and parse it.
 * It sets the root node for the scene.
 **/
void Scene::importNodeInfo(const std::string input_file) {
    // Create an instance of the Importer class
    Assimp::Importer importer;
    // And have it read the given file with some example postprocessing
    const aiScene* sceneImport = importer.ReadFile(input_file,
        aiProcess_CalcTangentSpace       |
        aiProcess_Triangulate            |
        aiProcess_JoinIdenticalVertices  |
        aiProcess_GenNormals             |
        aiProcess_SortByPType);


    rootNode = new Node();
    rootNode->mParent = NULL;

    if (sceneImport == NULL) {
        printf("The importer failed to open file: %s\n", input_file.c_str());
        exit(1);
    }

    // recursivly copy the nodes because sceneImport will be desctroyed
    // at the end of this function. 
    rootNode->copyNodes(sceneImport->mRootNode, sceneImport->mMeshes, sceneInfo);
}

/*
 * Create a camera in a default position, respecting the aspect ratio of the window.
 */
std::shared_ptr<RTUtil::PerspectiveCamera> Scene::getDefaultCamera() {
    return std::make_shared<RTUtil::PerspectiveCamera>(
        Eigen::Vector3f(6,0,10), // eye
        Eigen::Vector3f(0,0,0), // target
        Eigen::Vector3f(0,1,0), // up
        (float)windowWidth / (float) windowHeight, // aspect
        0.1, 50.0, // near, far
        42.0 * M_PI/180 // fov
    );
}

/*
 * Convert the given aiCamera to RTUtil::PerspectiveCamera.
 * It uses the windows aspect ratio.
 */
std::shared_ptr<RTUtil::PerspectiveCamera> Scene::getBuiltInCamera(aiCamera* aiC, aiMatrix4x4 transformation) {
    Eigen::Vector3f eye = RTUtil::a2e(transformation * aiC->mPosition);

    Eigen::Vector3f vDir = transformVector(transformation, RTUtil::a2e(aiC->mLookAt));

    //get target: get the closest point to the origin alone the mLookAt direction
    float d = (eye - eye.dot(vDir)*vDir).norm();
    float x = sqrt(eye.norm() * eye.norm() - d*d);
    Eigen::Vector3f target = eye + x*vDir;
    if (x == 0.0) {
        target = eye + 1.0 * vDir;
    }

    #ifdef DEBUG
        printf("World view camara eye=(%f, %f, %f), target=(%f, %f, %f)\n", eye(0), eye(1), eye(2), target(0), target(1), target(2));
    #endif

    // return the camera
    return std::make_shared<RTUtil::PerspectiveCamera>(
        eye,
        target,
        //transformVector(transformation, RTUtil::a2e(aiC->mUp)),
        RTUtil::a2e(aiC->mUp),
        (float)windowWidth / (float) windowHeight,
        aiC->mClipPlaneNear,
        aiC->mClipPlaneFar,
        aiC->mHorizontalFOV
    );
}

/*
 * Transform a vector instead of point.
 * For vector transformation the 4th element should be set to 0 instead of 1.
 * Return a normalized vector
 */
Eigen::Vector3f Scene::transformVector(aiMatrix4x4 t, Eigen::Vector3f v) {
    Eigen::Vector3f v1;
    v1(0) = t.a1 * v(0) + t.a2 * v(1) + t.a3 * v(2);
    v1(1) = t.b1 * v(0) + t.b2 * v(1) + t.b3 * v(2);
    v1(2) = t.c1 * v(0) + t.c2 * v(1) + t.c3 * v(2);
    return v1.normalized();
}

/*
 * Create a default light source.
 */
std::shared_ptr<RTUtil::LightInfo> Scene::getDefaultLight() {
    std::shared_ptr<RTUtil::LightInfo> l = std::make_shared<RTUtil::LightInfo>();
    l->nodeName = "DefaultPointLight";
    l->type = Point;
    l->position = Eigen::Vector3f(3.0, 3.0, 3.0);
    l->power = Eigen::Vector3f(300.0, 300.0, 300.0);
    return l;
}

/*
 * Get the first point light from the scene info file.
 */
 std::shared_ptr<RTUtil::LightInfo> Scene::getFirstPointLight() {
    for (std::shared_ptr<RTUtil::LightInfo> light: sceneInfo.lights) {
        if (light->type == Point) {
            return light;
        }
    }
    // return a default point light if no point light found in the scene info file.
    return getDefaultLight();
}

/*
 * Get the transformation for the light with given name
 */
aiMatrix4x4 Scene::getLightTransformation(std::string name) {
    Node* lightNode = rootNode->findNode(aiString(name.c_str()));
    if (lightNode == NULL) {
        // cannot find the light node, this is the default light
        return aiMatrix4x4(1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,0);
    } else {
        return Node::getTransformation(lightNode, lightNode->mTransformation);
    }
}

void Scene::printTransformation(std::string name, aiMatrix4x4 t) {
    printf("Transformation for %s:\n(\t%f, %f, %f, %f\n\t%f, %f, %f, %f\n\t%f, %f, %f, %f\n\t%f, %f, %f, %f)\n",
        name.c_str(), t.a1, t.a2, t.a3, t.a4, t.b1, t.b2, t.b3, t.b4, t.c1, t.c2, t.c3, t.c4, t.d1, t.d2, t.d3, t.d4);
}

