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
    constexpr uint32_t shadowmap_res_x = 2048;
    constexpr uint32_t shadowmap_res_y = 2048;

    constexpr uint32_t environmentmap_res_x = 1024;
    constexpr uint32_t environmentmap_res_y = 1024;

    constexpr float  scale_lengths = 100.0f; // The scene is expressed in centimetres rather than metres, hence the x100.

    const float shadow_width_half = 10.0f;
    const float shadow_depth_half = 7.0f;

    constexpr float  light_intensity = 72.0f;

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

	// Load the geometry of Sponza
	//auto const sponza_geometry = bonobo::loadObjects(config::resources_path("sponza/sponza.obj"));
	//if (sponza_geometry.empty()) {
	//    LogError("Failed to load the Sponza model");
	//    return;
	//}
	//std::vector<Node> sponza_elements;
	//sponza_elements.reserve(sponza_geometry.size());
	//for (auto const& shape : sponza_geometry) {
	//    Node node;
	//    node.set_geometry(shape);
	//    sponza_elements.push_back(node);
	//}

	auto const water = bonobo::loadObjects(config::resources_path("models/water/water.obj"));
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

	std::vector<std::vector<bonobo::mesh_data>> solid_objects = { floor , ball };
    std::vector<std::vector<bonobo::mesh_data>> trans_objects = { water };

#if 0
	std::vector<bonobo::mesh_data> meshes = { parametric_shapes::createQuad(10 * constant::scale_lengths, 10 * constant::scale_lengths, 1, 1), /* floor */
											  parametric_shapes::createQuad(10 * constant::scale_lengths, 10 * constant::scale_lengths, 1, 1), /* water */
											  parametric_shapes::createSphere(16,32, 0.5 * constant::scale_lengths) /* beach ball */ };
#endif

	std::vector<glm::vec3> solid_translations = { { 0.0f, -1.0f * constant::scale_lengths, 0.0f }, /* floor */
											    { 0.0f, 1.0f * constant::scale_lengths, 0.0f } /* beach ball */ };
    std::vector<glm::vec3> trans_translations = { { 0.0f, 0.0f, 0.0f }, /* water */ };

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

    GLuint fill_gbuffer_shader = 0u;
    program_manager.CreateAndRegisterProgram("Fill G-Buffer",
        { { ShaderType::vertex, "EDAN35/fill_gbuffer.vert" },
          { ShaderType::fragment, "EDAN35/fill_gbuffer.frag" } },
        fill_gbuffer_shader);
    if (fill_gbuffer_shader == 0u) {
        LogError("Failed to load G-buffer filling shader");
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
        { { ShaderType::vertex, "EDAN35/fill_environmentmap.vert" },
          { ShaderType::fragment, "EDAN35/fill_environmentmap.frag" } },
        fill_environmentmap_shader);
    if (fill_environmentmap_shader == 0u) {
        LogError("Failed to load environmentmap filling shader");
        return;
    }

    GLuint accumulate_lights_shader = 0u;
    program_manager.CreateAndRegisterProgram("Accumulate light",
        { { ShaderType::vertex, "EDAN35/accumulate_lights.vert" },
          { ShaderType::fragment, "EDAN35/accumulate_lights.frag" } },
        accumulate_lights_shader);
    if (accumulate_lights_shader == 0u) {
        LogError("Failed to load lights accumulating shader");
        return;
    }

    GLuint resolve_deferred_shader = 0u;
    program_manager.CreateAndRegisterProgram("Resolve deferred",
        { { ShaderType::vertex, "EDAN35/resolve_deferred.vert" },
          { ShaderType::fragment, "EDAN35/resolve_deferred.frag" } },
        resolve_deferred_shader);
    if (resolve_deferred_shader == 0u) {
        LogError("Failed to load deferred resolution shader");
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

    auto const set_uniforms = [](GLuint /*program*/) {};

    int framebuffer_width, framebuffer_height;
    glfwGetFramebufferSize(window, &framebuffer_width, &framebuffer_height);

    //
    // Setup textures
    //
    auto const diffuse_texture = bonobo::createTexture(framebuffer_width, framebuffer_height);
    auto const specular_texture = bonobo::createTexture(framebuffer_width, framebuffer_height);
    auto const normal_texture = bonobo::createTexture(framebuffer_width, framebuffer_height);
    auto const light_diffuse_contribution_texture = bonobo::createTexture(framebuffer_width, framebuffer_height);
    auto const light_specular_contribution_texture = bonobo::createTexture(framebuffer_width, framebuffer_height);
    auto const depth_texture = bonobo::createTexture(framebuffer_width, framebuffer_height, GL_TEXTURE_2D, GL_DEPTH_COMPONENT32F, GL_DEPTH_COMPONENT, GL_FLOAT);
    auto const shadowmap_texture = bonobo::createTexture(constant::shadowmap_res_x, constant::shadowmap_res_y, GL_TEXTURE_2D, GL_DEPTH_COMPONENT32F, GL_DEPTH_COMPONENT, GL_FLOAT);
    auto const environmentmap_texture = bonobo::createTexture(constant::environmentmap_res_x, constant::environmentmap_res_y);


    //
    // Setup FBOs
    //
    auto const deferred_fbo = bonobo::createFBO({ diffuse_texture, specular_texture, normal_texture }, depth_texture);
    auto const shadowmap_fbo = bonobo::createFBO({}, shadowmap_texture);
    auto const environmentmap_fbo = bonobo::createFBO({environmentmap_texture});
    auto const light_fbo = bonobo::createFBO({ light_diffuse_contribution_texture, light_specular_contribution_texture }, depth_texture);

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
    auto const environment_sampler = bonobo::createSampler([](GLuint sampler) {
        glSamplerParameteri(sampler, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glSamplerParameteri(sampler, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glSamplerParameteri(sampler, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glSamplerParameteri(sampler, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
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


        if (!shader_reload_failed) {
            glDepthFunc(GL_LESS);
            //
            // Pass 1: Render scene into the g-buffer
            //
            if (utils::opengl::debug::isSupported())
            {
                std::string const group_name = "Fill G-buffer";
                glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0u, group_name.size(), group_name.data());
            }

            glBindFramebuffer(GL_FRAMEBUFFER, deferred_fbo);
            GLenum const deferred_draw_buffers[3] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2 };
            glDrawBuffers(3, deferred_draw_buffers);
            auto const status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
            if (status != GL_FRAMEBUFFER_COMPLETE)
                LogError("Something went wrong with framebuffer %u", deferred_fbo);
            int framebuffer_width, framebuffer_height;
            glfwGetFramebufferSize(window, &framebuffer_width, &framebuffer_height);
            glViewport(0, 0, framebuffer_width, framebuffer_height);
            glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);

            GLStateInspection::CaptureSnapshot("Filling Pass");

            for (auto const& element : solids)
                element.render(mCamera.GetWorldToClipMatrix(), element.get_transform().GetMatrix(), fill_gbuffer_shader, set_uniforms);
            if (utils::opengl::debug::isSupported())
            {
                glPopDebugGroup();
            }

            glCullFace(GL_FRONT);
            //
            // Pass 2: Generate shadowmaps and accumulate lights' contribution
            //
            glBindFramebuffer(GL_FRAMEBUFFER, light_fbo);
            GLenum light_draw_buffers[2] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 };
            glDrawBuffers(2, light_draw_buffers);
            glViewport(0, 0, framebuffer_width, framebuffer_height);
			glClear(GL_COLOR_BUFFER_BIT);

            /* RENDER DIRECTIONAL LIGHT */
            auto light_matrix = lightProjection * lightTransform.GetMatrixInverse();

            //
            // Pass 2.1: Generate shadow map for sun
            //
            if (utils::opengl::debug::isSupported())
            {
                std::string const group_name = "Create shadow map Sun";
                glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0u, group_name.size(), group_name.data());
            }
            glBindFramebuffer(GL_FRAMEBUFFER, shadowmap_fbo);
            glViewport(0, 0, constant::shadowmap_res_x, constant::shadowmap_res_y);
			glClear(GL_DEPTH_BUFFER_BIT);

            GLStateInspection::CaptureSnapshot("Shadow Map Generation");

            for (auto const& element : solids)
                element.render(light_matrix, element.get_transform().GetMatrix(), fill_shadowmap_shader, set_uniforms);
            if (utils::opengl::debug::isSupported())
            {
                glPopDebugGroup();
            }
            glEnable(GL_BLEND);
            glDepthFunc(GL_GREATER);
            glDepthMask(GL_FALSE);
            glBlendEquationSeparate(GL_FUNC_ADD, GL_MIN);
            glBlendFuncSeparate(GL_ONE, GL_ONE, GL_ONE, GL_ONE);
            //
            // Pass 2.2: Accumulate light i contribution
            if (utils::opengl::debug::isSupported())
            {
                std::string const group_name = "Accumulate light Sun";
                glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0u, group_name.size(), group_name.data());
            }
            glBindFramebuffer(GL_FRAMEBUFFER, light_fbo);
            glDrawBuffers(2, light_draw_buffers);
            glUseProgram(accumulate_lights_shader);
            glViewport(0, 0, framebuffer_width, framebuffer_height);

            auto const spotlight_set_uniforms = [framebuffer_width, framebuffer_height, this, &light_matrix, &sunColor, &sunDir](GLuint program) {
                glUniform2f(glGetUniformLocation(program, "inv_res"),
                    1.0f / static_cast<float>(framebuffer_width),
                    1.0f / static_cast<float>(framebuffer_height));
                glUniformMatrix4fv(glGetUniformLocation(program, "view_projection_inverse"), 1, GL_FALSE,
                    glm::value_ptr(mCamera.GetClipToWorldMatrix()));
                glUniform3fv(glGetUniformLocation(program, "camera_position"), 1,
                    glm::value_ptr(mCamera.mWorld.GetTranslation()));
                glUniformMatrix4fv(glGetUniformLocation(program, "shadow_view_projection"), 1, GL_FALSE,
                    glm::value_ptr(light_matrix));
                glUniform3fv(glGetUniformLocation(program, "light_color"), 1, glm::value_ptr(sunColor));
                //glUniform3fv(glGetUniformLocation(program, "light_position"), 1, glm::value_ptr(lightTransform.GetTranslation()));
                //glUniform3fv(glGetUniformLocation(program, "light_direction"), 1, glm::value_ptr(lightTransform.GetFront()));
                glUniform3fv(glGetUniformLocation(program, "light_direction"), 1, glm::value_ptr(sunDir));
                glUniform1f(glGetUniformLocation(program, "light_intensity"), constant::light_intensity);
                //glUniform1f(glGetUniformLocation(program, "light_angle_falloff"), constant::light_angle_falloff);
                glUniform2f(glGetUniformLocation(program, "shadowmap_texel_size"),
                    1.0f / static_cast<float>(constant::shadowmap_res_x),
                    1.0f / static_cast<float>(constant::shadowmap_res_y));
            };

            bind_texture_with_sampler(GL_TEXTURE_2D, 0, accumulate_lights_shader, "depth_texture", depth_texture, depth_sampler);
            bind_texture_with_sampler(GL_TEXTURE_2D, 1, accumulate_lights_shader, "normal_texture", normal_texture, default_sampler);
            bind_texture_with_sampler(GL_TEXTURE_2D, 2, accumulate_lights_shader, "shadow_texture", shadowmap_texture, shadow_sampler);

            GLStateInspection::CaptureSnapshot("Accumulating");


            box.render(mCamera.GetWorldToClipMatrix(), lightTransform.GetMatrix() * boxScale,
                accumulate_lights_shader, spotlight_set_uniforms);

            glBindSampler(2u, 0u);
            glBindSampler(1u, 0u);
            glBindSampler(0u, 0u);

            glDepthMask(GL_TRUE);
            glDepthFunc(GL_LESS);
            glDisable(GL_BLEND);
            if (utils::opengl::debug::isSupported())
            {
                glPopDebugGroup();
            }

            //
            // Pass 2.3: Generate environment map for sun
            //
            glCullFace(GL_BACK);
            if (utils::opengl::debug::isSupported())
            {
                std::string const group_name = "Create environment map Sun";
                glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0u, group_name.size(), group_name.data());
            }

            glBindFramebuffer(GL_FRAMEBUFFER, environmentmap_fbo);
            GLenum const environment_draw_buffers[1] = { GL_COLOR_ATTACHMENT0 };
            glDrawBuffers(1, deferred_draw_buffers);
            auto const status_env = glCheckFramebufferStatus(GL_FRAMEBUFFER);
            if (status_env != GL_FRAMEBUFFER_COMPLETE)
                LogError("Something went wrong with framebuffer %u", environmentmap_fbo);
            glfwGetFramebufferSize(window, &framebuffer_width, &framebuffer_height);
            glViewport(0, 0, framebuffer_width, framebuffer_height);
            glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);

            GLStateInspection::CaptureSnapshot("Filling Pass");

            for (auto const& element : solids)
                element.render(light_matrix, element.get_transform().GetMatrix(), fill_environmentmap_shader, set_uniforms);
            if (utils::opengl::debug::isSupported())
            {
                glPopDebugGroup();
            }
            //------------

            glCullFace(GL_BACK);
            glDepthFunc(GL_ALWAYS);
            //
            // Pass 3: Compute final image using both the g-buffer and  the light accumulation buffer
            //
            if (utils::opengl::debug::isSupported())
            {
                std::string const group_name = "Resolve";
                glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0u, group_name.size(), group_name.data());
            }
            glBindFramebuffer(GL_FRAMEBUFFER, 0u);
            glUseProgram(resolve_deferred_shader);
            glViewport(0, 0, framebuffer_width, framebuffer_height);
            // XXX: Is any clearing needed?

            bind_texture_with_sampler(GL_TEXTURE_2D, 0, resolve_deferred_shader, "diffuse_texture", diffuse_texture, default_sampler);
            bind_texture_with_sampler(GL_TEXTURE_2D, 1, resolve_deferred_shader, "specular_texture", specular_texture, default_sampler);
            bind_texture_with_sampler(GL_TEXTURE_2D, 2, resolve_deferred_shader, "light_d_texture", light_diffuse_contribution_texture, default_sampler);
            bind_texture_with_sampler(GL_TEXTURE_2D, 3, resolve_deferred_shader, "light_s_texture", light_specular_contribution_texture, default_sampler);

            GLStateInspection::CaptureSnapshot("Resolve Pass");

            bonobo::drawFullscreen();

            glBindSampler(3, 0u);
            glBindSampler(2, 0u);
            glBindSampler(1, 0u);
            glBindSampler(0, 0u);
            glUseProgram(0u);
            if (utils::opengl::debug::isSupported())
            {
                glPopDebugGroup();
            }
        }


        //
        // Pass 4: Draw wireframe cones on top of the final image for debugging purposes
        //
		
        if (show_cone_wireframe) {
            glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
            box.render(mCamera.GetWorldToClipMatrix(), lightTransform.GetMatrix() * boxScale,
                render_light_cones_shader, set_uniforms);
            glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        }


        //
        // Output content of the g-buffer as well as of the shadowmap, for debugging purposes
        //
        if (show_textures) {
            bonobo::displayTexture({ -0.95f, -0.95f }, { -0.55f, -0.55f }, diffuse_texture, default_sampler, { 0, 1, 2, -1 }, glm::uvec2(framebuffer_width, framebuffer_height));
            bonobo::displayTexture({ -0.45f, -0.95f }, { -0.05f, -0.55f }, specular_texture, default_sampler, { 0, 1, 2, -1 }, glm::uvec2(framebuffer_width, framebuffer_height));
            bonobo::displayTexture({ 0.05f, -0.95f }, { 0.45f, -0.55f }, normal_texture, default_sampler, { 0, 1, 2, -1 }, glm::uvec2(framebuffer_width, framebuffer_height));
            bonobo::displayTexture({ 0.55f, -0.95f }, { 0.95f, -0.55f }, depth_texture, default_sampler, { 0, 0, 0, -1 }, glm::uvec2(framebuffer_width, framebuffer_height), true, mCamera.mNear, mCamera.mFar);
            bonobo::displayTexture({ -0.95f,  0.55f }, { -0.55f,  0.95f }, shadowmap_texture, default_sampler, { 0, 0, 0, -1 }, glm::uvec2(framebuffer_width, framebuffer_height), true, lightProjectionNearPlane, lightProjectionFarPlane);
            bonobo::displayTexture({ -0.45f,  0.55f }, { -0.05f,  0.95f }, light_diffuse_contribution_texture, default_sampler, { 0, 1, 2, -1 }, glm::uvec2(framebuffer_width, framebuffer_height));
            bonobo::displayTexture({ 0.05f,  0.55f }, { 0.45f,  0.95f }, light_specular_contribution_texture, default_sampler, { 0, 1, 2, -1 }, glm::uvec2(framebuffer_width, framebuffer_height));
            bonobo::displayTexture({ 0.55f, 0.55f }, { 0.95f, 0.95f }, environmentmap_texture, default_sampler, { 0, 1, 2, -1 }, glm::uvec2(framebuffer_width, framebuffer_height), true, lightProjectionNearPlane, lightProjectionFarPlane);
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

    glDeleteProgram(resolve_deferred_shader);
    resolve_deferred_shader = 0u;
    glDeleteProgram(accumulate_lights_shader);
    accumulate_lights_shader = 0u;
    glDeleteProgram(fill_shadowmap_shader);
    fill_shadowmap_shader = 0u;
    glDeleteProgram(fill_gbuffer_shader);
    fill_gbuffer_shader = 0u;
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

static
bonobo::mesh_data
loadCone()
{
    bonobo::mesh_data cone;
    cone.vertices_nb = 65;
    cone.drawing_mode = GL_TRIANGLE_STRIP;
    float vertexArrayData[65 * 3] = {
        0.f, 1.f, -1.f,
        0.f, 0.f, 0.f,
        0.38268f, 0.92388f, -1.f,
        0.f, 0.f, 0.f,
        0.70711f, 0.70711f, -1.f,
        0.f, 0.f, 0.f,
        0.92388f, 0.38268f, -1.f,
        0.f, 0.f, 0.f,
        1.f, 0.f, -1.f,
        0.f, 0.f, 0.f,
        0.92388f, -0.38268f, -1.f,
        0.f, 0.f, 0.f,
        0.70711f, -0.70711f, -1.f,
        0.f, 0.f, 0.f,
        0.38268f, -0.92388f, -1.f,
        0.f, 0.f, 0.f,
        0.f, -1.f, -1.f,
        0.f, 0.f, 0.f,
        -0.38268f, -0.92388f, -1.f,
        0.f, 0.f, 0.f,
        -0.70711f, -0.70711f, -1.f,
        0.f, 0.f, 0.f,
        -0.92388f, -0.38268f, -1.f,
        0.f, 0.f, 0.f,
        -1.f, 0.f, -1.f,
        0.f, 0.f, 0.f,
        -0.92388f, 0.38268f, -1.f,
        0.f, 0.f, 0.f,
        -0.70711f, 0.70711f, -1.f,
        0.f, 0.f, 0.f,
        -0.38268f, 0.92388f, -1.f,
        0.f, 1.f, -1.f,
        0.f, 1.f, -1.f,
        0.38268f, 0.92388f, -1.f,
        0.f, 1.f, -1.f,
        0.70711f, 0.70711f, -1.f,
        0.f, 0.f, -1.f,
        0.92388f, 0.38268f, -1.f,
        0.f, 0.f, -1.f,
        1.f, 0.f, -1.f,
        0.f, 0.f, -1.f,
        0.92388f, -0.38268f, -1.f,
        0.f, 0.f, -1.f,
        0.70711f, -0.70711f, -1.f,
        0.f, 0.f, -1.f,
        0.38268f, -0.92388f, -1.f,
        0.f, 0.f, -1.f,
        0.f, -1.f, -1.f,
        0.f, 0.f, -1.f,
        -0.38268f, -0.92388f, -1.f,
        0.f, 0.f, -1.f,
        -0.70711f, -0.70711f, -1.f,
        0.f, 0.f, -1.f,
        -0.92388f, -0.38268f, -1.f,
        0.f, 0.f, -1.f,
        -1.f, 0.f, -1.f,
        0.f, 0.f, -1.f,
        -0.92388f, 0.38268f, -1.f,
        0.f, 0.f, -1.f,
        -0.70711f, 0.70711f, -1.f,
        0.f, 0.f, -1.f,
        -0.38268f, 0.92388f, -1.f,
        0.f, 0.f, -1.f,
        0.f, 1.f, -1.f,
        0.f, 0.f, -1.f
    };

    glGenVertexArrays(1, &cone.vao);
    assert(cone.vao != 0u);
    glBindVertexArray(cone.vao);
    {
        glGenBuffers(1, &cone.bo);
        assert(cone.bo != 0u);
        glBindBuffer(GL_ARRAY_BUFFER, cone.bo);
        glBufferData(GL_ARRAY_BUFFER, cone.vertices_nb * 3 * sizeof(float), vertexArrayData, GL_STATIC_DRAW);

        glVertexAttribPointer(static_cast<int>(bonobo::shader_bindings::vertices), 3, GL_FLOAT, GL_FALSE, 0, reinterpret_cast<GLvoid const*>(0x0));
        glEnableVertexAttribArray(static_cast<int>(bonobo::shader_bindings::vertices));

        glBindBuffer(GL_ARRAY_BUFFER, 0u);
    }
    glBindVertexArray(0u);

    return cone;
}
