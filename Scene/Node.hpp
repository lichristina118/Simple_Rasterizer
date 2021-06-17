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

const int MAX_BONES_PER_VERTEX = 4;

struct VertexBoneData {        
    uint mIDs[MAX_BONES_PER_VERTEX];
    float mWeights[MAX_BONES_PER_VERTEX];
    VertexBoneData() {
        for (int i = 0; i < MAX_BONES_PER_VERTEX; i++) {
            mIDs[i] = -1;
            mWeights[i] = 0.0;  
        }
    }
};

class Node {
public:
    aiString     mName;
    aiMatrix4x4  mTransformation;
    Node*        mParent;
    unsigned int mNumChildren;
    Node**       mChildren;
    unsigned int mNumMeshes;
    aiMesh**     mMeshes;
    std::shared_ptr<nori::BSDF>* mMaterials;

    // to be passed to the shaders
    VertexBoneData*** mVertexBoneData;
    Eigen::Matrix<float, 3, Eigen::Dynamic>** mNormals;
    Eigen::Matrix<float, 3, Eigen::Dynamic>** mVertices;
    Eigen::VectorXi** mIndices;
    Eigen::Matrix<int, 4, Eigen::Dynamic>** mBoneIDs;
    Eigen::Matrix<float, 4, Eigen::Dynamic>** mBoneWeights;

    Node* findNode(aiString nodeName);
    void copyNodes(aiNode* in, aiMesh** meshes, RTUtil::SceneInfo sceneInfo);
    static void copyMesh(aiMesh* in, aiMesh* out);

    void loadNormalsForAll();
    void loadVerticesForAll();
    void loadIndicesForAll();
    void loadBonIDsForAll();
    void loadBonWeightsForAll();
    Eigen::VectorXi* getIndices(aiMesh* nodeMeshes);
    Eigen::Matrix<float, 3, Eigen::Dynamic>* getVertices(aiMesh* nodeMeshes);
    Eigen::Matrix<float, 3, Eigen::Dynamic>* getNormals(aiMesh* nodeMeshes);
    Eigen::Matrix<float, 3, Eigen::Dynamic> getTransformedVertices(aiMesh* nodeMeshes);
    Eigen::Matrix<int, 4, Eigen::Dynamic>* getBoneIDs(int meshIndex, aiMesh* nodeMesh);
    Eigen::Matrix<float, 4, Eigen::Dynamic>* getBoneWeights(int meshIndex, aiMesh* nodeMesh);

    static aiMatrix4x4 getTransformation(Node*, aiMatrix4x4 m);
    void loadVertexBones(int meshIndex, aiMesh* nodeMesh, std::map<std::string, int>& boneMapping);
};
