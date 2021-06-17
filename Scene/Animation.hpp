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

#include <../ext/assimp/include/assimp/scene.h>
#include <../ext/assimp/include/assimp/mesh.h>
#include <../ext/assimp/include/assimp/anim.h>

#include "Node.hpp"

struct BoneInfo {
    aiMatrix4x4 mOffsetMatrix;
    aiMatrix4x4 mTransformation;        

    BoneInfo(){
       // mOffsetMatrix = aiMatrix4x4(0.0);   
       // mLocalTransformation = aiMatrix4x4(0.0);   
    };
};

class Animation {
public:
    aiString mName;
    double mDuration;
    double mTicksPerSecond;
    unsigned int mNumChannels;
    aiNodeAnim **mChannels;
    float mAnimationTime;

    std::map<std::string, int> mBoneNameIds; 
    int mNumBones;
    std::vector<BoneInfo> mBoneInfo;
    float mSpeed;
    float mLastFrame;


    Animation(aiAnimation* anim);
    Animation();

    void updateAnimationTime();
    static aiNodeAnim* copyAiNodeAnimation(aiNodeAnim* nodeAnim);
    aiNodeAnim* getNodeAnimation(aiString nodeName);
    aiMatrix4x4 getNodeLocalAnimationTranformation(Node* node);
    aiMatrix4x4 getNodeGlobalAnimationTranformation(Node* node, aiMatrix4x4 m);
    aiVector3D getInterpolatedPosition(aiNodeAnim* nodeAnim);
    aiVector3D getInterpolatedScaling(aiNodeAnim* nodeAnim);
    aiQuaternion getInterpolatedRotation(aiNodeAnim* nodeAnim);
    void loadBones(Node* node);
    void loadBones(aiMesh* nodeMesh);
    void loadVertexBones(Node* node);
    void updateBoneTranformations(Node* node, aiMatrix4x4& parentT);
    void updateAnimationSpeed(float speed);
    void printDebugInfo();
};
