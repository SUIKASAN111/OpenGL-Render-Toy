#include <glm/glm.hpp>
#include <glm/gtx/matrix_decompose.hpp>
#include <iostream>
#include <string>

#include "camera.h"
#include "shader.h"
#include "renderer_window.h"
#include "postprocess.h"
#include "render_texture.h"
#include "editor_settings.h"
#include "model.h"
#include "shader.h"
#include "render_pipeline.h"
#include "scene_object.h"
#include "gizmos.h"


unsigned int cubeVAO, cubeVBO;
unsigned int quadVAO, quadVBO;
void renderCube();
void renderQuad();

void RenderPipeline::EnqueueRenderQueue(SceneModel *model)     { ModelQueueForRender.insert({model->id, model});   }

void RenderPipeline::RemoveFromRenderQueue(unsigned int id)    { ModelQueueForRender.erase(id);                    }

RenderPipeline::RenderPipeline(RendererWindow* _window) : window(_window) 
{
    depth_texture = new DepthTexture(window->Width(), window->Height());
    z_buffer = new DepthTexture(window->Width(), window->Height());
    shadow_map = new DepthTexture(shadow_map_setting.shadow_map_size, shadow_map_setting.shadow_map_size);
    /*shadow_cubemap = new DepthCubeTexture(shadow_map_setting.shadow_map_size, shadow_map_setting.shadow_map_size);*/
    depth_shader = new Shader(  FileSystem::GetContentPath() / "Shader/depth.vs",
                                FileSystem::GetContentPath() / "Shader/depth.fs",
                                true);
    grid_shader = new Shader(   FileSystem::GetContentPath() / "Shader/grid.vs",
                                FileSystem::GetContentPath() / "Shader/grid.fs",
                                true);
    /*depth_cubemap_shader = new Shader(  FileSystem::GetContentPath() / "Shader/point_shadow_depth.vs",
		                                FileSystem::GetContentPath() / "Shader/point_shadow_depth.fs", 
                                        true,
                                        FileSystem::GetContentPath() / "Shader/point_shadow_depth.gs");*/
    skybox_shader = new Shader( FileSystem::GetContentPath() / "Shader/custom/skybox.vs",
                                FileSystem::GetContentPath() / "Shader/custom/skybox.fs",
                                true);
    hdr_background_shader = new Shader( FileSystem::GetContentPath() / "Shader/custom/hdr_background.vs",
                                        FileSystem::GetContentPath() / "Shader/custom/hdr_background.fs",
                                        true);
    equirectangular2cubemap_shader = new Shader(FileSystem::GetContentPath() / "Shader/custom/equirectangular2cubemap.vs",
                                                FileSystem::GetContentPath() / "Shader/custom/equirectangular2cubemap.fs",
                                                true);
    irradiance_convolution_shader = new Shader( FileSystem::GetContentPath() / "Shader/custom/irradiance_covolution.vs",
                                                FileSystem::GetContentPath() / "Shader/custom/irradiance_covolution.fs",
                                                true);
    prefilter_shader = new Shader(  FileSystem::GetContentPath() / "Shader/custom/prefilter.vs",
                                    FileSystem::GetContentPath() / "Shader/custom/prefilter.fs",
                                    true);
    brdf_shader = new Shader(   FileSystem::GetContentPath() / "Shader/custom/brdf.vs",
                                FileSystem::GetContentPath() / "Shader/custom/brdf.fs",
                                true);

    depth_shader->LoadShader();
    grid_shader->LoadShader();
    //depth_cubemap_shader->LoadShader();
    skybox_shader->LoadShader();
    hdr_background_shader->LoadShader();
    equirectangular2cubemap_shader->LoadShader();
    irradiance_convolution_shader->LoadShader();
    prefilter_shader->LoadShader();
    brdf_shader->LoadShader();

    // do some prepare before the render loop
    //InitSkyboxTex();
    InitHdrTex();
    IrradianceConvolution();
    PrefilterSpecularIBL();
}

RenderPipeline::~RenderPipeline()
{
    delete depth_texture;
    delete shadow_map;
    //delete shadow_cubemap;
    delete depth_shader;
    delete grid_shader;
}

SceneModel *RenderPipeline::GetRenderModel(unsigned int id)
{
    if (ModelQueueForRender.find(id) != ModelQueueForRender.end())
    {
        return ModelQueueForRender[id];
    }
    else
    {
        return nullptr;
    }
}

void RenderPipeline::OnWindowSizeChanged(int width, int height)
{
    postprocess_manager->ResizeRenderArea(width, height);

    // Resize depth texture as well
    delete depth_texture;
    depth_texture = new DepthTexture(width, height);
}

/*********************
* Shadow Pass
**********************/
void RenderPipeline::ProcessShadowPass()
{
    GLfloat near_plane = 1.0f, far_plane = 10000.0f;
    Transform* light_transform = global_light->atr_transform->transform;
    glm::vec3 lightPos = light_transform->Position();

    glViewport(0, 0, shadow_map_setting.shadow_map_size, shadow_map_setting.shadow_map_size);
    shadow_map->BindFrameBuffer();
    glEnable(GL_DEPTH_TEST);
    glClear(GL_DEPTH_BUFFER_BIT);
    depth_shader->use();
    
    float sdm_size = shadow_map_setting.shadow_distance;
    glm::mat4 light_projection = glm::ortho(-sdm_size, sdm_size, -sdm_size, sdm_size, near_plane, far_plane);
    auto camera_pos = window->render_camera->Position;
    glm::mat4 light_view = glm::lookAt(-light_transform->GetFront() * glm::vec3(50) + camera_pos, glm::vec3(0,0,0) + camera_pos, glm::vec3(0,1,0));

    for (std::map<unsigned int, SceneModel *>::iterator it = ModelQueueForRender.begin(); it != ModelQueueForRender.end(); it++)
    {
        SceneModel *sm = it->second;
        depth_shader->use();
        Transform *transform = sm->atr_transform->transform;
        glm::mat4 m = glm::mat4(1.0f);
        m = transform->GetTransformMatrix();
        depth_shader->setMat4("model", m);                      // M
        depth_shader->setMat4("view", light_view);              // V    
        depth_shader->setMat4("projection", light_projection);  // P

        for (int i = 0; i < sm->meshRenderers.size(); i++)
        {
            if (sm->meshRenderers[i]->cast_shadow && global_light->light_type == LightType::DIRECTIONAL)
            {
                // Draw without any material
                sm->meshRenderers[i]->PureDraw();
            }
        }
    }
}

//void RenderPipeline::ProcessPointShadowPass()
//{
//    GLfloat near_plane = 1.0f, far_plane = 10000.0f;
//    float shadow_map_size = shadow_map_setting.shadow_map_size;
//    Transform* light_transform = global_light->atr_transform->transform;
//    glm::vec3 lightPos = light_transform->Position();
//	glm::mat4 shadowProj = glm::perspective(glm::radians(90.0f), (float)shadow_map_size / (float)shadow_map_size, near_plane, far_plane);
//    std::vector<glm::mat4> shadowTransforms;
//    shadowTransforms.push_back(shadowProj * glm::lookAt(lightPos, lightPos + glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f)));
//    shadowTransforms.push_back(shadowProj * glm::lookAt(lightPos, lightPos + glm::vec3(-1.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f)));
//    shadowTransforms.push_back(shadowProj * glm::lookAt(lightPos, lightPos + glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f)));
//    shadowTransforms.push_back(shadowProj * glm::lookAt(lightPos, lightPos + glm::vec3(0.0f, -1.0f, 0.0f), glm::vec3(0.0f, 0.0f, -1.0f)));
//    shadowTransforms.push_back(shadowProj * glm::lookAt(lightPos, lightPos + glm::vec3(0.0f, 0.0f, 1.0f), glm::vec3(0.0f, -1.0f, 0.0f)));
//    shadowTransforms.push_back(shadowProj * glm::lookAt(lightPos, lightPos + glm::vec3(0.0f, 0.0f, -1.0f), glm::vec3(0.0f, -1.0f, 0.0f)));
//
//    glViewport(0, 0, shadow_map_setting.shadow_map_size, shadow_map_setting.shadow_map_size);
//    shadow_cubemap->BindFrameBuffer();
//    glEnable(GL_DEPTH_TEST);
//    glClear(GL_DEPTH_BUFFER_BIT);
//    depth_cubemap_shader->use();
//
//    for (unsigned int i = 0; i < 6; ++i)
//        depth_cubemap_shader->setMat4("shadowMatrices[" + std::to_string(i) + "]", shadowTransforms[i]);
//    depth_cubemap_shader->setFloat("far_plane", far_plane);
//    depth_cubemap_shader->setVec3("lightPos", lightPos);
//
//    for (std::map<unsigned int, SceneModel*>::iterator it = ModelQueueForRender.begin(); it != ModelQueueForRender.end(); it++)
//    {
//        SceneModel* sm = it->second;
//        depth_cubemap_shader->use();
//        Transform* transform = sm->atr_transform->transform;
//        glm::mat4 m = glm::mat4(1.0f);
//        m = transform->GetTransformMatrix();
//        depth_cubemap_shader->setMat4("model", m);                      // M
//        for (int i = 0; i < sm->meshRenderers.size(); i++)
//        {
//            if (sm->meshRenderers[i]->cast_shadow)
//            {
//                // Draw without any material
//                sm->meshRenderers[i]->PureDraw();
//            }
//        }
//    }
//}

/*********************
* Z-Pre Pass
**********************/
void RenderPipeline::ProcessZPrePass()
{
    glViewport(0, 0, window->Width(), window->Height());
    glEnable(GL_DEPTH_TEST);
    depth_texture->BindFrameBuffer();
    glClear(GL_DEPTH_BUFFER_BIT);
    // view/projection transformations
    Camera* camera = window->render_camera;
    glm::mat4 projection = glm::perspective(glm::radians(camera->Zoom), (float)window->Width() / (float)window->Height(), 0.1f, 10000.0f);
    glm::mat4 view = camera->GetViewMatrix();

    for (std::map<unsigned int, SceneModel *>::iterator it = ModelQueueForRender.begin(); it != ModelQueueForRender.end(); it++)
    {
        SceneModel *sm = it->second;
        depth_shader->use();
        Transform *transform = sm->atr_transform->transform;
        glm::mat4 m = glm::mat4(1.0f);
        m = transform->GetTransformMatrix();
        depth_shader->setMat4("model", m);                // M
        depth_shader->setMat4("view", view);              // V    
        depth_shader->setMat4("projection", projection);  // P
        
        for (int i = 0; i < sm->meshRenderers.size(); i++)
        {
            // Draw without any material
            sm->meshRenderers[i]->PureDraw();
        }
    }

    depth_texture->SetAsReadTarget();
    z_buffer->SetAsRenderTarget();
    glBlitFramebuffer(0, 0, window->Width(), window->Height(), 0, 0, window->Width(), window->Height(), GL_DEPTH_BUFFER_BIT, GL_NEAREST);
}

/*********************
* Color Pass
**********************/
void RenderPipeline::ProcessColorPass()
{
    glClearColor(clear_color[0], clear_color[1], clear_color[2], 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);

    if (EditorSettings::UsePolygonMode)
    {
        glLineWidth(0.05);
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    }
    else
    {
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    }

    glViewport(0, 0, window->Width(), window->Height());
    // view/projection transformations
    Camera* camera = window->render_camera;
    glm::mat4 projection = glm::perspective(glm::radians(camera->Zoom), (float)window->Width() / (float)window->Height(), 0.1f, 10000.0f);
    glm::mat4 view = camera->GetViewMatrix();

    GLfloat near_plane = 1.0f, far_plane = 10000.0f;
    float sdm_size = shadow_map_setting.shadow_distance;
    glm::mat4 light_projection = glm::ortho(-sdm_size, sdm_size, -sdm_size, sdm_size, near_plane, far_plane);
    auto camera_pos = window->render_camera->Position;

    Transform* light_transform = global_light->atr_transform->transform;
    glm::vec3 lightPos = light_transform->Position();
    glm::mat4 light_view;
	bool pointLight = (global_light->light_type == LightType::POINT);
    light_view = glm::lookAt(-light_transform->GetFront() * glm::vec3(50) + camera_pos, glm::vec3(0,0,0) + camera_pos, glm::vec3(0,1,0));
  
    // Render Scene (Color Pass)
    for (std::map<unsigned int, SceneModel *>::iterator it = ModelQueueForRender.begin(); it != ModelQueueForRender.end(); it++)
    {
        SceneModel *sm = it->second;

        Material* prev_mat = nullptr;
        for (auto mr : sm->meshRenderers)
        {
            Material* mat = mr->material;
            if (mat == prev_mat)
            {
                continue;
            }
            prev_mat = mat;
            Shader* shader;
            if (EditorSettings::UsePolygonMode || !mat->shader->IsValid())
            {
                shader = Shader::LoadedShaders["default.fs"];
            }
            else
            {
                shader = mat->shader;
            }
            shader->use();
            // Render the loaded model
            Transform *transform = sm->atr_transform->transform;
            glm::mat4 m = glm::mat4(1.0f);
            m = transform->GetTransformMatrix();
            shader->setMat4("model", m);                // M
            shader->setMat4("view", view);              // V    
            shader->setMat4("projection", projection);  // P
            shader->setVec3("viewPos", camera->Position);
            shader->setMat4("light_view", light_view);
            shader->setMat4("light_projection", light_projection);
            shader->setInt("z_buffer", z_buffer->color_buffer);

            glActiveTexture(GL_TEXTURE0);
            glUniform1i(glGetUniformLocation(shader->ID, "shadowMap"), 0);
            glBindTexture(GL_TEXTURE_2D, shadow_map->color_buffer);
            glActiveTexture(GL_TEXTURE13);
            shader->setInt("irradianceMap", 13);
            glBindTexture(GL_TEXTURE_CUBE_MAP, irradianceMap);
            glActiveTexture(GL_TEXTURE14);
            shader->setInt("prefilterMap", 14);
            glBindTexture(GL_TEXTURE_CUBE_MAP, prefilterMap);
            glActiveTexture(GL_TEXTURE15);
            shader->setInt("brdfLUTTexture", 15);
            glBindTexture(GL_TEXTURE_2D, brdfLUTTexture);

            shader->setBool("skybox_enabled", EditorSettings::SkyboxEnabled);

            if (global_light != nullptr)
            {
                shader->setBool("pointLight", (global_light->light_type == LightType::POINT));
                shader->setVec3("lightPos", lightPos);
                glm::vec3 front = global_light->atr_transform->transform->GetFront();
                shader->setVec3("lightDir", -front);
                glm::vec3 lightColor = global_light->GetLightColor();
                float lightIntensity = global_light->GetLightIntensity();
                shader->setVec3("lightColor", lightColor);
                shader->setFloat("lightIntensity",  lightIntensity);
            }
            else
            {
                shader->setVec3("lightDir", glm::vec3(1, 1, 1));
                shader->setVec3("lightColor", glm::vec3(1, 0, 0));
            }
        }
        sm->DrawSceneModel();
    }
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
}

//void RenderPipeline::ProcessPointColorPass()
//{
//    glClearColor(clear_color[0], clear_color[1], clear_color[2], 1);
//    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
//    glEnable(GL_DEPTH_TEST);
//
//    if (EditorSettings::UsePolygonMode)
//    {
//        glLineWidth(0.05);
//        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
//    }
//    else
//    {
//        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
//    }
//
//    glViewport(0, 0, window->Width(), window->Height());
//    // view/projection transformations
//    Camera* camera = window->render_camera;
//    glm::mat4 projection = glm::perspective(glm::radians(camera->Zoom), (float)window->Width() / (float)window->Height(), 0.1f, 10000.0f);
//    glm::mat4 view = camera->GetViewMatrix();
//
//    GLfloat near_plane = 1.0f, far_plane = 10000.0f;
//    float sdm_size = shadow_map_setting.shadow_distance;
//    glm::mat4 light_projection = glm::ortho(-sdm_size, sdm_size, -sdm_size, sdm_size, near_plane, far_plane);
//    auto camera_pos = window->render_camera->Position;
//
//    Transform* light_transform = global_light->atr_transform->transform;
//    glm::vec3 lightPos = light_transform->Position();
//    glm::mat4 light_view;
//    bool pointLight = global_light->light_type == LightType::POINT;
//    light_view = glm::lookAt(-light_transform->GetFront() * glm::vec3(50) + camera_pos, glm::vec3(0, 0, 0) + camera_pos, glm::vec3(0, 1, 0));
//
//    // Render Scene (Color Pass)
//    for (std::map<unsigned int, SceneModel*>::iterator it = ModelQueueForRender.begin(); it != ModelQueueForRender.end(); it++)
//    {
//        SceneModel* sm = it->second;
//
//        Material* prev_mat = nullptr;
//        for (auto mr : sm->meshRenderers)
//        {
//            Material* mat = mr->material;
//            if (mat == prev_mat)
//            {
//                continue;
//            }
//            prev_mat = mat;
//            Shader* shader;
//            if (EditorSettings::UsePolygonMode || !mat->shader->IsValid())
//            {
//                shader = Shader::LoadedShaders["default.fs"];
//            }
//            else
//            {
//                shader = mat->shader;
//            }
//            shader->use();
//            // Render the loaded model
//            Transform* transform = sm->atr_transform->transform;
//            glm::mat4 m = glm::mat4(1.0f);
//            m = transform->GetTransformMatrix();
//            shader->setMat4("model", m);                // M
//            shader->setMat4("view", view);              // V    
//            shader->setMat4("projection", projection);  // P
//            shader->setVec3("viewPos", camera->Position);
//            shader->setMat4("light_view", light_view);
//            shader->setMat4("light_projection", light_projection);
//
//            glActiveTexture(GL_TEXTURE8);
//            glUniform1i(glGetUniformLocation(shader->ID, "shadowCubeMap"), 8);
//			glBindTexture(GL_TEXTURE_CUBE_MAP, shadow_cubemap->color_buffer);
//
//            if (global_light != nullptr)
//            {
//                shader->setBool("pointLight", global_light->light_type == LightType::POINT);
//                shader->setVec3("lightPos", lightPos);
//                glm::vec3 front = global_light->atr_transform->transform->GetFront();
//                shader->setVec3("lightDir", -front);
//                glm::vec3 lightColor = global_light->GetLightColor();
//                shader->setVec3("lightColor", lightColor);
//            }
//            else
//            {
//                shader->setVec3("lightDir", glm::vec3(1, 1, 1));
//                shader->setVec3("lightColor", glm::vec3(1, 0, 0));
//            }
//        }
//        sm->DrawSceneModel();
//    }
//    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
//
//}

/*********************
* Render Gizmos
**********************/
void RenderPipeline::RenderGizmos()
{
    glDisable(GL_DEPTH_TEST);
    Camera* camera = window->render_camera;
    glm::mat4 model = glm::mat4(1.0f);
    glm::mat4 view = camera->GetViewMatrix();
    glm::mat4 projection = glm::perspective(glm::radians(camera->Zoom), (float)window->Width() / (float)window->Height(), 0.1f, 10000.0f);
    Shader::LoadedShaders["color.fs"]->use();
    Shader::LoadedShaders["color.fs"]->setMat4("model", model);
    Shader::LoadedShaders["color.fs"]->setMat4("view", view);
    Shader::LoadedShaders["color.fs"]->setMat4("projection", projection);

    // Draw coordinate axis
    glLineWidth(4);
    for (std::map<unsigned int, SceneModel *>::iterator it = ModelQueueForRender.begin(); it != ModelQueueForRender.end(); it++)
    {
        SceneModel *sm = it->second;
        if (sm->is_selected)
        {
            
            Transform* transform = sm->atr_transform->transform; 
            GLine front(transform->Position(), transform->Position() + transform->GetFront());
            front.color = glm::vec3(0,0,1);
            front.DrawInGlobal();
            GLine right(transform->Position(), transform->Position() + transform->GetRight());
            right.color = glm::vec3(1,0,0);
            right.DrawInGlobal();
            GLine up(transform->Position(), transform->Position() + transform->GetUp());
            up.color = glm::vec3(0,1,0);
            up.DrawInGlobal();
        }
    }

    // Draw light debug cube
    glLineWidth(2);
    if (global_light->is_selected)
    {
        float r = 0.2;
        GCube light_cube(0.2);
        light_cube.color = global_light->GetLightColor();
        light_cube.transform = *global_light->atr_transform->transform;
        light_cube.Draw();

        if (global_light->light->light_type != global_light->light_type)
        {
            global_light->light_type = global_light->light->light_type;
        }

        if (global_light->light_type == LightType::DIRECTIONAL)
        {
            // GLine front(glm::vec3(0), glm::vec3(0,0,2));
            GLine front(light_cube.transform.Position(), (light_cube.transform.Position() + glm::vec3(2) * light_cube.transform.GetFront()));
            front.color = global_light->GetLightColor();
            front.DrawInGlobal();
        }
    }

    // Draw a grid
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    GGrid grid;
    Shader::LoadedShaders["grid.fs"]->use();
    Shader::LoadedShaders["grid.fs"]->setMat4("view", view);
    Shader::LoadedShaders["grid.fs"]->setMat4("projection", projection);
    Shader::LoadedShaders["grid.fs"]->setVec3("cameraPos", camera->Position);
    grid.Draw();
    glDisable(GL_BLEND);
}


/*********************
* Skybox Pass
**********************/
void RenderPipeline::RenderSkybox()
{
    glDepthFunc(GL_LEQUAL);  // change depth function so depth test passes when values are equal to depth buffer's content
    skybox_shader->use();
    skybox_shader->setInt("skybox", 0);
    Camera* camera = window->render_camera;
    glm::mat4 view = glm::mat4(glm::mat3(camera->GetViewMatrix())); // remove translation from the view matrix
    float width = window->cur_window_size.width;
    float height = window->cur_window_size.height;
    glm::mat4 projection = glm::perspective(glm::radians(camera->Zoom), (float)width / (float)height, 0.1f, 100.0f);
    skybox_shader->setMat4("view", view);
    skybox_shader->setMat4("projection", projection);
    glBindVertexArray(skyboxVAO);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_CUBE_MAP, skyboxTexture);
    glDrawArrays(GL_TRIANGLES, 0, 36);
    glBindVertexArray(0);
    glDepthFunc(GL_LESS);
}

void RenderPipeline::RenderHdrBackground()
{
    glDepthFunc(GL_LEQUAL);
    Camera* camera = window->render_camera;
    glm::mat4 view = camera->GetViewMatrix();
    float width = window->cur_window_size.width;
    float height = window->cur_window_size.height;
    glm::mat4 projection = glm::perspective(glm::radians(camera->Zoom), (float)width / (float)height, 0.1f, 100.0f);
    hdr_background_shader->use();
    hdr_background_shader->setMat4("view", view);
    hdr_background_shader->setMat4("projection", projection);
    hdr_background_shader->setInt("environmentMap", 0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_CUBE_MAP, envCubemap);
	renderCube();
	glDepthFunc(GL_LESS);
}


/****************************************************************
* Render order is decide by create order by now.
* If there's a requirement to render model with alpha
* we need to sort models by distance to camera. 
* All models is rendered without alpha clip or alpha blend now.
*****************************************************************/
void RenderPipeline::Render()
{
    // Draw shadow pass
    //if (global_light->light_type == LightType::POINT)
    //{
    //    ProcessPointShadowPass();
    //}
    //else
    //{
    //    ProcessShadowPass();
    //}
    ProcessShadowPass();

    // Z-PrePass
    ProcessZPrePass();

    // Pre Render Setting
    if (EditorSettings::UsePostProcess && !EditorSettings::UsePolygonMode && postprocess_manager != nullptr)
    {
        postprocess_manager->read_rt->BindFrameBuffer();
    }
    else
    {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    // Draw color pass
    //if (global_light->light_type == LightType::POINT)
    //{
    //    ProcessPointColorPass();
    //}
    //else
    //{
    //    ProcessColorPass();
    //}
    ProcessColorPass();

    // Draw Gizmos
    if (EditorSettings::DrawGizmos)
    {
        RenderGizmos();
    }

    // Render skybox
    if (EditorSettings::SkyboxEnabled)
    {
        //RenderSkybox();
        RenderHdrBackground();
    }

    // PostProcess
    if (EditorSettings::UsePostProcess && !EditorSettings::UsePolygonMode && postprocess_manager != nullptr)
    {
        postprocess_manager->ExecutePostProcessList();
    }
}


// Init Skybox
// ------------------------------------
void RenderPipeline::InitSkyboxTex()
{
    float skyboxVertices[] = {
        // positions          
        -1.0f,  1.0f, -1.0f,
        -1.0f, -1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,
         1.0f,  1.0f, -1.0f,
        -1.0f,  1.0f, -1.0f,

        -1.0f, -1.0f,  1.0f,
        -1.0f, -1.0f, -1.0f,
        -1.0f,  1.0f, -1.0f,
        -1.0f,  1.0f, -1.0f,
        -1.0f,  1.0f,  1.0f,
        -1.0f, -1.0f,  1.0f,

         1.0f, -1.0f, -1.0f,
         1.0f, -1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f,  1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,

        -1.0f, -1.0f,  1.0f,
        -1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f, -1.0f,  1.0f,
        -1.0f, -1.0f,  1.0f,

        -1.0f,  1.0f, -1.0f,
         1.0f,  1.0f, -1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
        -1.0f,  1.0f,  1.0f,
        -1.0f,  1.0f, -1.0f,

        -1.0f, -1.0f, -1.0f,
        -1.0f, -1.0f,  1.0f,
         1.0f, -1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,
        -1.0f, -1.0f,  1.0f,
         1.0f, -1.0f,  1.0f
    };

    std::vector<std::string> faces
    {
        "content/Textures/skybox/right.jpg",
        "content/Textures/skybox/left.jpg",
        "content/Textures/skybox/top.jpg",
        "content/Textures/skybox/bottom.jpg",
        "content/Textures/skybox/front.jpg",
        "content/Textures/skybox/back.jpg"
    };

    glGenTextures(1, &skyboxTexture);
    glBindTexture(GL_TEXTURE_CUBE_MAP, skyboxTexture);

    int width, height, nrChannels;
    for (unsigned int i = 0; i < faces.size(); i++)
    {
        unsigned char* data = stbi_load(faces[i].c_str(), &width, &height, &nrChannels, 0);
        if (data)
        {
            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
            stbi_image_free(data);
        }
        else
        {
            std::cout << "Cubemap texture failed to load at path: " << faces[i] << std::endl;
            stbi_image_free(data);
        }
    }
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

    // skybox VAO
    glGenVertexArrays(1, &skyboxVAO);
    glGenBuffers(1, &skyboxVBO);
    glBindVertexArray(skyboxVAO);
    glBindBuffer(GL_ARRAY_BUFFER, skyboxVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(skyboxVertices), &skyboxVertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
}

void RenderPipeline::InitHdrTex()
{
    // pbr: setup framebuffer
    // ----------------------
    glGenFramebuffers(1, &captureFBO);
    glGenRenderbuffers(1, &captureRBO);

    glBindFramebuffer(GL_FRAMEBUFFER, captureFBO);
    glBindRenderbuffer(GL_RENDERBUFFER, captureRBO);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, 512, 512);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, captureRBO);
    // pbr: load the HDR environment map
    // ---------------------------------
    stbi_set_flip_vertically_on_load(true);
    int width, height, nrComponents;
	float* data = stbi_loadf("content/Textures/hdr/brown_photostudio_07_16k.hdr", &width, &height, &nrComponents, 0);
    unsigned int hdrTexture;
    if (data)
    {
        glGenTextures(1, &hdrTexture);
        glBindTexture(GL_TEXTURE_2D, hdrTexture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, width, height, 0, GL_RGB, GL_FLOAT, data); // note how we specify the texture's data value to be float

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        stbi_image_free(data);
    }
    else
    {
        std::cout << "Failed to load HDR image." << std::endl;
    }

    // pbr: setup cubemap to render to and attach to framebuffer
    // ---------------------------------------------------------
    glGenTextures(1, &envCubemap);
    glBindTexture(GL_TEXTURE_CUBE_MAP, envCubemap);
    for (unsigned int i = 0; i < 6; ++i)
    {
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB16F, 512, 512, 0, GL_RGB, GL_FLOAT, nullptr);
    }
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    // pbr: convert HDR equirectangular environment map to cubemap equivalent
	// ----------------------------------------------------------------------
	equirectangular2cubemap_shader->use();
	equirectangular2cubemap_shader->setInt("equirectangularMap", 0);
	equirectangular2cubemap_shader->setMat4("projection", captureProjection);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, hdrTexture);

    glViewport(0, 0, 512, 512); // don't forget to configure the viewport to the capture dimensions.
    glBindFramebuffer(GL_FRAMEBUFFER, captureFBO);
    for (unsigned int i = 0; i < 6; ++i)
    {
        equirectangular2cubemap_shader->setMat4("view", captureViews[i]);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, envCubemap, 0);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // render cube
        renderCube();
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}


// generate irradiance map
void RenderPipeline::IrradianceConvolution()
{
    // pbr: create an irradiance cubemap, and re - scale capture FBO to irradiance scale.
    // --------------------------------------------------------------------------------
    glGenTextures(1, &irradianceMap);
    glBindTexture(GL_TEXTURE_CUBE_MAP, irradianceMap);
    for (unsigned int i = 0; i < 6; ++i)
    {
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB16F, 32, 32, 0, GL_RGB, GL_FLOAT, nullptr);
    }
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glBindFramebuffer(GL_FRAMEBUFFER, captureFBO);
    glBindRenderbuffer(GL_RENDERBUFFER, captureRBO);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, 32, 32);

    // pbr: solve diffuse integral by convolution to create an irradiance (cube)map.
    // -----------------------------------------------------------------------------
    irradiance_convolution_shader->use();
    irradiance_convolution_shader->setInt("environmentMap", 0);
    irradiance_convolution_shader->setMat4("projection", captureProjection);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_CUBE_MAP, envCubemap);

    glViewport(0, 0, 32, 32); // don't forget to configure the viewport to the capture dimensions.
    glBindFramebuffer(GL_FRAMEBUFFER, captureFBO);
    for (unsigned int i = 0; i < 6; ++i)
    {
        irradiance_convolution_shader->setMat4("view", captureViews[i]);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, irradianceMap, 0);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        renderCube();
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void RenderPipeline::PrefilterSpecularIBL()
{
    glGenTextures(1, &prefilterMap);
    glBindTexture(GL_TEXTURE_CUBE_MAP, prefilterMap);
    for (unsigned int i = 0; i < 6; ++i)
    {
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB16F, 128, 128, 0, GL_RGB, GL_FLOAT, nullptr);
    }
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR); // be sure to set minification filter to mip_linear 
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    // generate mipmaps for the cubemap so OpenGL automatically allocates the required memory.
    glGenerateMipmap(GL_TEXTURE_CUBE_MAP);

    // pbr: run a quasi monte-carlo simulation on the environment lighting to create a prefilter (cube)map.
    // ----------------------------------------------------------------------------------------------------
    prefilter_shader->use();
    prefilter_shader->setInt("environmentMap", 0);
    prefilter_shader->setMat4("projection", captureProjection);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_CUBE_MAP, envCubemap);

    glBindFramebuffer(GL_FRAMEBUFFER, captureFBO);
    unsigned int maxMipLevels = 5;
    for (unsigned int mip = 0; mip < maxMipLevels; ++mip)
    {
        // reisze framebuffer according to mip-level size.
        unsigned int mipWidth = static_cast<unsigned int>(128 * std::pow(0.5, mip));
        unsigned int mipHeight = static_cast<unsigned int>(128 * std::pow(0.5, mip));
        glBindRenderbuffer(GL_RENDERBUFFER, captureRBO);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, mipWidth, mipHeight);
        glViewport(0, 0, mipWidth, mipHeight);

        float roughness = (float)mip / (float)(maxMipLevels - 1);
        prefilter_shader->setFloat("roughness", roughness);
        for (unsigned int i = 0; i < 6; ++i)
        {
            prefilter_shader->setMat4("view", captureViews[i]);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, prefilterMap, mip);

            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            renderCube();
        }
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // pbr: generate a 2D LUT from the BRDF equations used.
    // ----------------------------------------------------
    glGenTextures(1, &brdfLUTTexture);

    // pre-allocate enough memory for the LUT texture.
    glBindTexture(GL_TEXTURE_2D, brdfLUTTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RG16F, 512, 512, 0, GL_RG, GL_FLOAT, 0);
    // be sure to set wrapping mode to GL_CLAMP_TO_EDGE
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    // then re-configure capture framebuffer object and render screen-space quad with BRDF shader.
    glBindFramebuffer(GL_FRAMEBUFFER, captureFBO);
    glBindRenderbuffer(GL_RENDERBUFFER, captureRBO);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, 512, 512);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, brdfLUTTexture, 0);

    glViewport(0, 0, 512, 512);
    brdf_shader->use();
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    renderQuad();

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}


// assistant func
// --------------
void renderCube()
{
    // initialize (if necessary)
    if (cubeVAO == 0)
    {
        float vertices[] = {
            // back face
            -1.0f, -1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 0.0f, 0.0f, // bottom-left
             1.0f,  1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 1.0f, 1.0f, // top-right
             1.0f, -1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 1.0f, 0.0f, // bottom-right         
             1.0f,  1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 1.0f, 1.0f, // top-right
            -1.0f, -1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 0.0f, 0.0f, // bottom-left
            -1.0f,  1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 0.0f, 1.0f, // top-left
            // front face
            -1.0f, -1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 0.0f, 0.0f, // bottom-left
             1.0f, -1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 1.0f, 0.0f, // bottom-right
             1.0f,  1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 1.0f, 1.0f, // top-right
             1.0f,  1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 1.0f, 1.0f, // top-right
            -1.0f,  1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 0.0f, 1.0f, // top-left
            -1.0f, -1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 0.0f, 0.0f, // bottom-left
            // left face
            -1.0f,  1.0f,  1.0f, -1.0f,  0.0f,  0.0f, 1.0f, 0.0f, // top-right
            -1.0f,  1.0f, -1.0f, -1.0f,  0.0f,  0.0f, 1.0f, 1.0f, // top-left
            -1.0f, -1.0f, -1.0f, -1.0f,  0.0f,  0.0f, 0.0f, 1.0f, // bottom-left
            -1.0f, -1.0f, -1.0f, -1.0f,  0.0f,  0.0f, 0.0f, 1.0f, // bottom-left
            -1.0f, -1.0f,  1.0f, -1.0f,  0.0f,  0.0f, 0.0f, 0.0f, // bottom-right
            -1.0f,  1.0f,  1.0f, -1.0f,  0.0f,  0.0f, 1.0f, 0.0f, // top-right
            // right face
             1.0f,  1.0f,  1.0f,  1.0f,  0.0f,  0.0f, 1.0f, 0.0f, // top-left
             1.0f, -1.0f, -1.0f,  1.0f,  0.0f,  0.0f, 0.0f, 1.0f, // bottom-right
             1.0f,  1.0f, -1.0f,  1.0f,  0.0f,  0.0f, 1.0f, 1.0f, // top-right         
             1.0f, -1.0f, -1.0f,  1.0f,  0.0f,  0.0f, 0.0f, 1.0f, // bottom-right
             1.0f,  1.0f,  1.0f,  1.0f,  0.0f,  0.0f, 1.0f, 0.0f, // top-left
             1.0f, -1.0f,  1.0f,  1.0f,  0.0f,  0.0f, 0.0f, 0.0f, // bottom-left     
            // bottom face
            -1.0f, -1.0f, -1.0f,  0.0f, -1.0f,  0.0f, 0.0f, 1.0f, // top-right
             1.0f, -1.0f, -1.0f,  0.0f, -1.0f,  0.0f, 1.0f, 1.0f, // top-left
             1.0f, -1.0f,  1.0f,  0.0f, -1.0f,  0.0f, 1.0f, 0.0f, // bottom-left
             1.0f, -1.0f,  1.0f,  0.0f, -1.0f,  0.0f, 1.0f, 0.0f, // bottom-left
            -1.0f, -1.0f,  1.0f,  0.0f, -1.0f,  0.0f, 0.0f, 0.0f, // bottom-right
            -1.0f, -1.0f, -1.0f,  0.0f, -1.0f,  0.0f, 0.0f, 1.0f, // top-right
            // top face
            -1.0f,  1.0f, -1.0f,  0.0f,  1.0f,  0.0f, 0.0f, 1.0f, // top-left
             1.0f,  1.0f , 1.0f,  0.0f,  1.0f,  0.0f, 1.0f, 0.0f, // bottom-right
             1.0f,  1.0f, -1.0f,  0.0f,  1.0f,  0.0f, 1.0f, 1.0f, // top-right     
             1.0f,  1.0f,  1.0f,  0.0f,  1.0f,  0.0f, 1.0f, 0.0f, // bottom-right
            -1.0f,  1.0f, -1.0f,  0.0f,  1.0f,  0.0f, 0.0f, 1.0f, // top-left
            -1.0f,  1.0f,  1.0f,  0.0f,  1.0f,  0.0f, 0.0f, 0.0f  // bottom-left        
        };
        glGenVertexArrays(1, &cubeVAO);
        glGenBuffers(1, &cubeVBO);
        // fill buffer
        glBindBuffer(GL_ARRAY_BUFFER, cubeVBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
        // link vertex attributes
        glBindVertexArray(cubeVAO);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindVertexArray(0);
    }
    // render Cube
    glBindVertexArray(cubeVAO);
    glDrawArrays(GL_TRIANGLES, 0, 36);
    glBindVertexArray(0);
}

void renderQuad()
{
    if (quadVAO == 0)
    {
        float quadVertices[] = {
            // positions        // texture Coords
            -1.0f,  1.0f, 0.0f, 0.0f, 1.0f,
            -1.0f, -1.0f, 0.0f, 0.0f, 0.0f,
             1.0f,  1.0f, 0.0f, 1.0f, 1.0f,
             1.0f, -1.0f, 0.0f, 1.0f, 0.0f,
        };
        // setup plane VAO
        glGenVertexArrays(1, &quadVAO);
        glGenBuffers(1, &quadVBO);
        glBindVertexArray(quadVAO);
        glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), &quadVertices, GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
    }
    glBindVertexArray(quadVAO);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glBindVertexArray(0);
}