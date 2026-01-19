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
    subMeshes.clear();
    materialDiffuseTex.clear();
    textures.clear();

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
    vertices.clear();
    indices.clear();
    textures.clear();
    subMeshes.clear();
    materialDiffuseTex.clear();
    
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

    // Build directory for textures
    std::string dir = "";
    size_t last_slash_idx = path.find_last_of("/\\");
    if (last_slash_idx != std::string::npos)
        dir = path.substr(0, last_slash_idx);
    
    
    Vertex v;
    // ---- Load geometry from ALL Assimp meshes and record the index ranges (subMesh)
    for (unsigned int i = 0; i < scene->mNumMeshes; i++)
    {
        aiMesh* mesh = scene->mMeshes[i];
        if (!mesh) continue;
        
        unsigned int baseVertex = (unsigned int)vertices.size();

        // record where this mesh's indices start + its material
        SubMesh part;
        part.indexOffset = (unsigned int)indices.size();
        part.materialIndex = (int)mesh->mMaterialIndex;

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
        // finish the part index count and store it
        part.indexCount = (unsigned int)indices.size() - part.indexOffset;
        subMeshes.push_back(part);
    }
    
    
	// OLD CODE : only load diffuse texture from the first material that has one
    // Load diffuse texture from the first material that has one
    /*std::string dir = "";
    size_t last_slash_idx = path.find_last_of("/\\");
    if (last_slash_idx != std::string::npos)
        dir = path.substr(0, last_slash_idx);*/

    // Load ONE diffuse texture per material (materialIndex -> textureID)
    // OLD CODE
    //for (unsigned int m = 0; m < scene->mNumMeshes; m++)
    //{
    //    aiMesh* m = scene->mMeshes[mi];
    //    if (!m) continue;

    //    if (m->mMaterialIndex >= scene->mNumMaterials) continue;

    //    aiMaterial* material = scene->mMaterials[m->mMaterialIndex];
    //    if (!material) continue;

    //    if (material->GetTextureCount(aiTextureType_DIFFUSE) > 0)
    //    {
    //        std::vector<Texture> diffuseMaps =
    //            loadMaterialTextures(material, aiTextureType_DIFFUSE, "texture_diffuse", dir);

    //        textures.insert(textures.end(), diffuseMaps.begin(), diffuseMaps.end());
    //        break;
    //    }
    //}
    materialDiffuseTex.assign(scene->mNumMaterials, 0);
    for (unsigned int m = 0; m < scene->mNumMaterials; m++)
    {
        aiMaterial* mat = scene->mMaterials[m];
        if (!mat) continue;

        // If this material has a diffuse texture, load the FIRST one (index 0)
        if (mat->GetTextureCount(aiTextureType_DIFFUSE) > 0)
        {
            aiString str;
            mat->GetTexture(aiTextureType_DIFFUSE, 0, &str);

            unsigned int texId = loadTextureAndBind(str.C_Str(), dir);
            materialDiffuseTex[m] = texId;

            // >>> CHANGED/ADDED <<<
            // optional: keep a record in textures[] (useful for debugging/logging)
            if (texId != 0)
            {
                Texture t;
                t.id = texId;
                t.type = "texture_diffuse";
                textures.push_back(t);
            }
        }
    }

    std::cout << "numVertex: " << vertices.size() << std::endl;
    std::cout << "numIndex: " << indices.size() << std::endl;
    std::cout << "numSubMeshes: " << subMeshes.size() << std::endl; 
    std::cout << "numMaterials: " << materialDiffuseTex.size() << std::endl; 
    std::cout << "numTextures (debug list): " << textures.size() << std::endl;
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

    // set buffer data to triangle vertex and setting vertex attributes
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex), &vertices[0] /*vertices.data()*/, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), 0);
    // set normal attributes
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void *) (sizeof(float) * 3));
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
std::vector<Texture> Mesh::loadMaterialTextures(aiMaterial *mat, aiTextureType type, std::string typeName, std::string dir)
{
    std::vector<Texture> textures;
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
//Material Mesh::loadMaterial(aiMaterial* mat) 
//{
//    Material material;
//    aiColor3D color(0.f, 0.f, 0.f);
//    float shininess;
//
//    mat->Get(AI_MATKEY_COLOR_DIFFUSE, color);
//    material.Diffuse = glm::vec3(color.r, color.b, color.g);
//
//    mat->Get(AI_MATKEY_COLOR_AMBIENT, color);
//    material.Ambient = glm::vec3(color.r, color.b, color.g);
//
//    mat->Get(AI_MATKEY_COLOR_SPECULAR, color);
//    material.Specular = glm::vec3(color.r, color.b, color.g);
//
//    mat->Get(AI_MATKEY_SHININESS, shininess);
//    material.Shininess = shininess;
//
//    return material;
//}
void Mesh::draw(glm::mat4 matModel, glm::mat4 matView, glm::mat4 matProj)
{
    // 1. Bind the correct shader program
    glUseProgram(shaderId);    

    // model  
    GLuint model_loc = glGetUniformLocation(shaderId, "model" );
    glUniformMatrix4fv(model_loc, 1, GL_FALSE, &matModel[0][0]);
   
    // view
    GLuint view_loc = glGetUniformLocation(shaderId, "view" );
    glUniformMatrix4fv(view_loc, 1, GL_FALSE, &matView[0][0]);
    
    // projection
    glm::mat4 mat_projection = matProj;
    GLuint projection_loc = glGetUniformLocation( shaderId, "projection" );
    glUniformMatrix4fv(projection_loc, 1, GL_FALSE, &mat_projection[0][0]);
    
    // Texture mapping (unit 0)
    GLint loc = glGetUniformLocation(shaderId, "diffuseMap");
    if (loc >= 0) glUniform1i(loc, 0);
    loc = glGetUniformLocation(shaderId, "textureMap");
    if (loc >= 0) glUniform1i(loc, 0);
    
    // picked flag
    glUniform1i(glGetUniformLocation(shaderId, "bPicked"), bPicked);

    // bind VAO
    glBindVertexArray(buffers[0]);

	// OLD CODE: draw entire mesh in one call
    //glActiveTexture(GL_TEXTURE0);

    //if (!textures.empty() && textures[0].id != 0)
    //{
    //    glBindTexture(GL_TEXTURE_2D, textures[0].id);
    //}
    //else
    //{
    //    glBindTexture(GL_TEXTURE_2D, 0);
    //}
    //// Spatial Data Structures
    //glUniform1i(glGetUniformLocation(shaderId, "bPicked"), bPicked);
    //// 3. Bind the corresponding model's VAO
    //glBindVertexArray(buffers[0]);
    //// 4. Draw the model
    //glDrawElements(GL_TRIANGLES, indices.size(), GL_UNSIGNED_INT, 0);
    //// 5. Unset vertex buffer
    //glBindVertexArray(0);

    if (!subMeshes.empty())
    {
        for (const SubMesh& part : subMeshes)
        {
            glActiveTexture(GL_TEXTURE0);

            unsigned int texId = 0;
            if (part.materialIndex >= 0 && part.materialIndex < (int)materialDiffuseTex.size())
                texId = materialDiffuseTex[part.materialIndex];

            glBindTexture(GL_TEXTURE_2D, texId);

            glDrawElements(
                GL_TRIANGLES,
                part.indexCount,
                GL_UNSIGNED_INT,
                (void*)(part.indexOffset * sizeof(unsigned int))
            );
        }
    }
    else
    {
        // Fallback: procedural meshes etc.
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, 0);
        glDrawElements(GL_TRIANGLES, (GLsizei)indices.size(), GL_UNSIGNED_INT, 0);
    }

    glBindVertexArray(0);
}