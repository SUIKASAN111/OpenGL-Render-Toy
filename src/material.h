#pragma once

#include <vector>
#include <string>
#include "texture.h"
#include "editor_content.h"
#include "singleton_util.h"

class Shader;

enum E_CULL_FACE
{
	culloff 		= 0,
	cullfront 		= 1,
	cullback 		= 2
};

template <class T>
class MaterialSlot
{
public:
	MaterialSlot(std::string _name, T _variable) : variable(_variable), slot_name(_name) {}
	
	std::string slot_name;
	T variable;
};

struct MaterialTexture2D
{
	Texture2D** texture;
	glm::vec2 tilling = glm::vec2(1, 1);
	glm::vec2 offset = glm::vec2(0, 0);

	MaterialTexture2D(Texture2D** _texture)
	{
		texture = _texture;
	}
};

struct MaterialVariables
{
	std::vector<MaterialSlot<MaterialTexture2D>*> 	allTextures;
	std::vector<MaterialSlot<int*>*> 				allInt;
	std::vector<MaterialSlot<float*>*> 				allFloat;
	std::vector<MaterialSlot<float*>*> 				allVec3;
	std::vector<MaterialSlot<float*>*> 				allColor;
};

enum EMaterialType
{
	PHONG,
	BLINN_PHONG,
	COOK_TORRANCE
};

class MaterialManager : public Singleton<MaterialManager>
{
public:
	static Material* CreateMaterialByType(EMaterialType type);
};

class Material
{
public:
	const std::string name = "Material Base";
	unsigned int id;
	Shader* shader;
	E_CULL_FACE cullface = E_CULL_FACE::culloff;
	void SetTexture(Texture2D** slot, Texture2D* new_tex);
	MaterialVariables material_variables;

private:
	static unsigned int cur_id;

public:
	Material();
    virtual ~Material();
	bool IsValid();
	virtual void Setup(std::vector<Texture2D*> default_textures) = 0;
	void OnTextureRemoved(Texture2D* removed_texture);

protected:
	void DefaultSetup();
};

class PhongMaterial : public Material
{
public:
	PhongMaterial();
	~PhongMaterial() override;
	void Setup(std::vector<Texture2D*> default_textures) 	override;

	const std::string name = "Phong Material";
	Texture2D* albedo_map = EditorContent::editor_tex["white"];
	Texture2D* normal_map = EditorContent::editor_tex["normal"];
	Texture2D* parallax_map = EditorContent::editor_tex["white"];
	float color[3] = { 1, 1, 1 };
	float height_scale = 0;
	float shadow_strength = 1;
};

class BlinnPhongMaterial : public Material
{
public:
	BlinnPhongMaterial();
	~BlinnPhongMaterial() override;
	void Setup(std::vector<Texture2D*> default_textures) 	override;

	const std::string name = "Blinn-Phong Material";
	Texture2D* albedo_map = EditorContent::editor_tex["white"];
	Texture2D* normal_map = EditorContent::editor_tex["normal"];
	Texture2D* parallax_map = EditorContent::editor_tex["white"];
	float color[3] = { 1, 1, 1 };
	float height_scale = 0;
	float shadow_strength = 1;
};

class CTPBRMaterial : public Material
{
public:
	const std::string name = "Cook-Torrance Material";
	CTPBRMaterial();
	~CTPBRMaterial() override;
	void Setup(std::vector<Texture2D*> default_textures) override;

	Texture2D* albedo_map = EditorContent::editor_tex["white"];
	Texture2D* normal_map = EditorContent::editor_tex["normal"];
	Texture2D* metallic_map = EditorContent::editor_tex["white"];
	Texture2D* roughness_map = EditorContent::editor_tex["white"];
	Texture2D* ao_map = EditorContent::editor_tex["white"];

	float color[3] = { 1, 1, 1 };
	float roughness_strength = 1;
	float metallic_strength = 0;
	float ao_strength = 1;
	float shadow_strength = 1;
};