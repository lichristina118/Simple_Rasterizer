
#include <nanogui/window.h>
#include <nanogui/glcanvas.h>
#include <nanogui/layout.h>

#include <cpplocate/cpplocate.h>

#include <../ext/assimp/include/assimp/material.h>
#include <../ext/assimp/include/assimp/matrix4x4.h>

#include <RTUtil/conversions.hpp>

#include "Node.hpp"

//# define DEBUG 1

/*
 * Recursively go to all the children to find the node with given name.
 */
Node* Node::findNode(aiString nodeName) {
    if (mName == nodeName) {
        return this;
    } else {
        for (int i = 0; i < mNumChildren; i++) {
            Node* n = mChildren[i]->findNode(nodeName);
            if (n != NULL) {
                return n;
            }
        }
    }
    return NULL;
}

/*
 * Recursively copies the node hierachy. 
 */
void Node::copyNodes(aiNode* in, aiMesh** meshes, RTUtil::SceneInfo sceneInfo) {

    mName = aiString(in->mName);
    mTransformation = in->mTransformation;
    mNumChildren = in->mNumChildren;
    mNumMeshes = in->mNumMeshes;

    #ifdef DEBUG
        printf("Node Name: %s, Meshes: %d\n", mName.C_Str(), mNumMeshes);
    #endif

    // copy the meshes
    if (in->mNumMeshes > 0 && in->mMeshes != NULL) {
        mMeshes = new aiMesh*[in->mNumMeshes];
        mMaterials = new std::shared_ptr<nori::BSDF>[in->mNumMeshes];
        for (int i = 0; i < in->mNumMeshes; i++) {
            #ifdef DEBUG
                printf("  Mesh number: %d\n", in->mMeshes[i]);
            #endif
            mMeshes[i] = new(aiMesh);
            Node::copyMesh(meshes[in->mMeshes[i]], mMeshes[i]);

            // get material for this mesh from the scene info
            if (sceneInfo.nodeMaterials.find(mName.C_Str()) != sceneInfo.nodeMaterials.end()) {
                mMaterials[i] = sceneInfo.nodeMaterials[mName.C_Str()];;        
            } else {
                mMaterials[i] = sceneInfo.defaultMaterial;
            }
        }
    } else {
        mMeshes = NULL;
    }

    mVertexBoneData = new VertexBoneData**[mNumMeshes];

    // recursively copy children nodes
    if (in->mNumChildren > 0 && in->mChildren != NULL) {
        mChildren = new Node*[in->mNumChildren];
        for (int i = 0; i < in->mNumChildren; i++) {
            mChildren[i] = new Node();
            mChildren[i]->mParent = this;
            mChildren[i]->copyNodes(in->mChildren[i], meshes, sceneInfo);
        }
    } else {
        mChildren = NULL;
    }    
}

/*
 * copy the aiMesh
 */
 void Node::copyMesh(aiMesh* in, aiMesh* out) {

    out->mName = in->mName;
    out->mPrimitiveTypes = in->mPrimitiveTypes;
    out->mNumVertices = in->mNumVertices;
    out->mNumFaces = in->mNumFaces;

    if (in->mNumFaces > 0 && in->mFaces != NULL) {
        out->mFaces = new aiFace[in->mNumFaces];
        for (int i = 0; i < in->mNumFaces; i ++) {
           out->mFaces[i] = in->mFaces[i]; // aiFace has copy constructor
        }
    } else {
       out->mFaces = NULL; 
    }

    if (in->mNumVertices > 0 && in->mVertices != NULL) {
        out->mVertices = new aiVector3D[in->mNumVertices];
        for (int i = 0; i < in->mNumVertices; i ++) {
           out->mVertices[i] = in->mVertices[i]; 
        }
    } else {
       out->mVertices = NULL; 
    }

    if (in->mNumVertices > 0 && in->mNormals != NULL) {
        out->mNormals = new aiVector3D[in->mNumVertices];
        for (int i = 0; i < in->mNumVertices; i ++) {
           out->mNormals[i] = in->mNormals[i]; 
        }
    } else {
       out->mNormals = NULL; 
    }

    if (in->mNumVertices > 0 && in->mTangents != NULL) {
        out->mTangents = new aiVector3D[in->mNumVertices];
        for (int i = 0; i < in->mNumVertices; i ++) {
           out->mTangents[i] = in->mTangents[i]; 
        }
    } else {
       out->mTangents = NULL; 
    }

    if (in->mNumVertices > 0 && in->mBitangents != NULL) {
        out->mBitangents = new aiVector3D[in->mNumVertices];
        for (int i = 0; i < in->mNumVertices; i ++) {
           out->mBitangents[i] = in->mBitangents[i]; 
        }
    } else {
       out->mBitangents = NULL; 
    }

    // for animation
    out->mNumBones = in->mNumBones;
    if (in->mNumBones > 0) {
        out->mBones = new aiBone*[in->mNumBones];
        for (int i = 0; i < in->mNumBones; i++) {
            out->mBones[i] = new aiBone(*(in->mBones[i]));
        }
    } 
}

/*
 * Recursively loads the vertices for all the meshes
 * for this and all the dependent nodes.
 */
void Node::loadVerticesForAll() {
    if (mNumMeshes > 0) {
        mVertices = new Eigen::Matrix<float, 3, Eigen::Dynamic>*[mNumMeshes];
        for (int i = 0; i < mNumMeshes; i++) {
            mVertices[i] = getVertices(mMeshes[i]);
        }
    }

    for (int i = 0; i < mNumChildren; i++) {
        mChildren[i]->loadVerticesForAll();
    }
}

/*
 * get the vertices for the given node mesh
 */
Eigen::Matrix<float, 3, Eigen::Dynamic>*  Node::getVertices(aiMesh* nodeMeshes) {
    Eigen::Matrix<float, 3, Eigen::Dynamic>* positions = new Eigen::Matrix<float, 3, Eigen::Dynamic>(3,nodeMeshes->mNumVertices);
    int i = 0;
    for (int v = 0; v < nodeMeshes->mNumVertices; v++) {
        (*positions)(i) = nodeMeshes->mVertices[v].x;
        (*positions)(i + 1) = nodeMeshes->mVertices[v].y;
        (*positions)(i + 2) = nodeMeshes->mVertices[v].z;
        i += 3;
    }

    return positions;
}

/*
 * Recursively loads the normals for all the meshes
 * for this and all the dependent nodes.
 */
void Node::loadNormalsForAll() {
    if (mNumMeshes > 0) {
        mNormals = new Eigen::Matrix<float, 3, Eigen::Dynamic>*[mNumMeshes];
        for (int i = 0; i < mNumMeshes; i++) {
            mNormals[i] = getNormals(mMeshes[i]);
        }
    }

    for (int i = 0; i < mNumChildren; i++) {
        mChildren[i]->loadNormalsForAll();
    }
}

/*
 * get the normals for the given node mesh
 */
Eigen::Matrix<float, 3, Eigen::Dynamic>* Node::getNormals(aiMesh* nodeMeshes) {
    Eigen::Matrix<float, 3, Eigen::Dynamic>* normals = new Eigen::Matrix<float, 3, Eigen::Dynamic>(3,nodeMeshes->mNumVertices);
    int i = 0;
    for (int v = 0; v < nodeMeshes->mNumVertices; v++) {
        (*normals)(i) = nodeMeshes->mNormals[v].x;
        (*normals)(i + 1) = nodeMeshes->mNormals[v].y;
        (*normals)(i + 2) = nodeMeshes->mNormals[v].z;
        i += 3;
    }

    return normals;
}

/*
 * get the vertices for the given node mesh
 */
Eigen::Matrix<float, 3, Eigen::Dynamic>  Node::getTransformedVertices(aiMesh* nodeMeshes) {
    aiMatrix4x4 t = getTransformation(this, this->mTransformation);
    Eigen::Matrix<float, 3, Eigen::Dynamic> positions(3,nodeMeshes->mNumVertices);
    int i = 0;
    for (int v = 0; v < nodeMeshes->mNumVertices; v++) {
        aiVector3D aiV(nodeMeshes->mVertices[v].x, nodeMeshes->mVertices[v].y, nodeMeshes->mVertices[v].z);
        aiV = t * aiV;

        positions(i) = aiV.x;
        positions(i + 1) = aiV.y;
        positions(i + 2) = aiV.z;
        i += 3;
    }

    return positions;
}

/*
 * Recursively loads the indices for all the meshes
 * for this and all the dependent nodes.
 */
void Node::loadIndicesForAll() {
    if (mNumMeshes > 0) {
        mIndices = new Eigen::VectorXi*[mNumMeshes];
        for (int i = 0; i < mNumMeshes; i++) {
            mIndices[i] = getIndices(mMeshes[i]);
        }
    }

    for (int i = 0; i < mNumChildren; i++) {
        mChildren[i]->loadIndicesForAll();
    }
}


/*
 *  get indeices for the given node mesh
 */
Eigen::VectorXi* Node::getIndices(aiMesh* nodeMeshes) {

    Eigen::VectorXi* indices = new Eigen::VectorXi(3*nodeMeshes->mNumFaces);
    int i = 0;
    for (int f = 0; f < nodeMeshes->mNumFaces; f++) {
        (*indices)(i) = nodeMeshes->mFaces[f].mIndices[0];
        (*indices)(i + 1) = nodeMeshes->mFaces[f].mIndices[1];
        (*indices)(i + 2) = nodeMeshes->mFaces[f].mIndices[2];
        i += 3;
    }
    return indices;
}

/*
 * Get the accumulated transformation for the node. It traverses all the way
 * to the root.
 */ 
aiMatrix4x4 Node::getTransformation(Node* node, aiMatrix4x4 m)
{
    if (node->mParent == NULL) {
        return m;
    } else {
        return getTransformation(node->mParent, node->mParent->mTransformation * m);
    }
}


/*
 *  get boneIds and weights that influence the vertices for the given mesh.
 */
void Node::loadVertexBones(int meshIndex, aiMesh* nodeMesh, std::map<std::string, int> &boneMapping) {
    mVertexBoneData[meshIndex] = new VertexBoneData*[nodeMesh->mNumVertices];

    #ifdef DEBUG
        printf("meshIndex=%d, mNumBones=%d, mNumVertices=%d\n", meshIndex, nodeMesh->mNumBones, nodeMesh->mNumVertices);
    #endif

    for (int v = 0; v < nodeMesh->mNumVertices; v++) {
        int i = 0;
        mVertexBoneData[meshIndex][v] = new VertexBoneData();
        for (int b = 0; b < nodeMesh->mNumBones; b++) {
            for (int w =0; w < nodeMesh->mBones[b]->mNumWeights; w++) {
                //printf("vId=%d, weights=%f\n", nodeMesh->mBones[b]->mWeights[w].mVertexId, nodeMesh->mBones[b]->mWeights[w].mWeight);
                if (nodeMesh->mBones[b]->mWeights[w].mVertexId == v) {
                    int boneId = boneMapping[nodeMesh->mBones[b]->mName.C_Str()];
                    //printf("bondid=%d\n", boneId);
                    mVertexBoneData[meshIndex][v]->mIDs[i] = boneId;
                    mVertexBoneData[meshIndex][v]->mWeights[i] = nodeMesh->mBones[b]->mWeights[w].mWeight;
                    i++;
                    break;
                }
            }
            if (i >= MAX_BONES_PER_VERTEX) {
                break;
            }
        }
    }
}

/*
 * Recursively loads the bon IDs for all the vertices
 * for this and all the dependent nodes.
 */
void Node::loadBonIDsForAll() {
    if (mNumMeshes > 0) {
        mBoneIDs = new Eigen::Matrix<int, 4, Eigen::Dynamic>*[mNumMeshes];
        for (int i = 0; i < mNumMeshes; i++) {
            mBoneIDs[i] = getBoneIDs(i, mMeshes[i]);
        }
    }

    for (int i = 0; i < mNumChildren; i++) {
        mChildren[i]->loadBonIDsForAll();
    }
}

/*
 * get the bone ids that incluence the vertices for the given mesh
 */
Eigen::Matrix<int, 4, Eigen::Dynamic>*  Node::getBoneIDs(int meshIndex, aiMesh* nodeMeshes) {
    Eigen::Matrix<int, 4, Eigen::Dynamic>* boneIDs = new Eigen::Matrix<int, 4, Eigen::Dynamic>(4,nodeMeshes->mNumVertices);
    int i = 0;
    for (int v = 0; v < nodeMeshes->mNumVertices; v++) {
        (*boneIDs)(i) = mVertexBoneData[meshIndex][v]->mIDs[0];
        (*boneIDs)(i + 1) = mVertexBoneData[meshIndex][v]->mIDs[1];
        (*boneIDs)(i + 2) = mVertexBoneData[meshIndex][v]->mIDs[2];
        (*boneIDs)(i + 3) = mVertexBoneData[meshIndex][v]->mIDs[3];
        i += 4;

        #ifdef DEBUG
            printf("Vertex id=%d, boneIds=(%d, %d, %d, %d)\n", v,
                mVertexBoneData[meshIndex][v]->mIDs[0], mVertexBoneData[meshIndex][v]->mIDs[1],
                mVertexBoneData[meshIndex][v]->mIDs[2], mVertexBoneData[meshIndex][v]->mIDs[3]);
        #endif
    }

    return boneIDs;
}

/*
 * Recursively loads the bon weightss for all the vertices
 * for this and all the dependent nodes.
 */
void Node::loadBonWeightsForAll() {
    if (mNumMeshes > 0) {
        mBoneWeights = new Eigen::Matrix<float, 4, Eigen::Dynamic>*[mNumMeshes];
        for (int i = 0; i < mNumMeshes; i++) {
            mBoneWeights[i] = getBoneWeights(i, mMeshes[i]);
        }
    }

    for (int i = 0; i < mNumChildren; i++) {
        mChildren[i]->loadBonWeightsForAll();
    }
}


/*
 * get the bone weights that incluence the vertices for the given mesh
 */
Eigen::Matrix<float, 4, Eigen::Dynamic>* Node::getBoneWeights(int meshIndex, aiMesh* nodeMeshes) {
    Eigen::Matrix<float, 4, Eigen::Dynamic>* boneWts = new Eigen::Matrix<float, 4, Eigen::Dynamic>(4,nodeMeshes->mNumVertices);
    int i = 0;
    for (int v = 0; v < nodeMeshes->mNumVertices; v++) {
        (*boneWts)(i) = mVertexBoneData[meshIndex][v]->mWeights[0];
        (*boneWts)(i + 1) = mVertexBoneData[meshIndex][v]->mWeights[1];
        (*boneWts)(i + 2) = mVertexBoneData[meshIndex][v]->mWeights[2];
        (*boneWts)(i + 3) = mVertexBoneData[meshIndex][v]->mWeights[3];
        i += 4;

        #ifdef DEBUG
            printf("Vertex id=%d, boneWeights=(%f, %f, %f, %f)\n", v,
                mVertexBoneData[meshIndex][v]->mWeights[0], mVertexBoneData[meshIndex][v]->mWeights[1],
                mVertexBoneData[meshIndex][v]->mWeights[2], mVertexBoneData[meshIndex][v]->mWeights[3]);
        #endif
    }

    return boneWts;
}