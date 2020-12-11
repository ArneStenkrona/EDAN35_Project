// Do not use intrinsic functions, which allows using `constexpr` on GLM
// functions.
#define GLM_FORCE_PURE 1

#include "project.hpp"

#include "config.hpp"
#include "core/Bonobo.h"
#include "core/FPSCamera.h"
#include "core/GLStateInspection.h"
#include "core/GLStateInspectionView.h"
#include "core/helpers.hpp"
#include "core/node.hpp"
#include "core/opengl.hpp"
#include "core/ShaderProgramManager.hpp"

#include "Common/parametric_shapes.hpp"

#include <imgui.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <tinyfiledialogs.h>

#include <array>
#include <clocale>
#include <cstdlib>
#include <stdexcept>

namespace constant
{
    constexpr uint32_t light_texture_res_x = 2048;
    constexpr uint32_t light_texture_res_y = 2048;

    constexpr uint32_t heightmap_res_x = 4096;
    constexpr uint32_t heightmap_res_y = 4096;

    constexpr float scale_lengths = 1.0f; // The scene is expressed in metres, hence the x1.

    const float shadow_width_half = 10.0f;
    const float shadow_depth_half = 7.0f;


    constexpr float  light_intensity = 72.0f;

    struct Wave {
        float Amplitude;
        float Frequency;
        float Phase;
        float Sharpness;
        glm::vec2 Direction;

        glm::vec2 padding;

        Wave(float A, float f, float phase, float sharpness, float dx, float dz) 
            : Amplitude(A), Frequency(f), Phase(phase), Sharpness(sharpness), Direction(dx, dz), padding(0,0) {};
    };

    Wave waveOne(0.2, 2 * 10 * 2, 0.5, 2.0, -1.0 , 0.0 );
    Wave waveTwo(0.1, 4 * 10 *  2, 1.3, 2.0, -0.7 , 0.7 );

    const glm::vec3 underwaterColour = glm::vec3(0.400, 0.900, 1.000);
    const glm::vec3 atmosphereColour = glm::vec3(0.529, 0.808, 0.922);

    const float MAMSL = 2.0; // Meter above mean sea level. (M.ö.h)
}

static bonobo::mesh_data loadCone();

project::Project::Project(WindowManager& windowManager) :
    mCamera(0.5f * glm::half_pi<float>(),
        static_cast<float>(config::resolution_x) / static_cast<float>(config::resolution_y),
        0.01f * constant::scale_lengths, 60.0f * constant::scale_lengths),
    inputHandler(), mWindowManager(windowManager), window(nullptr)
{
    WindowManager::WindowDatum window_datum{ inputHandler, mCamera, config::resolution_x, config::resolution_y, 0, 0, 0, 0 };

    window = mWindowManager.CreateGLFWWindow("Project: Caustics", window_datum, config::msaa_rate);
    if (window == nullptr) {
        throw std::runtime_error("Failed to get a window: aborting!");
    }

    GLStateInspection::Init();
    GLStateInspection::View::Init();

    bonobo::init();
}

project::Project::~Project()
{
    bonobo::deinit();

    GLStateInspection::View::Destroy();
    GLStateInspection::Destroy();
}

void
project::Project::run()
{
    const std::vector<bonobo::mesh_data> water = { parametric_shapes::createQuad(20, 20, 1000, 1000) };
	if (water.empty()) {
		LogError("Failed to load the water model");
		return;
	}

	auto const floor = bonobo::loadObjects(config::resources_path("models/floor/floor.obj"));
	if (floor.empty()) {
		LogError("Failed to load the floor model");
		return;
	}
	auto const ball = bonobo::loadObjects(config::resources_path("models/ball/ball.obj"));
	if (ball.empty()) {
		LogError("Failed to load the ball model");
		return;
	}

	std::vector<std::vector<bonobo::mesh_data>> solid_objects = { floor, ball };
    std::vector<std::vector<bonobo::mesh_data>> trans_objects = { water };

	std::vector<glm::vec3> solid_translations = { { 0, -3.0f * constant::scale_lengths, 0}, { 0.0f, -1.0f * constant::scale_lengths, 0.0f } };
    std::vector<glm::vec3> trans_translations = { { 0.0f, constant::MAMSL * constant::scale_lengths, 0.0f },};

    std::vector<Node> solids;
    for (size_t i = 0; i < solid_objects.size(); ++i) {
		for (size_t j = 0; j < solid_objects[i].size(); ++j) {
			Node node;
			node.get_transform().SetTranslate(solid_translations[i]);
            node.get_transform().Scale(constant::scale_lengths);
			node.set_geometry(solid_objects[i][j]);
            solids.push_back(node);
		}
    }

    std::vector<Node> transparents;
    for (size_t i = 0; i < trans_objects.size(); ++i) {
        for (size_t j = 0; j < trans_objects[i].size(); ++j) {
            Node node;
            node.get_transform().SetTranslate(trans_translations[i]);
            node.get_transform().Scale(constant::scale_lengths);
            node.set_geometry(trans_objects[i][j]);
            transparents.push_back(node);
        }
    }

    auto const ortho_box = parametric_shapes::createCube(1.0);
    Node box;
    box.set_geometry(ortho_box);

    //
    // Setup the camera
    //
    mCamera.mWorld.SetTranslate(glm::vec3(0.0f, 1.0f, 1.8f) * constant::scale_lengths);
    mCamera.mMouseSensitivity = 0.003f;
    mCamera.mMovementSpeed = 3.0f * constant::scale_lengths; // 3 m/s => 10.8 km/h.

    //
    // Load all the shader programs used
    //
    ShaderProgramManager program_manager;
    GLuint fallback_shader = 0u;
    program_manager.CreateAndRegisterProgram("Fallback",
        { { ShaderType::vertex, "EDAF80/fallback.vert" },
          { ShaderType::fragment, "EDAF80/fallback.frag" } },
        fallback_shader);
    if (fallback_shader == 0u) {
        LogError("Failed to load fallback shader");
        return;
    }

    GLuint fill_heightmap_shader = 0u; 
    program_manager.CreateAndRegisterProgram("Fill Heightmap",
        { { ShaderType::vertex, "Project/fill_heightmap.vert" },
          { ShaderType::fragment, "Project/fill_heightmap.frag" } },
        fill_heightmap_shader);
    if (fill_heightmap_shader == 0u) {
        LogError("Failed to load heightmap filling shader");
        return;
    }

    GLuint fill_shadowmap_shader = 0u;
    program_manager.CreateAndRegisterProgram("Fill shadow map",
        { { ShaderType::vertex, "EDAN35/fill_shadowmap.vert" },
          { ShaderType::fragment, "EDAN35/fill_shadowmap.frag" } },
        fill_shadowmap_shader);
    if (fill_shadowmap_shader == 0u) {
        LogError("Failed to load shadowmap filling shader");
        return;
    }

    GLuint fill_environmentmap_shader = 0u;
    program_manager.CreateAndRegisterProgram("Fill environment map",
        { { ShaderType::vertex, "Project/fill_environmentmap.vert" },
          { ShaderType::fragment, "Project/fill_environmentmap.frag" } },
        fill_environmentmap_shader);
    if (fill_environmentmap_shader == 0u) {
        LogError("Failed to load environment-map filling shader");
        return;
    }

    GLuint fill_causticmap_shader = 0u;
    program_manager.CreateAndRegisterProgram("Fill caustic map",
        { { ShaderType::vertex, "Project/fill_causticmap.vert" },
          { ShaderType::fragment, "Project/fill_causticmap.frag" } },
        fill_causticmap_shader);
    if (fill_causticmap_shader == 0u) {
        LogError("Failed to load caustic-map filling shader");
        return;
    }

    GLuint render_underwater = 0u;
    program_manager.CreateAndRegisterProgram("Underwater",
        { { ShaderType::vertex, "Project/underwater.vert" },
          { ShaderType::fragment, "Project/underwater.frag" } },
        render_underwater);
    if (render_underwater == 0u) {
        LogError("Failed to load resolve shader");
        return;
    }

    GLuint render_water = 0u;
    program_manager.CreateAndRegisterProgram("Water",
        { { ShaderType::vertex, "Project/water.vert" },
          { ShaderType::fragment, "Project/water.frag" } },
        render_water);
    if (render_water == 0u) {
        LogError("Failed to load resolve shader");
        return;
    }

    GLuint render_light_cones_shader = 0u;
    program_manager.CreateAndRegisterProgram("Render light box",
        { { ShaderType::vertex, "EDAN35/render_light_cones.vert" },
          { ShaderType::fragment, "EDAN35/render_light_cones.frag" } },
        render_light_cones_shader);
    if (render_light_cones_shader == 0u) {
        LogError("Failed to load light box rendering shader");
        return;
    }

    auto const no_extra_uniforms = [](GLuint /*program*/) {};

    int framebuffer_width, framebuffer_height;
    glfwGetFramebufferSize(window, &framebuffer_width, &framebuffer_height);

    //
    // Setup textures
    //

    auto const underwater_scene_texture = bonobo::createTexture(framebuffer_width, framebuffer_height);

    auto const shadowmap_texture = bonobo::createTexture(constant::light_texture_res_x, constant::light_texture_res_y,
        GL_TEXTURE_2D, GL_DEPTH_COMPONENT32F, GL_DEPTH_COMPONENT, GL_FLOAT);
    auto const environmentmap_texture = bonobo::createTexture(constant::light_texture_res_x, constant::light_texture_res_y,
        GL_TEXTURE_2D, GL_RGBA32F);
    auto const causticmap_texture = bonobo::createTexture(constant::light_texture_res_x, constant::light_texture_res_y 
        /*, GL_TEXTURE_2D, GL_R8*/);
    auto const depth_texture = bonobo::createTexture(framebuffer_width, framebuffer_height,
        GL_TEXTURE_2D, GL_DEPTH_COMPONENT32F, GL_DEPTH_COMPONENT, GL_FLOAT);
    auto const heightmap_texture = bonobo::createTexture(constant::heightmap_res_x, constant::heightmap_res_y,
        GL_TEXTURE_2D, GL_RGBA32F);

    //
    // Setup FBOs
    //
    auto const heightmap_fbo = bonobo::createFBO({ heightmap_texture });
    auto const shadowmap_fbo = bonobo::createFBO({}, shadowmap_texture);
    auto const environmentmap_fbo = bonobo::createFBO({ environmentmap_texture });
    auto const causticmap_fbo = bonobo::createFBO({ causticmap_texture });
    auto const underwater_scene_fbo = bonobo::createFBO({ underwater_scene_texture }, depth_texture);
    //
    // Setup samplers
    //
    auto const default_sampler = bonobo::createSampler([](GLuint sampler) {
        glSamplerParameteri(sampler, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glSamplerParameteri(sampler, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glSamplerParameteri(sampler, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glSamplerParameteri(sampler, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        });

    auto const depth_sampler = bonobo::createSampler([](GLuint sampler) {
        glSamplerParameteri(sampler, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glSamplerParameteri(sampler, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glSamplerParameteri(sampler, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glSamplerParameteri(sampler, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        });

    auto const shadow_sampler = bonobo::createSampler([](GLuint sampler) {
        glSamplerParameteri(sampler, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glSamplerParameteri(sampler, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glSamplerParameteri(sampler, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glSamplerParameteri(sampler, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glSamplerParameteri(sampler, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
        glSamplerParameteri(sampler, GL_TEXTURE_COMPARE_FUNC, GL_LESS);
        GLfloat border_color[4] = { 1.0f, 0.0f, 0.0f, 0.0f };
        glSamplerParameterfv(sampler, GL_TEXTURE_BORDER_COLOR, border_color);
        });

    auto const heightmap_sampler = bonobo::createSampler([](GLuint sampler) {
        glSamplerParameteri(sampler, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glSamplerParameteri(sampler, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glSamplerParameteri(sampler, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glSamplerParameteri(sampler, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    });

    auto const caustics_sampler = bonobo::createSampler([](GLuint sampler) {
        glSamplerParameteri(sampler, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glSamplerParameteri(sampler, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glSamplerParameteri(sampler, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glSamplerParameteri(sampler, GL_TEXTURE_WRAP_T, GL_REPEAT);
    });

    auto const bind_texture_with_sampler = [](GLenum target, unsigned int slot, GLuint program, std::string const& name, GLuint texture, GLuint sampler) {
        glActiveTexture(GL_TEXTURE0 + slot);
        glBindTexture(target, texture);
        glUniform1i(glGetUniformLocation(program, name.c_str()), static_cast<GLint>(slot));
        glBindSampler(slot, sampler);
    };

    //
    // Setup lights properties
    //
    bool are_lights_paused = false;

	const glm::vec3 sunDir{ 0.0f, -1.0f, 0.0f };
	const glm::vec3 sunColor{ 1.0f, 1.0f, 1.0f };

	float const left = -constant::shadow_width_half * constant::scale_lengths;
	float const right = constant::shadow_width_half * constant::scale_lengths;
	float const top = -constant::shadow_width_half * constant::scale_lengths;
	float const bot = constant::shadow_width_half * constant::scale_lengths;

    float const lightProjectionNearPlane = -constant::shadow_depth_half * constant::scale_lengths;
    float const lightProjectionFarPlane = constant::shadow_depth_half * constant::scale_lengths;


	TRSTransformf lightTransform;
    lightTransform.SetTranslate(-sunDir);

    // UP is z as up is seen from the perspective of an image on the XY plane
    glm::vec3 safeUp = glm::dot(-sunDir, glm::vec3{0.0f, 1.0f, 0.0f}) > 0.99f ? glm::normalize(glm::vec3{0.0f, 1.0f, 1.0f}) : glm::vec3{0.0f, 1.0f, 0.0f};
    lightTransform.LookAt(glm::vec3{ 0.0f,0.0f,0.0f }, safeUp);

	auto lightProjection = glm::ortho(left, right, top, bot, lightProjectionNearPlane, lightProjectionFarPlane);

    glm::mat4 boxScale = glm::scale(glm::mat4(1.0f), glm::vec3(right-left, bot - top, lightProjectionFarPlane - lightProjectionNearPlane));

    auto seconds_nb = 0.0f;

    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClearDepthf(1.0f);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);

    auto lastTime = std::chrono::high_resolution_clock::now();
    bool show_textures = true;
    bool show_cone_wireframe = false;

    bool show_logs = true;
    bool show_gui = true;
    bool shader_reload_failed = false;

    while (!glfwWindowShouldClose(window)) {
        auto const nowTime = std::chrono::high_resolution_clock::now();
        auto const deltaTimeUs = std::chrono::duration_cast<std::chrono::microseconds>(nowTime - lastTime);
        lastTime = nowTime;
        if (!are_lights_paused)
            seconds_nb += std::chrono::duration<decltype(seconds_nb)>(deltaTimeUs).count();

        auto& io = ImGui::GetIO();
        inputHandler.SetUICapture(io.WantCaptureMouse, io.WantCaptureKeyboard);

        glfwPollEvents();
        inputHandler.Advance();
        mCamera.Update(deltaTimeUs, inputHandler);

        if (inputHandler.GetKeycodeState(GLFW_KEY_R) & JUST_PRESSED) {
            shader_reload_failed = !program_manager.ReloadAllPrograms();
            if (shader_reload_failed)
                tinyfd_notifyPopup("Shader Program Reload Error",
                    "An error occurred while reloading shader programs; see the logs for details.\n"
                    "Rendering is suspended until the issue is solved. Once fixed, just reload the shaders again.",
                    "error");
        }
        if (inputHandler.GetKeycodeState(GLFW_KEY_F3) & JUST_RELEASED)
            show_logs = !show_logs;
        if (inputHandler.GetKeycodeState(GLFW_KEY_F2) & JUST_RELEASED)
            show_gui = !show_gui;

        mWindowManager.NewImGuiFrame();

        if (mCamera.mWorld.GetTranslation().y >= constant::MAMSL * constant::scale_lengths)
        {
            glClearColor(constant::atmosphereColour.x,
                constant::atmosphereColour.y, constant::atmosphereColour.z, 1.0f);
        }
        else
        {
            glClearColor(constant::underwaterColour.x,
                constant::underwaterColour.y, constant::underwaterColour.z, 1.0f);
        }

#if 0
        glm::vec3 playerPos = mCamera.mWorld.GetTranslation();
        playerPos.y = 0;
        lightTransform.SetTranslate(playerPos);
#endif 

        if (!shader_reload_failed) {

            /* RENDER DIRECTIONAL LIGHT */
            auto light_matrix = lightProjection * lightTransform.GetMatrixInverse();

            //
            // Pass 1: Render heightmap 
            //
            glCullFace(GL_BACK);
            if (utils::opengl::debug::isSupported())
            {
                std::string const group_name = "Heightmap Generation";
                glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0u, group_name.size(), group_name.data());
            }

            auto const build_heightmap_uniforms = [&seconds_nb, this](GLuint program) {
                // time uniform
                glUniform1f(glGetUniformLocation(program, "time"), seconds_nb);

                // Wave 1
                glUniform1f(glGetUniformLocation(program, "wave1.Amplitude"), constant::waveOne.Amplitude);
                glUniform1f(glGetUniformLocation(program, "wave1.Frequency"), constant::waveOne.Frequency);
                glUniform1f(glGetUniformLocation(program, "wave1.Phase"), constant::waveOne.Phase);
                glUniform1f(glGetUniformLocation(program, "wave1.Sharpness"), constant::waveOne.Sharpness);
                glUniform2fv(glGetUniformLocation(program, "wave1.Direction"), 1, glm::value_ptr(constant::waveOne.Direction));

                // Wave 2
                glUniform1f(glGetUniformLocation(program, "wave2.Amplitude"), constant::waveTwo.Amplitude);
                glUniform1f(glGetUniformLocation(program, "wave2.Frequency"), constant::waveTwo.Frequency);
                glUniform1f(glGetUniformLocation(program, "wave2.Phase"), constant::waveTwo.Phase);
                glUniform1f(glGetUniformLocation(program, "wave2.Sharpness"), constant::waveTwo.Sharpness);
                glUniform2fv(glGetUniformLocation(program, "wave2.Direction"), 1, glm::value_ptr(constant::waveTwo.Direction));
            };

            glBindFramebuffer(GL_FRAMEBUFFER, heightmap_fbo);
            GLenum const heightmap_draw_buffers[1] = { GL_COLOR_ATTACHMENT0 };
            glDrawBuffers(1, heightmap_draw_buffers);
            auto status_env = glCheckFramebufferStatus(GL_FRAMEBUFFER);
            if (status_env != GL_FRAMEBUFFER_COMPLETE)
                LogError("Something went wrong with framebuffer %u", heightmap_fbo);
            glViewport(0, 0, constant::heightmap_res_x, constant::heightmap_res_y);

            glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);

            GLStateInspection::CaptureSnapshot("Heightmap Generation Pass");

            glUseProgram(fill_heightmap_shader);
            build_heightmap_uniforms(fill_heightmap_shader);
            bonobo::drawFullscreen();

            if (utils::opengl::debug::isSupported())
            {
                glPopDebugGroup();
            }

            //
            // Pass 2.0: Generate shadow map for sun
            //

            glCullFace(GL_FRONT);

            if (utils::opengl::debug::isSupported())
            {
                std::string const group_name = "Create shadow map Sun";
                glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0u, group_name.size(), group_name.data());
            }
            glBindFramebuffer(GL_FRAMEBUFFER, shadowmap_fbo);
            glViewport(0, 0, constant::light_texture_res_x, constant::light_texture_res_y);
			glClear(GL_DEPTH_BUFFER_BIT);

            GLStateInspection::CaptureSnapshot("Shadow Map Generation");

            for (auto const& element : solids)
                element.render(light_matrix, element.get_transform().GetMatrix(), fill_shadowmap_shader, no_extra_uniforms);

            if (utils::opengl::debug::isSupported())
            {
                glPopDebugGroup();
            }
            

            //
            // Pass 3.0: Generate environment map for sun
            //
            glCullFace(GL_BACK);
            if (utils::opengl::debug::isSupported())
            {
                std::string const group_name = "Create environment map Sun";
                glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0u, group_name.size(), group_name.data());
            }

            glBindFramebuffer(GL_FRAMEBUFFER, environmentmap_fbo);
            GLenum const environment_draw_buffers[1] = { GL_COLOR_ATTACHMENT0 };
            glDrawBuffers(1, environment_draw_buffers);
            status_env = glCheckFramebufferStatus(GL_FRAMEBUFFER);
            if (status_env != GL_FRAMEBUFFER_COMPLETE)
                LogError("Something went wrong with framebuffer %u", environmentmap_fbo);
            glViewport(0, 0, constant::light_texture_res_x, constant::light_texture_res_y);

            glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);

            GLStateInspection::CaptureSnapshot("Filling Pass");

            for (auto const& element : solids)
                element.render(light_matrix, element.get_transform().GetMatrix(), fill_environmentmap_shader, no_extra_uniforms);
            if (utils::opengl::debug::isSupported())
            {
                glPopDebugGroup();
            }

            //
            // Pass 3.1: Generate caustic map for sun
            //
            glCullFace(GL_BACK);
            if (utils::opengl::debug::isSupported())
            {
                std::string const group_name = "Create caustic map Sun";
                glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0u, group_name.size(), group_name.data());
            }

            glBindFramebuffer(GL_FRAMEBUFFER, causticmap_fbo);
            GLenum const caustic_draw_buffers[1] = { GL_COLOR_ATTACHMENT0 };
            glDrawBuffers(1, caustic_draw_buffers);
            status_env = glCheckFramebufferStatus(GL_FRAMEBUFFER);
            if (status_env != GL_FRAMEBUFFER_COMPLETE)
                LogError("Something went wrong with framebuffer %u", causticmap_fbo);
            glViewport(0, 0, constant::light_texture_res_x, constant::light_texture_res_y);

            glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);

            GLStateInspection::CaptureSnapshot("Filling Pass");
            
            auto const caustic_set_uniforms = [&sunColor, &sunDir, &seconds_nb](GLuint program) {
                glUniform2f(glGetUniformLocation(program, "inv_res"),
                    1.0f / static_cast<float>(constant::light_texture_res_x),
                    1.0f / static_cast<float>(constant::light_texture_res_y));
                glUniform3fv(glGetUniformLocation(program, "light_color"), 1, glm::value_ptr(sunColor));
                glUniform3fv(glGetUniformLocation(program, "light_direction"), 1, glm::value_ptr(sunDir));
                glUniform2f(glGetUniformLocation(program, "environmentmap_texel_size"),
                    1.0f / static_cast<float>(constant::light_texture_res_x),
                    1.0f / static_cast<float>(constant::light_texture_res_y));
            };

            glUseProgram(fill_causticmap_shader);
            bind_texture_with_sampler(GL_TEXTURE_2D, 0, fill_causticmap_shader, "environmentmap_texture", environmentmap_texture, default_sampler);
            bind_texture_with_sampler(GL_TEXTURE_2D, 1, fill_causticmap_shader, "heightmap_texture", heightmap_texture, heightmap_sampler);


            for (auto & element : transparents) 
                element.render(light_matrix, element.get_transform().GetMatrix(), fill_causticmap_shader, false, caustic_set_uniforms);
            if (utils::opengl::debug::isSupported())
            {
                glPopDebugGroup();
            }

            //
            // Pass 4: Render underwater scene
            //
            glCullFace(GL_BACK);
            glDepthFunc(GL_LESS);

            if (utils::opengl::debug::isSupported())
            {
                std::string const group_name = "Resolve scene";
                glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0u, group_name.size(), group_name.data());
            }

            glBindFramebuffer(GL_FRAMEBUFFER, underwater_scene_fbo);
            GLenum const underwater_draw_buffers[1] = { GL_COLOR_ATTACHMENT0 };
            glDrawBuffers(1, underwater_draw_buffers);
            status_env = glCheckFramebufferStatus(GL_FRAMEBUFFER);
            if (status_env != GL_FRAMEBUFFER_COMPLETE)
                LogError("Something went wrong with framebuffer %u", underwater_scene_fbo);

            glViewport(0, 0, framebuffer_width, framebuffer_height);
            glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);

            GLStateInspection::CaptureSnapshot("underwater Pass");

            auto const resolve_uniforms = [&sunColor, &sunDir, &seconds_nb, this, &light_matrix, &framebuffer_width, &framebuffer_height](GLuint program) {
                // COMMON
                glUniformMatrix4fv(glGetUniformLocation(program, "view_projection_inverse"), 1, GL_FALSE,
                    glm::value_ptr(mCamera.GetClipToWorldMatrix()));
                glUniform3fv(glGetUniformLocation(program, "camera_position"), 1,
                    glm::value_ptr(mCamera.mWorld.GetTranslation()));
                glUniformMatrix4fv(glGetUniformLocation(program, "shadow_view_projection"), 1, GL_FALSE,
                    glm::value_ptr(light_matrix));
                glUniform3fv(glGetUniformLocation(program, "sun_dir"), 1,
                    glm::value_ptr(sunDir));
                glUniform2f(glGetUniformLocation(program, "shadowmap_texel_size"),
                    1.0f / static_cast<float>(constant::light_texture_res_x),
                    1.0f / static_cast<float>(constant::light_texture_res_y));
                glUniform2f(glGetUniformLocation(program, "inv_res"),
                    1.0f / static_cast<float>(framebuffer_width),
                    1.0f / static_cast<float>(framebuffer_height));
                glUniform3fv(glGetUniformLocation(program, "atmosphereColour"), 1,
                    glm::value_ptr(constant::atmosphereColour));
                glUniform3fv(glGetUniformLocation(program, "underwaterColour"), 1,
                    glm::value_ptr(constant::underwaterColour));
                glUniform1f(glGetUniformLocation(program, "MAMSL"),
                    constant::MAMSL);
            };

            glUseProgram(render_underwater);
            bind_texture_with_sampler(GL_TEXTURE_2D, 5, render_underwater, "shadow_texture", shadowmap_texture, shadow_sampler);
            bind_texture_with_sampler(GL_TEXTURE_2D, 6, render_underwater, "causticmap_texture", causticmap_texture, caustics_sampler);

            for (auto const& element : solids)
                element.render(mCamera.GetWorldToClipMatrix(), element.get_transform().GetMatrix(), render_underwater, resolve_uniforms);


            //
            //  Pass 5: Render final scene
            //
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            glViewport(0, 0, framebuffer_width, framebuffer_height);
            glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);

            GLStateInspection::CaptureSnapshot("Final render Pass");

            glUseProgram(render_underwater);
            bind_texture_with_sampler(GL_TEXTURE_2D, 5, render_underwater, "shadow_texture", shadowmap_texture, shadow_sampler);
            bind_texture_with_sampler(GL_TEXTURE_2D, 6, render_underwater, "causticmap_texture", causticmap_texture, caustics_sampler);

            for (auto const& element : solids)
                element.render(mCamera.GetWorldToClipMatrix(), element.get_transform().GetMatrix(), render_underwater, resolve_uniforms);



            glUseProgram(render_water);
            bind_texture_with_sampler(GL_TEXTURE_2D, 5, render_water, "heightmap_texture", heightmap_texture, heightmap_sampler);
            bind_texture_with_sampler(GL_TEXTURE_2D, 6, render_water, "underwater_texture", underwater_scene_texture, default_sampler);

            glCullFace(GL_FRONT);
            for (auto const& element : transparents)
                element.render(mCamera.GetWorldToClipMatrix(), element.get_transform().GetMatrix(), render_water, resolve_uniforms);
            glCullFace(GL_BACK);

            for (auto const& element : transparents)
                element.render(mCamera.GetWorldToClipMatrix(), element.get_transform().GetMatrix(), render_water, resolve_uniforms);

            if (utils::opengl::debug::isSupported())
            {
                glPopDebugGroup();
            }
        }

        glBindFramebuffer(GL_FRAMEBUFFER, 0u);
        //
        // Pass 5: Draw wireframe cones on top of the final image for debugging purposes
        //
        glDisable(GL_CULL_FACE);
        if (show_cone_wireframe) {
            glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
            box.render(mCamera.GetWorldToClipMatrix(), lightTransform.GetMatrix() * boxScale,
                render_light_cones_shader, no_extra_uniforms);
            glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        }
        glEnable(GL_CULL_FACE);

        //
        // Output content of the g-buffer as well as of the shadowmap, for debugging purposes
        //
        if (show_textures) {
            //bonobo::displayTexture({ 0.7f, 0.55f }, { 0.95f, 0.95f }, environmentmap_texture, default_sampler, { 0, 1, 2, -1 }, glm::uvec2(framebuffer_width, framebuffer_height), false);
            bonobo::displayTexture({ 0.7f, 0.55f }, { 0.95f, 0.95f }, heightmap_texture, heightmap_sampler, { 0, 1, 2, -1 }, glm::uvec2(framebuffer_width, framebuffer_height), false);
            bonobo::displayTexture({ 0.7f, 0.05f }, { 0.95f, 0.45f }, causticmap_texture, default_sampler, { 0, 1, 2, -1 }, glm::uvec2(framebuffer_width, framebuffer_height), false);
            bonobo::displayTexture({ 0.7f, -0.45f }, { 0.95f, -0.05f }, underwater_scene_texture, default_sampler, { 0, 1, 2, -1 }, glm::uvec2(framebuffer_width, framebuffer_height), false);
            bonobo::displayTexture({ 0.7f, -0.95f }, { 0.95f, -0.55f }, shadowmap_texture, depth_sampler, { 0, 0, 0, -1 }, glm::uvec2(framebuffer_width, framebuffer_height), false, lightProjectionNearPlane, lightProjectionFarPlane);
        }

        //
        // Reset viewport back to normal
        //
        glViewport(0, 0, framebuffer_width, framebuffer_height);

        GLStateInspection::View::Render();

        bool opened = ImGui::Begin("Render Time", nullptr, ImGuiWindowFlags_None);
        if (opened)
            ImGui::Text("%.3f ms", std::chrono::duration<float, std::milli>(deltaTimeUs).count());
        ImGui::End();

        opened = ImGui::Begin("Scene Controls", nullptr, ImGuiWindowFlags_None);
        if (opened) {
            ImGui::Checkbox("Pause lights", &are_lights_paused);
            //ImGui::SliderInt("Number of lights", &lights_nb, 1, static_cast<int>(constant::lights_nb));
            ImGui::Checkbox("Show textures", &show_textures);
            ImGui::Checkbox("Show light cones wireframe", &show_cone_wireframe);
        }
        ImGui::End();

        if (show_logs)
            Log::View::Render();
        if (show_gui)
            mWindowManager.RenderImGuiFrame();

        glfwSwapBuffers(window);
    }

    glDeleteProgram(render_underwater);
    render_underwater = 0u;
    glDeleteProgram(render_water);
    render_water = 0u;

    glDeleteProgram(fill_heightmap_shader);
    fill_heightmap_shader = 0u;
    glDeleteProgram(fill_shadowmap_shader);
    fill_shadowmap_shader = 0u;
    glDeleteProgram(fill_environmentmap_shader);
    fill_environmentmap_shader = 0u;
    glDeleteProgram(fill_causticmap_shader);
    fill_causticmap_shader = 0u;

    glDeleteProgram(fallback_shader);
    fallback_shader = 0u;
}

int main()
{
    std::setlocale(LC_ALL, "");

    Bonobo framework;

    try {
        project::Project project(framework.GetWindowManager());
        project.run();
    }
    catch (std::runtime_error const& e) {
        LogError(e.what());
    }
}
