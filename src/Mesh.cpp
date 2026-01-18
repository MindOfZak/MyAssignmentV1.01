#include <iostream>

#include <glad/glad.h>

#include "Mesh.h"

#include <assimp/scene.h>
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>

#include "Grid.h"
#include "Octree.h"

Mesh::Mesh()
{

}

Mesh::~Mesh()
{

}

void Mesh::init(std::string path, GLuint id)
{
    shaderId = id;
    loadModel(path);
    initBuffer();
}

void Mesh::initFromData(const std::vector<Vertex>& verts,
                        const std::vector<unsigned int>& idx,
                        GLuint id)
{
    shaderId = id;
    vertices = verts;
    indices = idx;
    initBuffer();
}

void Mesh::initSpatial(bool useOctree, glm::mat4 mat)
{
    if (useOctree)
        pSpatial = std::make_unique<Octree>();
    else
        pSpatial = std::make_unique<Grid>(glm::ivec3(32));
    
    pSpatial->Build(vertices, indices, mat);
}

void Mesh::loadModel(std::string path)
{
    Assimp::Importer importer;
    const aiScene* scene = importer.ReadFile(
        path,
        aiProcess_JoinIdenticalVertices |
        aiProcess_FlipUVs |
        aiProcess_Triangulate
    );

    if (scene == NULL || scene->mRootNode == NULL)
    {
        std::cout << "load model failed: " << importer.GetErrorString() << std::endl;
        return;
    }
    std::cout << "load model successful" << std::endl;

    Vertex v;

    // ---- Load geometry from ALL sub-meshes safely ----
    for (unsigned int i = 0; i < scene->mNumMeshes; i++)
    {
        aiMesh* mesh = scene->mMeshes[i];
        if (!mesh) continue;

        unsigned int baseVertex = (unsigned int)vertices.size();

        // vertices
        for (unsigned int j = 0; j < mesh->mNumVertices; j++)
        {
            glm::vec3 pos;
            pos.x = mesh->mVertices[j].x;
            pos.y = mesh->mVertices[j].y;
            pos.z = mesh->mVertices[j].z;
            v.pos = pos;

            glm::vec3 normal(0.0f, 1.0f, 0.0f);
            if (mesh->HasNormals())
            {
                normal.x = mesh->mNormals[j].x;
                normal.y = mesh->mNormals[j].y;
                normal.z = mesh->mNormals[j].z;
            }
            v.normal = normal;

            // UVs
            if (mesh->mTextureCoords[0])
            {
                v.texCoord = glm::vec2(
                    mesh->mTextureCoords[0][j].x,
                    mesh->mTextureCoords[0][j].y
                );
            }
            else
            {
                v.texCoord = glm::vec2(0.0f, 0.0f);
            }

            vertices.push_back(v);
        }

        // indices (with baseVertex offset)
        for (unsigned int j = 0; j < mesh->mNumFaces; j++)
        {
            const aiFace& face = mesh->mFaces[j];

            // safety: only triangles
            if (face.mNumIndices != 3) continue;

            indices.push_back(baseVertex + face.mIndices[0]);
            indices.push_back(baseVertex + face.mIndices[1]);
            indices.push_back(baseVertex + face.mIndices[2]);
        }
    }

    // ---- Load diffuse texture from the first material that has one ----
    std::string dir = "";
    size_t last_slash_idx = path.find_last_of("/\\");
    if (last_slash_idx != std::string::npos)
        dir = path.substr(0, last_slash_idx);

    for (unsigned int mi = 0; mi < scene->mNumMeshes; mi++)
    {
        aiMesh* m = scene->mMeshes[mi];
        if (!m) continue;

        if (m->mMaterialIndex >= scene->mNumMaterials) continue;

        aiMaterial* material = scene->mMaterials[m->mMaterialIndex];
        if (!material) continue;

        if (material->GetTextureCount(aiTextureType_DIFFUSE) > 0)
        {
            std::vector<Texture> diffuseMaps =
                loadMaterialTextures(material, aiTextureType_DIFFUSE, "texture_diffuse", dir);

            textures.insert(textures.end(), diffuseMaps.begin(), diffuseMaps.end());
            break;
        }
    }

    std::cout << "numVertex: " << vertices.size() << std::endl;
    std::cout << "numIndex: " << indices.size() << std::endl;
    std::cout << "numTextures: " << textures.size() << std::endl;
}


void Mesh::initBuffer()
{
    // create vertex buffer
    GLuint vao;
    glGenVertexArrays(1, &vao);
    GLuint vertBufID;
    glGenBuffers(1, &vertBufID);
    glBindBuffer(GL_ARRAY_BUFFER, vertBufID);
    GLuint idxBufID;
    glGenBuffers(1, &idxBufID);
    
    // remember VAO
    glBindVertexArray(vao);
    buffers.push_back(vao);


    // changed in LabA07 
    // set buffer data to triangle vertex and setting vertex attributes
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex), &vertices[0] /*vertices.data()*/, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    //glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 6, 0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), 0);

    // set normal attributes
    glEnableVertexAttribArray(1);
    //glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 6, (void *) (sizeof(float) * 3));
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void *) (sizeof(float) * 3));

    // added in LabA07: Adding texture coord attribute
    // vertex texture coords
    glEnableVertexAttribArray(2);
    // the second parameter: 2 coordinates (tx, ty) per texture coord	
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, texCoord));

    // bind index buffer
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, idxBufID);

    // set buffer data for triangle index
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(GLuint), indices.data(), GL_STATIC_DRAW);

    glBindVertexArray(0);
}

void Mesh::setShaderId(GLuint sid) {
    shaderId = sid;
}

// added in LabA07
// =====================================================
std::vector<Texture> Mesh::loadMaterialTextures(aiMaterial *mat, aiTextureType type, std::string typeName, std::string dir)
{
    std::vector<Texture> textures;

    // actually, we only support one texture
    int nTex = mat->GetTextureCount(type);
    for(unsigned int i = 0; i < nTex ; i++)
    {
        aiString str;
        mat->GetTexture(type, i, &str);

        Texture texture;
        texture.id = loadTextureAndBind(str.C_Str(), dir);
        texture.type = typeName;
        if (texture.id > 0)
            textures.push_back(texture);
    }
    return textures;
}  

unsigned int Mesh::loadTextureAndBind(const char* path, const std::string& directory)
{
    std::string filename = std::string(path);
    filename = directory + '/' + filename;

    int width, height, nrComponents;
    unsigned char* data = stbi_load(filename.c_str(), &width, &height, &nrComponents, 0);
    if (! data)
    {
        std::cout << "Texture failed to load at path: " << path << std::endl;
        stbi_image_free(data);

        return 0;
    }

    unsigned int textureID;
    glGenTextures(1, &textureID);

    GLenum format;
    if (nrComponents == 1)
        format = GL_RED;
    else if (nrComponents == 3)
        format = GL_RGB;
    else if (nrComponents == 4)
        format = GL_RGBA;

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, textureID);
    glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
    // Basic texture mapping requirement (no mip-mapping).
    // If you later add mip-mapping as an advanced feature, re-enable mip generation
    // and use a mip filter.
    // glGenerateMipmap(GL_TEXTURE_2D);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    stbi_image_free(data);


    return textureID;
}

Material Mesh::loadMaterial(aiMaterial* mat) 
{
    Material material;
    aiColor3D color(0.f, 0.f, 0.f);
    float shininess;

    mat->Get(AI_MATKEY_COLOR_DIFFUSE, color);
    material.Diffuse = glm::vec3(color.r, color.b, color.g);

    mat->Get(AI_MATKEY_COLOR_AMBIENT, color);
    material.Ambient = glm::vec3(color.r, color.b, color.g);

    mat->Get(AI_MATKEY_COLOR_SPECULAR, color);
    material.Specular = glm::vec3(color.r, color.b, color.g);

    mat->Get(AI_MATKEY_SHININESS, shininess);
    material.Shininess = shininess;

    return material;
}

// =====================================================


// all drawings come here
void Mesh::draw(glm::mat4 matModel, glm::mat4 matView, glm::mat4 matProj)
{
    // 1. Bind the correct shader program
    glUseProgram(shaderId);

    //std::cout << "shader: " << shaderId << std::endl;

    // 2. Set the appropriate uniforms for each shader
    // set the modelling transform  
    GLuint model_loc = glGetUniformLocation(shaderId, "model" );
    glUniformMatrix4fv(model_loc, 1, GL_FALSE, &matModel[0][0]);

    // set view matrix
    GLuint view_loc = glGetUniformLocation(shaderId, "view" );
    glUniformMatrix4fv(view_loc, 1, GL_FALSE, &matView[0][0]);

    // set projection transforms
    glm::mat4 mat_projection = matProj;
    GLuint projection_loc = glGetUniformLocation( shaderId, "projection" );
    glUniformMatrix4fv(projection_loc, 1, GL_FALSE, &mat_projection[0][0]);

    // added in LabA07
    // =====================================================
    // Texture mapping (unit 0)
    GLint loc = glGetUniformLocation(shaderId, "diffuseMap");
    if (loc >= 0) glUniform1i(loc, 0);

    loc = glGetUniformLocation(shaderId, "textureMap");
    if (loc >= 0) glUniform1i(loc, 0);

    // Bind the mesh's first diffuse texture (if present)
    glActiveTexture(GL_TEXTURE0);

    if (!textures.empty() && textures[0].id != 0)
    {
        glBindTexture(GL_TEXTURE_2D, textures[0].id);
    }
    else
    {
        glBindTexture(GL_TEXTURE_2D, 0);
    }

    // =====================================================
    // LabA 11 Spatial Data Structures
    glUniform1i(glGetUniformLocation(shaderId, "bPicked"), bPicked);

    // 3. Bind the corresponding model's VAO
    glBindVertexArray(buffers[0]);

    // 4. Draw the model
    glDrawElements(GL_TRIANGLES, indices.size(), GL_UNSIGNED_INT, 0);

    // 5. Unset vertex buffer
    glBindVertexArray(0);
}