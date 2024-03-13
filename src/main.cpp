#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <vector>

#include "file_system.h"
#include "renderer_window.h"
#include "input_management.h"
#include "renderer_ui.h"
#include "scene_object.h"
#include "material.h"
#include "model.h"
#include "scene.h"
#include "shader.h"
#include "editor_content.h"
#include "render_texture.h"
#include "editor_settings.h"
#include "postprocess.h"

#define window_width    1920
#define window_height   1080

const double Pi = 3.1415926;

bool focused        = false;
float currentFrame  = 0.0f;
float lastFrame     = 0.0f;
float deltaTime     = 0.0f;

unsigned int loadCubemap(vector<std::string> faces);

// Create camera
Camera camera(glm::vec3(0.0f, 20.0f, 30.0f), glm::vec3(0.0f, 1.0f, 0.0f), -90, -40);

int main()
{
    RendererWindow main_window(&camera, "Renderer-Toy (v1.0.0)", WindowSize(window_width, window_height));
    // Create scene
    Scene* scene = new Scene(&main_window);
    main_window.AttatchObserver(&scene->render_pipeline);

    // Load Resources
    // ------------------------------------------------------------------------------------------------------------------------------
    std::filesystem::path tex_dir = FileSystem::FileSystem::GetContentPath() / "Textures";
    std::vector<std::string> str_to_find{ "Normal", "normal", "NORMAL" };
    for (const auto& tex_path_entry : std::filesystem::directory_iterator(tex_dir))
    {
        ETexType tex_format = ETexType::SRGBA;
        std::filesystem::path tex_path = tex_path_entry.path();
        std::string tex_path_str = tex_path.string();
        std::string tex_name = tex_path.stem().string();
        for (const auto& str : str_to_find)
        {
            if (tex_name.find(str) != std::string::npos)
            {
                tex_format = ETexType::RGBA;
                break;
            }
        }
        Texture2D* temp_tex = new Texture2D(tex_path_str, tex_format, true);
        EditorContent::editor_tex.insert({ tex_name, temp_tex });
    }
    // custom texture
    std::filesystem::path custom_tex_dir = tex_dir / "custom";
    for (const auto& tex_path_entry : std::filesystem::directory_iterator(custom_tex_dir))
    {
        ETexType tex_format = ETexType::SRGBA;
        std::filesystem::path custom_tex_path = tex_path_entry.path();
        std::string custom_tex_path_str = custom_tex_path.string();
        std::string custom_tex_name = custom_tex_path.stem().string();
        for (const auto & str : str_to_find)
        {
            if (custom_tex_name.find(str) != std::string::npos)
            {
                tex_format = ETexType::RGBA;
                break;
            }
        }
        Texture2D* temp_tex = new Texture2D(custom_tex_path_str, tex_format, true);
        EditorContent::editor_tex.insert({ custom_tex_name, temp_tex });
    }

    Texture2D *folder_ico   = new Texture2D(FileSystem::FileSystem::GetContentPath() / "editor/ico/folder_ico.png", ETexType::SRGBA, true);
    Texture2D *file_ico     = new Texture2D(FileSystem::FileSystem::GetContentPath() / "editor/ico/file_ico.png", ETexType::SRGBA, true);
    EditorContent::editor_tex.insert({"folder_ico", folder_ico});
    EditorContent::editor_tex.insert({"file_ico", file_ico});

    Model *M_sphere     = new Model(FileSystem::FileSystem::GetContentPath() / "Models/sphere.fbx");
    Model *M_cube       = new Model(FileSystem::FileSystem::GetContentPath() / "Models/cube.fbx");

    // build and compile our shader program
    // ------------------------------------
    Shader* default_shader = new Shader(    FileSystem::GetContentPath() / "Shader/default.vs",
                                            FileSystem::GetContentPath() / "Shader/default.fs",
                                            true);

    Shader *color_shader    = new Shader(   FileSystem::GetContentPath() / "Shader/color.vs",
                                            FileSystem::GetContentPath() / "Shader/color.fs",
                                            true);
    Shader *PBR_shader      = new Shader(   FileSystem::GetContentPath() / "Shader/default.vs",
                                            FileSystem::GetContentPath() / "Shader/PBR.fs",
                                            true);
    Shader *gamma_correcting_shader   = new Shader(   FileSystem::GetContentPath() / "Shader/framebuffer.vs",
                                            FileSystem::GetContentPath() / "Shader/gamma_correcting_frame.fs",
                                            true);
    Shader *inverse_shader  = new Shader(   FileSystem::GetContentPath() / "Shader/framebuffer.vs",
                                            FileSystem::GetContentPath() / "Shader/inverse_color.fs",
                                            true);
    Shader *blur_shader     = new Shader(   FileSystem::GetContentPath() / "Shader/framebuffer.vs",
                                            FileSystem::GetContentPath() / "Shader/blur.fs",
                                            true);

    Shader *bloom_shader     = new Shader(  FileSystem::GetContentPath() / "Shader/framebuffer.vs",
                                            FileSystem::GetContentPath() / "Shader/bloom.fs",
                                            true);

    //customed Shaders
    Shader* phong_shader = new Shader(  FileSystem::GetContentPath() / "Shader/custom/phong.vs",
                                        FileSystem::GetContentPath() / "Shader/custom/phong.fs",
                                        true);
    Shader* blinn_phong_shader = new Shader(FileSystem::GetContentPath() / "Shader/custom/blinn_phong.vs",
                                            FileSystem::GetContentPath() / "Shader/custom/blinn_phong.fs",
                                            true);
    Shader* cook_torrance_shader = new Shader(  FileSystem::GetContentPath() / "Shader/custom/cook_torrance.vs",
                                                FileSystem::GetContentPath() / "Shader/custom/cook_torrance.fs",
                                                true);


    // default_shader->LoadShader();
    color_shader->LoadShader();
    PBR_shader->LoadShader();
    gamma_correcting_shader->LoadShader();
    inverse_shader->LoadShader();
    blur_shader->LoadShader();
    bloom_shader->LoadShader();
    default_shader->LoadShader();
    phong_shader->LoadShader();
    blinn_phong_shader->LoadShader();
    cook_torrance_shader->LoadShader();

    // Create a post process manager
    PostProcessManager* ppm = new PostProcessManager(main_window.Width(), main_window.Height(), scene->render_pipeline.depth_texture);
    scene->RegisterSceneObject(ppm);
    // Assign postprocess manager to scene's renderer pipeline
    scene->render_pipeline.postprocess_manager = ppm;

    // Post process for test
    ppm->AddPostProcess( ppm->CreatePostProcess<PostProcess>( inverse_shader, "inverse", false));
    ppm->AddPostProcess( ppm->CreatePostProcess<PostProcess>( blur_shader, "Blur", false ));
    // ppm->AddPostProcess( ppm->CreatePostProcess<PostProcess>( raymarching_shader, "RayMarching", false ));
    ppm->AddPostProcess( ppm->CreatePostProcess<BloomProcess>( bloom_shader, "Bloom", false ));
    // Add a gamma correct post process
    ppm->AddPostProcess( ppm->CreatePostProcess<PostProcess>( gamma_correcting_shader, "Gamma correction" ));

    // Render loop
    // -----------
    while (!glfwWindowShouldClose(main_window.Window))
    {
        currentFrame = static_cast<float>(glfwGetTime());
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;
        // input
        // -----
        InputInfo::GetInstance()->Update();
        Input::processInput(main_window.Window, camera, deltaTime);

        focused = InputInfo::GetInstance()->mouse_button_right;
        if (focused)
        {
            camera.ProcessMouseMovement(InputInfo::GetInstance()->mouse_offset_x,
                                        InputInfo::GetInstance()->mouse_offset_y);
        }
        // Render
        // ------
        scene->render_pipeline.clear_color = main_window.clear_color;
        scene->RenderScene();

        // glfw: swap buffers and poll IO events (keys pressed/released, mouse moved etc.)
        // --------------------------------- ----------------------------------------------
        glfwPollEvents();

        // Start the Dear ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // Render UI
        main_window.imgui->RenderAll(&main_window, scene);
        ImGui::Render();

        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(main_window.Window);
    }

    // glfw: terminate, clearing all previously allocated GLFW resources.
    // ------------------------------------------------------------------
    main_window.terminate_window();

    return 0;
}