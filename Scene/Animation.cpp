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

#include "Animation.hpp"
#include "Scene.hpp"

//# define DEBUG 1

Animation::Animation(aiAnimation* anim) {
    mNumChannels = 0;
    mAnimationTime = 0.0;
    mNumBones = 0;

    mName = anim->mName;
    mDuration = anim->mDuration;
    mTicksPerSecond = anim->mTicksPerSecond;
    if (mTicksPerSecond == 0.0) {
        mTicksPerSecond = 25.0f;
    }

    mNumChannels = anim->mNumChannels;
    if (anim->mNumChannels > 0) {
        mChannels = new aiNodeAnim*[anim->mNumChannels];
        for (int i = 0; i < anim->mNumChannels; i++) {
            mChannels[i] = copyAiNodeAnimation(anim->mChannels[i]);
        }
    }

    mAnimationTime = 0.0;
    mNumBones = 0;
    mSpeed = 1.0;
    mLastFrame = 0.0;

    #ifdef DEBUG
        printf("animation: mDuration=%f, mTicksPerSecond=%f, mNumChannels=%d\n", mDuration, mTicksPerSecond, mNumChannels);
    #endif

    glfwSetTime(0.0);
}

Animation::Animation() {
    mNumChannels = 0;
    mAnimationTime = 0.0;
    mNumBones = 0;
    mSpeed = 1.0;
    mLastFrame = 0.0;
    glfwSetTime(0.0);
}

/*
 * Update animation time according to current time.
 */
void Animation::updateAnimationTime() {
    float currentFrame = glfwGetTime() * mSpeed;
    mAnimationTime += mTicksPerSecond * (currentFrame - mLastFrame);
    mAnimationTime = fmod(mAnimationTime, mDuration);
    mLastFrame = currentFrame;
}

/*
 * Restarts the animation with the given speed.
 */
void Animation::updateAnimationSpeed(float speed) {
    mSpeed = speed;
    mLastFrame = 0.0;
    glfwSetTime(0.0);
}


/*
 * Make a copy of the given aiAnimation object
 */
aiNodeAnim* Animation::copyAiNodeAnimation(aiNodeAnim* nodeAnim) {
    aiNodeAnim* aiNodeAnimOut = new aiNodeAnim();
    aiNodeAnimOut->mNodeName = nodeAnim->mNodeName;
    aiNodeAnimOut->mNumPositionKeys = nodeAnim->mNumPositionKeys;
    #ifdef DEBUG
        printf("animation: mNodeName=%s, mNumPositionKeys=%d\n", nodeAnim->mNodeName.C_Str(), nodeAnim->mNumPositionKeys);
    #endif

    if (nodeAnim->mNumPositionKeys > 0) {
        aiNodeAnimOut->mPositionKeys = new aiVectorKey[nodeAnim->mNumPositionKeys];
        for (int i = 0; i < nodeAnim->mNumPositionKeys; i++) {
            aiNodeAnimOut->mPositionKeys[i] = nodeAnim->mPositionKeys[i];
            #ifdef DEBUG
                printf("animation: mPositionKeys[%d]=(%f, (%f,%f,%f))\n", i, 
                        nodeAnim->mPositionKeys[i].mTime, nodeAnim->mPositionKeys[i].mValue.x, 
                        nodeAnim->mPositionKeys[i].mValue.y, nodeAnim->mPositionKeys[i].mValue.z);
            #endif
        }
    }

    #ifdef DEBUG
        printf("animation: mNodeName=%s, mNumRotationKeys=%d\n", nodeAnim->mNodeName.C_Str(), nodeAnim->mNumRotationKeys);
    #endif
    aiNodeAnimOut->mNumRotationKeys = nodeAnim->mNumRotationKeys;
    if (nodeAnim->mNumRotationKeys > 0) {
        aiNodeAnimOut->mRotationKeys = new aiQuatKey[nodeAnim->mNumRotationKeys];
        for (int i = 0; i < nodeAnim->mNumRotationKeys; i++) {
            aiNodeAnimOut->mRotationKeys[i] = nodeAnim->mRotationKeys[i];
            #ifdef DEBUG
                printf("animation: mRotationKeys[%d]=(%f, (%f, %f,%f,%f))\n", i, 
                        nodeAnim->mRotationKeys[i].mTime, nodeAnim->mRotationKeys[i].mValue.w, nodeAnim->mRotationKeys[i].mValue.x, 
                        nodeAnim->mRotationKeys[i].mValue.y, nodeAnim->mRotationKeys[i].mValue.z);
            #endif
        }
    }

    #ifdef DEBUG
        printf("animation: mNodeName=%s, mNumScalingKeys=%d\n", nodeAnim->mNodeName.C_Str(), nodeAnim->mNumScalingKeys);
    #endif
    aiNodeAnimOut->mNumScalingKeys = nodeAnim->mNumScalingKeys;
    if (nodeAnim->mNumScalingKeys > 0) {
        aiNodeAnimOut->mScalingKeys = new aiVectorKey[nodeAnim->mNumScalingKeys];
        for (int i = 0; i < nodeAnim->mNumScalingKeys; i++) {
            aiNodeAnimOut->mScalingKeys[i] = nodeAnim->mScalingKeys[i];
            #ifdef DEBUG
                printf("animation: mScalingKeys[%d]=(%f, (%f,%f,%f))\n", i, 
                        nodeAnim->mScalingKeys[i].mTime, nodeAnim->mScalingKeys[i].mValue.x, 
                        nodeAnim->mScalingKeys[i].mValue.y, nodeAnim->mScalingKeys[i].mValue.z);
            #endif
        }
    }

    aiNodeAnimOut->mPreState = nodeAnim->mPreState;
    aiNodeAnimOut->mPostState = nodeAnim->mPostState;

    return aiNodeAnimOut;
}

/*
 * find the aiNodeAnim object for the node
 */
aiNodeAnim* Animation::getNodeAnimation(aiString nodeName) {
    for (int i = 0; i < mNumChannels; i++) {
        if (mChannels[i]->mNodeName == nodeName) {
            return mChannels[i];
        }
    }

    return NULL;
}

/*
 * get node's global animation trasformation with animation
 */
aiMatrix4x4 Animation::getNodeGlobalAnimationTranformation(Node* node, aiMatrix4x4 m)
{
    if (node->mParent == NULL) {
        return m;
    } else {
        return getNodeGlobalAnimationTranformation(node->mParent, getNodeLocalAnimationTranformation(node->mParent) * m);
    }
}

/*
 * This function interpolates and generates the node (or bone) annimation transformation at the current time.
 * It returns the node's model transformation if there is no animation for this node (or bone).
*/
aiMatrix4x4 Animation::getNodeLocalAnimationTranformation(Node* node) {
    if (mChannels == 0) {
        return node->mTransformation;
    }

    // find the node animation object
    aiNodeAnim* nodeAnim = getNodeAnimation(node->mName);
    if (nodeAnim == NULL) {
        return node->mTransformation;
    }

    // get scaling transformation matrix
    aiVector3D scaling = getInterpolatedScaling(nodeAnim);
    aiMatrix4x4 s(scaling.x, 0.0f, 0.0f, 0.0f,
               0.0f, scaling.y, 0.0f, 0.0f,
               0.0f, 0.0f, scaling.z, 0.0f,
               0.0f, 0.0f, 0.0f, 1.0f);

    // get rotation transformation matrix
    aiQuaternion rotation = getInterpolatedRotation(nodeAnim);
    aiMatrix4x4 r = aiMatrix4x4(rotation.GetMatrix());

    // get translation transformation matrix
    aiVector3D translation = getInterpolatedPosition(nodeAnim);
    aiMatrix4x4 t(1.0f, 0.0f, 0.0f, translation.x,
               0.0f, 1.0f, 0.0f, translation.y,
               0.0f, 0.0f, 1.0f, translation.z,
               0.0f, 0.0f, 0.0f, 1.0f);

    return t * r * s;
}


/*
 * Recursively updates the bone global transformation for each bone for current
 * animation time
 */
void Animation::updateBoneTranformations(Node* node, aiMatrix4x4& parentT) {
    std::string nodeName = node->mName.C_Str();

    aiMatrix4x4 nodeT = getNodeLocalAnimationTranformation(node);

    aiMatrix4x4 tempT = parentT * nodeT;
    if (mBoneNameIds.find(nodeName) != mBoneNameIds.end()) {
        uint boneId = mBoneNameIds[nodeName];
        mBoneInfo[boneId].mTransformation = tempT * mBoneInfo[boneId].mOffsetMatrix;
    }

    for (int i = 0 ; i < node->mNumChildren ; i++) {
        updateBoneTranformations(node->mChildren[i], tempT);
    }
}

/*
 * gets the interpolated position transformation for the given
 * node animation frames at the current animation time.
 */
aiVector3D Animation::getInterpolatedPosition(aiNodeAnim* nodeAnim)
{
    // only one position key, take it
    if (nodeAnim->mNumPositionKeys == 1) {
        return nodeAnim->mPositionKeys[0].mValue;
    }
     
    // find the position key index       
    int index = 0;
    for (int i = 0 ; i < nodeAnim->mNumPositionKeys-1 ; i++) {
        if (mAnimationTime < (float)nodeAnim->mPositionKeys[i+1].mTime) {
            index = i;
            break;
        }
    }

    // mAnimationTime is in between these 2 keys
    aiVectorKey& p1 = nodeAnim->mPositionKeys[index];
    aiVectorKey& p2 = nodeAnim->mPositionKeys[index+1];

    // linear interpolation
    float timeFactor = (mAnimationTime - (float)p1.mTime) / (float)(p2.mTime - p1.mTime);
    return p1.mValue + timeFactor * (p2.mValue - p1.mValue);
}


/*
 * gets the interpolated scaling transformation for the given
 * node animation frames at the current animation time.
 */
aiVector3D Animation::getInterpolatedScaling(aiNodeAnim* nodeAnim)
{
    // only one scaling key, take it
    if (nodeAnim->mNumScalingKeys == 1) {
        return nodeAnim->mScalingKeys[0].mValue;
    }

     // find the scaling key index       
    int index = 0;
    for (int i = 0 ; i < nodeAnim->mNumScalingKeys-1 ; i++) {
        if (mAnimationTime < (float)nodeAnim->mScalingKeys[i+1].mTime) {
            index = i;
            break;
        }
    }

    // mAnimationTime is in between these 2 keys
    aiVectorKey& s1 = nodeAnim->mScalingKeys[index];
    aiVectorKey& s2 = nodeAnim->mScalingKeys[index+1];

    // linear interpolation
    float timeFactor = (mAnimationTime - (float)s1.mTime) / (float)(s2.mTime - s1.mTime);
    return s1.mValue + timeFactor * (s2.mValue - s1.mValue);
}

/*
 * gets the interpolated rotation transformation for the given
 * node animation frames at the current animation time.
 */
 aiQuaternion Animation::getInterpolatedRotation(aiNodeAnim* nodeAnim)
{
    // only one rotation key, take it
    if (nodeAnim->mNumRotationKeys == 1) {
        return nodeAnim->mRotationKeys[0].mValue;
    }
    
     // find the rotation key index       
    int index = 0;
    for (int i = 0 ; i < nodeAnim->mNumRotationKeys-1 ; i++) {
        if (mAnimationTime < (float)nodeAnim->mRotationKeys[i+1].mTime) {
            index = i;
            break;
        }
    }

    // mAnimationTime is in between these 2 keys
    aiQuatKey& r1 = nodeAnim->mRotationKeys[index];
    aiQuatKey& r2 = nodeAnim->mRotationKeys[index+1];

    //  interpolation
    float timeFactor = (mAnimationTime - (float)r1.mTime) / (float)(r2.mTime - r1.mTime);
    aiQuaternion rotation;
    aiQuaternion::Interpolate(rotation, r1.mValue, r2.mValue, timeFactor);
    return rotation.Normalize();
}

/*
 * Recursively reads the bone information from the nodes and meshes
 * and pupolates mNumBones, mBoneInfo and mBoneNameIds. 
 */
void Animation::loadBones(Node* node) {
    if (node->mNumMeshes > 0) {
        for (int m = 0; m < node->mNumMeshes; m++) {
            loadBones(node->mMeshes[m]);
        }
    }

    for (int i = 0; i < node->mNumChildren; i++) {
        loadBones(node->mChildren[i]);
    } 
}

/*
 * Reads the bone information for a given mesh structure
 * and pupolates mNumBones, mBoneInfo and mBoneNameIds. 
 */
 void Animation::loadBones(aiMesh* nodeMesh) {
    for (int b = 0; b < nodeMesh->mNumBones; b++) {
        int id = 0;
        std::string name = nodeMesh->mBones[b]->mName.C_Str();

        if (mBoneNameIds.find(name) == mBoneNameIds.end()) {
            id = mNumBones;
            mNumBones++;

            BoneInfo tmp;
            mBoneInfo.push_back(tmp);
        } else {
            id = mBoneNameIds[name];
        }

        mBoneNameIds[name] = id;
        mBoneInfo[id].mOffsetMatrix = nodeMesh->mBones[b]->mOffsetMatrix;
    }
}

void Animation::loadVertexBones(Node* node) {
    if (node->mNumMeshes > 0) {
        for (int m = 0; m < node->mNumMeshes; m++) {
            node->loadVertexBones(m, node->mMeshes[m], mBoneNameIds);
        }
    }
    for (int i = 0; i < node->mNumChildren; i++) {
        loadVertexBones(node->mChildren[i]);
    } 
}

void Animation::printDebugInfo() {
    #ifdef DEBUG
        printf("animation: mDuration=%f, mTicksPerSecond=%f, mNumChannels=%d, mNumBones=%d\n", mDuration, mTicksPerSecond, mNumChannels, mNumBones);
        for (int b = 0; b < mNumBones; b++) {
            Scene::printTransformation("mOffsetMatrix" + std::to_string(b), mBoneInfo[b].mOffsetMatrix);
        }
        for(std::map<std::string, int>::const_iterator it = mBoneNameIds.begin();
            it != mBoneNameIds.end(); ++it) {
            printf("boneName=%s, bonbID=%d\n", it->first.c_str(), it->second);
        }

    #endif 
}

