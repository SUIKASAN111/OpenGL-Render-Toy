#pragma once

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <string>
#include <vector>
#include <iostream>

#include "editor_settings.h"
#include "texture.h"
#include "material.h"
#include "shader.h"
using namespace std;

#define MAX_BONE_INFLUENCE 4

struct Vertex
{
    // position
    glm::vec3 Position;
    // normal
    glm::vec3 Normal;
    // texCoords
    glm::vec2 TexCoords;
    // tangent
    glm::vec3 Tangent;
    // bitangent
    glm::vec3 Bitangent;
    // bone indexes which will influence this vertex
    int m_BoneIDs[MAX_BONE_INFLUENCE];
    // weights from each bone
    float m_Weights[MAX_BONE_INFLUENCE];
};

class Mesh
{
public:
    // mesh Data
    vector<Vertex> vertices;
    vector<unsigned int> indices;
    vector<Texture2D*> textures;
    unsigned int VAO;
    string name = "mesh";

    // constructor
    Mesh(vector<Vertex> vertices, vector<unsigned int> indices, vector<Texture2D*> textures)
    {
        this->vertices = vertices;
        this->indices = indices;
        this->textures = textures;
        // now that we have all the required data, set the vertex buffers and its attribute pointers.
        setupMesh();
    }

    // Render the mesh
    void Draw(Material* material)
    {
        // Use material shader
        material->Setup(textures);

        // Draw mesh
        glBindVertexArray(VAO);
        glDrawElements(GL_TRIANGLES, static_cast<unsigned int>(indices.size()), GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);

        // Always good practice to set everything back to defaults once configured.
        glActiveTexture(GL_TEXTURE0);
    }

    // Draw without material setting (use shader.use() to set render method)
    void Draw()
    {
        // Draw mesh
        glBindVertexArray(VAO);
        glDrawElements(GL_TRIANGLES, static_cast<unsigned int>(indices.size()), GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);

        // Always good practice to set everything back to defaults once configured.
        glActiveTexture(GL_TEXTURE0);
    }

private:
    // Render data
    unsigned int VBO, EBO;

    // Initializes all the buffer objects/arrays
    void setupMesh()
    {
        // Create buffers/arrays
        glGenVertexArrays(1, &VAO);
        glGenBuffers(1, &VBO);
        glGenBuffers(1, &EBO);

        glBindVertexArray(VAO);
        // Load data into vertex buffers
        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        // A great thing about structs is that their memory layout is sequential for all its items.
        // The effect is that we can simply pass a pointer to the struct and it translates perfectly to a glm::vec3/2 array which
        // Again translates to 3/2 floats which translates to a byte array.
        glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex), &vertices[0], GL_STATIC_DRAW);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), &indices[0], GL_STATIC_DRAW);

        // Set the vertex attribute pointers
        // vertex Positions
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0);
        // vertex normals
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, Normal));
        // vertex texture coords
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, TexCoords));
        // vertex tangent
        glEnableVertexAttribArray(3);
        glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, Tangent));
        // vertex bitangent
        glEnableVertexAttribArray(4);
        glVertexAttribPointer(4, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, Bitangent));
        // ids
        glEnableVertexAttribArray(5);
        glVertexAttribIPointer(5, 4, GL_INT, sizeof(Vertex), (void*)offsetof(Vertex, m_BoneIDs));

        // weights
        glEnableVertexAttribArray(6);
        glVertexAttribPointer(6, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, m_Weights));
        glBindVertexArray(0);
    }
};

class MeshRenderer
{
public:
    Material*   material;
    Mesh*       mesh;
    bool        cast_shadow = true;

public:
    MeshRenderer(Material* _material, Mesh* _mesh) : material(_material), mesh(_mesh) {}
    ~MeshRenderer()
    {
        if (material == nullptr) return;
        delete material;
        material = nullptr;
    }

    void SetMaterial(EMaterialType type)
    {
        delete material;
        material = MaterialManager::CreateMaterialByType(type);
    }

    void Draw()
    {
        if (material->IsValid() && mesh != nullptr)
        {
            switch (material->cullface)
            {
            case E_CULL_FACE::culloff:
                glDisable(GL_CULL_FACE);
                break;
            case E_CULL_FACE::cullfront:
                glEnable(GL_CULL_FACE);
                glCullFace(GL_FRONT);
                break;
            case E_CULL_FACE::cullback:
                glEnable(GL_CULL_FACE);
                glCullFace(GL_BACK);
                break;
            default:
                glEnable(GL_CULL_FACE);
                break;
            }

            if (EditorSettings::UsePolygonMode)
            {
                // draw mesh
                glBindVertexArray(mesh->VAO);
                glDrawElements(GL_TRIANGLES, static_cast<unsigned int>(mesh->indices.size()), GL_UNSIGNED_INT, 0);
                glBindVertexArray(0);

                // always good practice to set everything back to defaults once configured.
                glActiveTexture(GL_TEXTURE0);
            }
            else
            {
                //setTB();
                mesh->Draw(material);
            }
        }
    }

    void PureDraw()
    {
        if (mesh != nullptr)
        {
            mesh->Draw();
        }
    }

    void setTB()
    {
        auto vertices = mesh->vertices;
        glm::vec3 pos1 = vertices[0].Position, pos2 = vertices[1].Position, pos3 = vertices[2].Position;
        glm::vec2 uv1 = vertices[0].TexCoords, uv2 = vertices[1].TexCoords, uv3 = vertices[2].TexCoords;
        glm::vec3 edge1 = pos2 - pos1;
        glm::vec3 edge2 = pos3 - pos1;
        glm::vec2 deltaUV1 = uv2 - uv1;
        glm::vec2 deltaUV2 = uv3 - uv1;

        float f = 1.0f / (deltaUV1.x * deltaUV2.y - deltaUV2.x * deltaUV1.y);
        glm::vec3 tangent, bitangent;
        tangent.x = f * (deltaUV2.y * edge1.x - deltaUV1.y * edge2.x);
        tangent.y = f * (deltaUV2.y * edge1.y - deltaUV1.y * edge2.y);
        tangent.z = f * (deltaUV2.y * edge1.z - deltaUV1.y * edge2.z);
        bitangent.x = f * (-deltaUV2.x * edge1.x + deltaUV1.x * edge2.x);
        bitangent.y = f * (-deltaUV2.x * edge1.y + deltaUV1.x * edge2.y);
        bitangent.z = f * (-deltaUV2.x * edge1.z + deltaUV1.x * edge2.z);
        material->shader->setVec3("tangent", tangent);
		material->shader->setVec3("bitangent", bitangent);
    }
};