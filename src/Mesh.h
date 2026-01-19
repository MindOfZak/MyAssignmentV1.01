#ifndef __MESH_H__
#define __MESH_H__

#include <iostream>
#include <vector>
#include <memory> // needed for std::unique_ptr

#include <glad/glad.h>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/transform.hpp>
#include <assimp/material.h>
#include "Spatial.h"


struct Texture {
    GLuint id;
    std::string type;
};


struct Material {
    glm::vec3 Diffuse;
    glm::vec3 Specular;
    glm::vec3 Ambient;
    float Shininess;
};

// ==============================================


class Mesh {

protected:
    // array of vertices and normals
     
    std::vector<Vertex> vertices;

    // triangle vertex indices
    std::vector< unsigned int > indices;

	// textures (only keeping for compatibility with model loading, but will not rely on textures[0])
    std::vector<Texture> textures;

    // Material material;
    std::vector<GLuint> buffers;

    // my shader program ID
    GLuint shaderId;

    // picking highlight boolean
    bool bPicked = false;
    
    void initBuffer();

    // texture helpers
    std::vector<Texture> loadMaterialTextures(aiMaterial *mat, aiTextureType type, std::string typeName, std::string dir);
    unsigned int loadTextureAndBind(const char* path, const std::string& directory);
    
    struct SubMesh
    {
        unsigned int indexOffset = 0;   // start in indices[]
        unsigned int indexCount = 0;   // how many indices for this part
        int materialIndex = -1;  // Assimp material index for this part
    };

    // List of parts to draw separately (one per Assimp mesh)
    std::vector<SubMesh> subMeshes;

    // materialIndex -> diffuse texture ID (0 if none)
    std::vector<unsigned int> materialDiffuseTex;

    // NOT USED
    //Material loadMaterial(aiMaterial* mat);

public:

    std::unique_ptr<Spatial> pSpatial = nullptr;

    Mesh();
    ~Mesh();

    void init(std::string path, GLuint shaderId);
    // Procedural mesh (e.g., generated grid floor)
    void initFromData(const std::vector<Vertex>& verts,
                      const std::vector<unsigned int>& idx,
                      GLuint shaderId);
    void loadModel(std::string path);

    void initSpatial(bool useOctree, glm::mat4 mat);

    void setShaderId(GLuint sid);

    // added in LabA 11
    void setPicked(bool b) { bPicked = b; }
    
    void draw(glm::mat4 matModel, glm::mat4 matView, glm::mat4 matProj);
};

#endif