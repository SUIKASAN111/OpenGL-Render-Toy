#pragma once

#include <map>
#include <stb_image.h>

#include "renderer_window.h"

class SceneModel;
class SceneLight;
class Camera;
class Shader;
class DepthTexture;
class DepthCubeTexture;
class RendererWindow;
class PostProcessManager;

class RenderPipeline : public IOnWindowSizeChanged
{
public:
	RenderPipeline(RendererWindow* _window);
    ~RenderPipeline();
	void EnqueueRenderQueue(SceneModel* model);
	//void RemoveFromRenderQueue(SceneModel* model);
	void RemoveFromRenderQueue(unsigned int id);
	SceneModel* GetRenderModel(unsigned int id);
	void Render();
	void OnWindowSizeChanged(int width, int height) override;

    struct ShadowMapSetting
    {
        float shadow_map_size = 4096;
        float shadow_distance = 50;
	} shadow_map_setting;

    float *clear_color;
    SceneLight* global_light;
    PostProcessManager *postprocess_manager = nullptr;

    DepthTexture* depth_texture;
    DepthTexture* z_buffer;
    DepthTexture* shadow_map;
    //DepthCubeTexture* shadow_cubemap;

	unsigned int skyboxTexture;
    unsigned int skyboxVAO, skyboxVBO;

    unsigned int envCubemap;
    unsigned int irradianceMap;
    unsigned int prefilterMap;
    unsigned int brdfLUTTexture;

private:
    std::map<unsigned int, SceneModel *> ModelQueueForRender;
    RendererWindow *window;
    // Shaders
    Shader* depth_shader;   // for shadow map
    Shader* grid_shader;    // for grid rendering
    //Shader* depth_cubemap_shader;
    Shader* skybox_shader;  // for skybox cubemap rendering
    Shader* hdr_background_shader;  // for hdr skybox, the diffierence from the previous one is that we need gamma correction for hdr
    Shader* equirectangular2cubemap_shader; // convert hdr equirectangular map to cubemap
    Shader* irradiance_convolution_shader; // generate irradiance map for diffuse ambient term
    Shader* prefilter_shader; // prefilter specular IBL Split-Sum Part.1
    Shader* brdf_shader; // for brdf convolution  Split-Sum Part.2


    void ProcessZPrePass        ();
    void ProcessShadowPass      ();
    //void ProcessPointShadowPass ();
    void ProcessColorPass       ();
    //void ProcessPointColorPass  ();
    void RenderGizmos           ();
    void RenderSkybox           ();
    void RenderHdrBackground    ();

    void InitSkyboxTex();
    void InitHdrTex();
    void IrradianceConvolution();
    void PrefilterSpecularIBL();


    glm::mat4 captureProjection = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 10.0f);
    glm::mat4 captureViews[6] =
    {
        glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(1.0f,  0.0f,  0.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
        glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(-1.0f,  0.0f,  0.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
        glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f,  1.0f,  0.0f), glm::vec3(0.0f,  0.0f,  1.0f)),
        glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f,  0.0f), glm::vec3(0.0f,  0.0f, -1.0f)),
        glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f,  0.0f,  1.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
        glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f,  0.0f, -1.0f), glm::vec3(0.0f, -1.0f,  0.0f))
    };

    unsigned int captureFBO;
    unsigned int captureRBO;
};