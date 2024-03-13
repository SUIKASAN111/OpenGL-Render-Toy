#include <string>

#include "renderer_console.h"
#include "material.h"
#include "texture.h"
#include "shader.h"
#include "file_system.h"

unsigned int Material::cur_id = 0;

Material::Material() { id = cur_id++; }
Material::~Material() { RendererConsole::GetInstance()->AddLog("delete Material"); }
PhongMaterial::~PhongMaterial() { RendererConsole::GetInstance()->AddLog("delete Phong Material"); }
BlinnPhongMaterial::~BlinnPhongMaterial() { RendererConsole::GetInstance()->AddLog("delete Blinn-Phong Material"); }
CTPBRMaterial::~CTPBRMaterial() { RendererConsole::GetInstance()->AddLog("delete Cook-Torrance Material"); }
bool Material::IsValid() { return shader != nullptr; }

void Material::SetTexture(Texture2D **slot, Texture2D *new_tex)
{
    (*slot)->textureRefs.RemoveRef(this);
    (*slot) = new_tex;
    (*slot)->textureRefs.AddRef(this);
}

void Material::DefaultSetup()
{
    unsigned int gl_tex_id = 0;
    for (auto tex : material_variables.allTextures)
    {
        glActiveTexture(GL_TEXTURE1 + gl_tex_id);
        glUniform1i(glGetUniformLocation(shader->ID, (tex->slot_name + ".texture").c_str()), 1 + gl_tex_id);
        shader->setVec2((tex->slot_name + ".tilling").c_str(), tex->variable.tilling );
        shader->setVec2((tex->slot_name + ".offset").c_str(), tex->variable.offset );
        glBindTexture(GL_TEXTURE_2D, (*tex->variable.texture)->id);
        gl_tex_id++;
    }
    shader->use();
    for (auto value : material_variables.allColor)
    {
        shader->setVec3(value->slot_name.c_str(), glm::vec3(value->variable[0], value->variable[1], value->variable[2]));
    }
    for (auto value : material_variables.allFloat)
    {
        shader->setFloat(value->slot_name.c_str(), *value->variable);
    }
    for (auto value : material_variables.allInt)
    {
        shader->setInt(value->slot_name.c_str(), *value->variable);
    }
    for (auto value : material_variables.allVec3)
    {
        shader->setVec3(value->slot_name.c_str(), glm::vec3(value->variable[0], value->variable[1], value->variable[2]));
    }
}

void Material::OnTextureRemoved(Texture2D *removed_texture)
{
    for (int i = 0; i < material_variables.allTextures.size(); i++)
    {
        if (*(material_variables.allTextures[i]->variable.texture) == removed_texture)
        {
            this->SetTexture(material_variables.allTextures[i]->variable.texture, EditorContent::editor_tex["white"]);
        }
    }
}

Material* MaterialManager::CreateMaterialByType(EMaterialType type)
{
    switch (type)
    {
    case EMaterialType::PHONG:
        return new PhongMaterial();
        break;
    case EMaterialType::BLINN_PHONG:
        return new BlinnPhongMaterial();
        break;
    case EMaterialType::COOK_TORRANCE:
        return new CTPBRMaterial();
        break;
    default:
        return new BlinnPhongMaterial();
        break;
    }
}

PhongMaterial::PhongMaterial() : Material::Material()
{
    shader = Shader::LoadedShaders["phong.fs"];

    // Init all material variables
    albedo_map->textureRefs.AddRef(this);
    normal_map->textureRefs.AddRef(this);
    parallax_map->textureRefs.AddRef(this);

    material_variables.allTextures.push_back(new MaterialSlot<MaterialTexture2D>("albedo_map", MaterialTexture2D(&albedo_map)));
    material_variables.allTextures.push_back(new MaterialSlot<MaterialTexture2D>("normal_map", MaterialTexture2D(&normal_map)));
    material_variables.allTextures.push_back(new MaterialSlot<MaterialTexture2D>("parallax_map", MaterialTexture2D(&parallax_map)));

    material_variables.allColor.push_back(new MaterialSlot<float*>("color", color));
    material_variables.allFloat.push_back(new MaterialSlot<float*>("heightScale", &height_scale));
    material_variables.allFloat.push_back(new MaterialSlot<float*>("shadowStrength", &shadow_strength));
}

void PhongMaterial::Setup(std::vector<Texture2D*> default_textures)
{
    DefaultSetup();
}

BlinnPhongMaterial::BlinnPhongMaterial() : Material::Material()
{
    shader = Shader::LoadedShaders["blinn_phong.fs"];

    // Init all material variables
    albedo_map->textureRefs.AddRef(this);
    normal_map->textureRefs.AddRef(this);
    parallax_map->textureRefs.AddRef(this);

    material_variables.allTextures.push_back(new MaterialSlot<MaterialTexture2D>("albedo_map", MaterialTexture2D(&albedo_map)));
    material_variables.allTextures.push_back(new MaterialSlot<MaterialTexture2D>("normal_map", MaterialTexture2D(&normal_map)));
    material_variables.allTextures.push_back(new MaterialSlot<MaterialTexture2D>("parallax_map", MaterialTexture2D(&parallax_map)));

    material_variables.allColor.push_back(new MaterialSlot<float*>("color", color));
    material_variables.allFloat.push_back(new MaterialSlot<float*>("heightScale", &height_scale));
    material_variables.allFloat.push_back(new MaterialSlot<float*>("shadowStrength", &shadow_strength));
} 

void BlinnPhongMaterial::Setup(std::vector<Texture2D*> default_textures)
{
    DefaultSetup();
}

CTPBRMaterial::CTPBRMaterial()
{
    shader = Shader::LoadedShaders["cook_torrance.fs"];

    // Init all material variables
    albedo_map->textureRefs.AddRef(this);
    normal_map->textureRefs.AddRef(this);
    metallic_map->textureRefs.AddRef(this);
    roughness_map->textureRefs.AddRef(this);
    ao_map->textureRefs.AddRef(this);

    material_variables.allTextures.push_back(new MaterialSlot<MaterialTexture2D>("albedo_map", MaterialTexture2D(&albedo_map)));
    material_variables.allTextures.push_back(new MaterialSlot<MaterialTexture2D>("normal_map", MaterialTexture2D(&normal_map)));
    material_variables.allTextures.push_back(new MaterialSlot<MaterialTexture2D>("metallic_map", MaterialTexture2D(&metallic_map)));
    material_variables.allTextures.push_back(new MaterialSlot<MaterialTexture2D>("roughness_map", MaterialTexture2D(&roughness_map)));
    material_variables.allTextures.push_back(new MaterialSlot<MaterialTexture2D>("ao_map", MaterialTexture2D(&ao_map)));


    material_variables.allColor.push_back(new MaterialSlot<float*>("color", color));
    material_variables.allFloat.push_back(new MaterialSlot<float*>("roughnessStrength", &roughness_strength));
    material_variables.allFloat.push_back(new MaterialSlot<float*>("metallicStrength", &metallic_strength));
    material_variables.allFloat.push_back(new MaterialSlot<float*>("aoStrength", &ao_strength));
    material_variables.allFloat.push_back(new MaterialSlot<float*>("shadowStrength", &shadow_strength));
}

void CTPBRMaterial::Setup(std::vector<Texture2D*> default_textures)
{
    DefaultSetup();
}

