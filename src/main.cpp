#include <algorithm>
#include <cmath>
#include <iostream>
#include <string>
#include <unordered_map>
#include <array>
#include <SDL.h>
#include <imgui.h>
#include <glm/glm.hpp>
#include <glm/ext.hpp>
#include "glt/gl_core_4_5.h"
#include "glt/debug.h"
#include "glt/buffer_allocator.h"
#include "glt/util.h"
#include "glt/draw_elems_indirect_cmd.h"
#include "glt/strided_array.h"
#include "glt/arcball_camera.h"
#include "glt/flythrough_camera.h"
#include "glt/load_models.h"
#include "glt/load_texture.h"
#include "glt/framebuffer.h"
#include "imgui_impl.h"

const int WIN_WIDTH = 1280;
const int WIN_HEIGHT = 720;

enum RENDER_MODE { FULL, AO_ONLY, NO_AO };

// Tweak params for the AO and blur passes, laid out to match the layout
// of the uniform buffer
struct AOParams {
	// Parameters for the AO pass
	int use_rendered_normals;
	int n_samples;
	int turns;
	float ball_radius;
	float sigma;
	float kappa;
	float beta;
	// Parameters for the blurring pass
	int filter_scale;
	float edge_sharpness;
};

/*
 * Run the assignment program
 */
void run(SDL_Window *win, const std::string &model_file);

int main(int argc, char **argv){
	if (argc < 2){
		std::cout << "Usage: ./exe <model.obj>\n";
		return 1;
	}
	std::string model_file{argv[1]};
	if (SDL_Init(SDL_INIT_VIDEO) != 0){
		std::cout << "SDL_Init error: " << SDL_GetError() << std::endl;
		return 1;
	}

	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
	SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
	SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 4);
	SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
	SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
#ifdef DEBUG
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_DEBUG_FLAG);
#endif

	SDL_Window *win = SDL_CreateWindow("Final Project - Will Usher", SDL_WINDOWPOS_CENTERED,
		SDL_WINDOWPOS_CENTERED, WIN_WIDTH, WIN_HEIGHT, SDL_WINDOW_OPENGL);
	SDL_GLContext ctx = SDL_GL_CreateContext(win);

	// If we don't have a 4.5 context available we may fail loading some functions
	// but this is ok
	bool fail_ok = true;
	if (ctx == nullptr){
		std::cout << "Failed to get 4.5 context, retrying on 4.3" << std::endl;
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
		ctx = SDL_GL_CreateContext(win);
		if (ctx == nullptr){
			std::cout << "Unable to get version 4.3 or higher context, aborting" << std::endl;
			SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Failed to Create OpenGL Context",
					"Could not create an OpenGL context with suitable version, OpenGL 4.3 or higher is required",
					NULL);
			SDL_DestroyWindow(win);
			SDL_Quit();
			return 1;
		}
		fail_ok = true;
	}
	SDL_GL_SetSwapInterval(1);

	if (ogl_LoadFunctions() == ogl_LOAD_FAILED && !fail_ok){
		SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Failed to Load OpenGL Functions",
				"Could not load OpenGL functions for 4.3+, OpenGL 4.3 or higher is required",
				NULL);
		std::cout << "ogl load failed" << std::endl;
		SDL_GL_DeleteContext(ctx);
		SDL_DestroyWindow(win);
		SDL_Quit();
		return 1;
	}
#ifdef DEBUG
	if (argc > 1){
		glt::dbg::register_debug_callback();
	}
#endif
	glClearColor(0.1f, 0.1f, 0.1f, 1.f);
	glClearDepth(1.f);
	glClearStencil(0);
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_CULL_FACE);
	glCullFace(GL_BACK);

	std::cout << "OpenGL Version: " << glGetString(GL_VERSION) << "\n"
		<< "OpenGL Vendor: " << glGetString(GL_VENDOR) << "\n"
		<< "OpenGL Renderer: " << glGetString(GL_RENDERER) << "\n"
		<< "GLSL Version: " << glGetString(GL_SHADING_LANGUAGE_VERSION) << "\n";

	run(win, model_file);

	SDL_GL_DeleteContext(ctx);
	SDL_DestroyWindow(win);
	SDL_Quit();
	return 0;
}
void run(SDL_Window *win, const std::string &model_file){
	// Load and setup our shaders
	const std::string shader_path = glt::get_resource_path("shaders");
	GLint shader = glt::load_program({std::make_pair(GL_VERTEX_SHADER, shader_path + "vert.glsl"),
		std::make_pair(GL_GEOMETRY_SHADER, shader_path + "geom.glsl"),
		std::make_pair(GL_FRAGMENT_SHADER, shader_path + "frag.glsl")});
	GLint ao_sample_shader = glt::load_program({
		std::make_pair(GL_VERTEX_SHADER, shader_path + "ao_sample_vert.glsl"),
		std::make_pair(GL_FRAGMENT_SHADER, shader_path + "ao_sample_frag.glsl")});
	GLint blur_pass_shader = glt::load_program({
		std::make_pair(GL_VERTEX_SHADER, shader_path + "ao_sample_vert.glsl"),
		std::make_pair(GL_FRAGMENT_SHADER, shader_path + "blur_frag.glsl")});
	assert(shader != -1 && ao_sample_shader != -1 && blur_pass_shader != -1);

	GLuint depth_pass_unif = glGetUniformLocation(shader, "depth_pass");
	GLuint ao_only_unif = glGetUniformLocation(shader, "ao_only");
	GLuint ao_values_tex_unif = glGetUniformLocation(shader, "ao_texture");
	glUseProgram(shader);
	glUniform1ui(depth_pass_unif, 0);
	glUniform1ui(ao_only_unif, 0);

	GLuint cam_pos_tex_unif = glGetUniformLocation(ao_sample_shader, "camera_positions");
	GLuint cam_norm_tex_unif = glGetUniformLocation(ao_sample_shader, "camera_normals");

	GLuint blur_cam_pos_tex_unif = glGetUniformLocation(blur_pass_shader, "camera_positions");
	GLuint blur_ao_in_unif = glGetUniformLocation(blur_pass_shader, "ao_in");
	GLuint blur_axis_unif = glGetUniformLocation(blur_pass_shader, "axis");

	GLuint vao;
	glGenVertexArrays(1, &vao);
	glBindVertexArray(vao);

	glt::BufferAllocator allocator{static_cast<size_t>(128e6)};

	glt::SubBuffer vert_buf, elem_buf, mat_buf;
	std::unordered_map<std::string, glt::ModelMatInfo> model_info;
	glt::OBJTextures textures;
	if (!glt::load_model_with_mats(model_file, allocator, vert_buf, elem_buf, mat_buf, textures, model_info)){
		std::cout << "Error loading model!\n";
		return;
	}
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 2 * sizeof(glm::vec3) + sizeof(glm::vec2),
			(void*)vert_buf.offset);
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 2 * sizeof(glm::vec3) + sizeof(glm::vec2),
			(void*)(vert_buf.offset + sizeof(glm::vec3)));
	glEnableVertexAttribArray(2);
	glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 2 * sizeof(glm::vec3) + sizeof(glm::vec2),
			(void*)(vert_buf.offset + 2 * sizeof(glm::vec3)));

	std::cout << "num model textures = " << textures.textures.size() << std::endl;
	std::vector<GLint> tex_unifs;
	for (size_t i = 0; i < textures.textures.size(); ++i){
		glActiveTexture(GL_TEXTURE0 + i);
		glBindTexture(GL_TEXTURE_2D_ARRAY, textures.textures[i]);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
		tex_unifs.push_back(i);
	}
	GLuint textures_unif_loc = glGetUniformLocation(shader, "model_textures");
	glUseProgram(shader);
	glUniform1iv(textures_unif_loc, tex_unifs.size(), tex_unifs.data());
	glBindBufferRange(GL_SHADER_STORAGE_BUFFER, 4, mat_buf.buffer, mat_buf.offset, mat_buf.size);

	// Number of mip levels for our ao and depth textures
	GLsizei levels = std::log2(std::max(WIN_WIDTH, WIN_HEIGHT));

	// Texture and render target for our AO values
	int ao_tex_unit = textures.textures.size() + 2;
	glActiveTexture(GL_TEXTURE0 + ao_tex_unit);
	GLuint ao_val_tex;
	glGenTextures(1, &ao_val_tex);
	glBindTexture(GL_TEXTURE_2D, ao_val_tex);
	glTexStorage2D(GL_TEXTURE_2D, 1, GL_RG32F, WIN_WIDTH, WIN_HEIGHT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

	GLuint ao_pass_fbo;
	glGenFramebuffers(1, &ao_pass_fbo);
	glBindFramebuffer(GL_FRAMEBUFFER, ao_pass_fbo);
	glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, ao_val_tex, 0);
	glDrawBuffer(GL_COLOR_ATTACHMENT0);
	assert(glt::check_framebuffer(ao_pass_fbo));

	glUseProgram(shader);
	glUniform1i(ao_values_tex_unif, ao_tex_unit);

	// Setup the depth only render target for the depth pass
	int cspace_pos_tex_unit = textures.textures.size();
	int cspace_norm_tex_unit = textures.textures.size() + 1;
	glActiveTexture(GL_TEXTURE0 + cspace_pos_tex_unit);
	std::array<GLuint, 4> ao_pass_textures;
	glGenTextures(ao_pass_textures.size(), ao_pass_textures.data());
	glBindTexture(GL_TEXTURE_2D, ao_pass_textures[0]);
	glTexStorage2D(GL_TEXTURE_2D, levels, GL_DEPTH_COMPONENT32F, WIN_WIDTH, WIN_HEIGHT);

	glBindTexture(GL_TEXTURE_2D, ao_pass_textures[1]);
	glTexStorage2D(GL_TEXTURE_2D, levels, GL_RGB32F, WIN_WIDTH, WIN_HEIGHT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);

	glActiveTexture(GL_TEXTURE0 + cspace_norm_tex_unit);
	glBindTexture(GL_TEXTURE_2D, ao_pass_textures[2]);
	glTexStorage2D(GL_TEXTURE_2D, levels, GL_RGB32F, WIN_WIDTH, WIN_HEIGHT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);

	GLuint depth_pass_fbo;
	glGenFramebuffers(1, &depth_pass_fbo);
	glBindFramebuffer(GL_FRAMEBUFFER, depth_pass_fbo);
	glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, ao_pass_textures[0], 0);
	glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, ao_pass_textures[1], 0);
	glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, ao_pass_textures[2], 0);
	GLenum depth_draw_buffers[2] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 };
	glDrawBuffers(2, depth_draw_buffers);
	assert(glt::check_framebuffer(depth_pass_fbo));

	// Intermediate target for the blur pass
	int blur_pass_intermediate_unit = textures.textures.size() + 3;
	glActiveTexture(GL_TEXTURE0 + blur_pass_intermediate_unit);
	glBindTexture(GL_TEXTURE_2D, ao_pass_textures[3]);
	glTexStorage2D(GL_TEXTURE_2D, 1, GL_RG32F, WIN_WIDTH, WIN_HEIGHT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

	GLuint blur_pass_fbo;
	glGenFramebuffers(1, &blur_pass_fbo);
	glBindFramebuffer(GL_FRAMEBUFFER, blur_pass_fbo);
	glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, ao_pass_textures[3], 0);
	glDrawBuffer(GL_COLOR_ATTACHMENT0);
	assert(glt::check_framebuffer(blur_pass_fbo));

	// Also set the AO sample pass shader to read from the R32F texture holding camera space depth values
	glUseProgram(ao_sample_shader);
	glUniform1i(cam_pos_tex_unif, cspace_pos_tex_unit);
	glUniform1i(cam_norm_tex_unif, cspace_norm_tex_unit);

	glUseProgram(blur_pass_shader);
	glUniform1i(blur_cam_pos_tex_unif, cspace_pos_tex_unit);

	glm::vec3 light_pos = glm::normalize(glm::vec3{0, 1, -1});
	glm::mat4 look_at_mat = glm::lookAt(glm::vec3{100, 50, 0}, glm::vec3{0, 50, 0}, glm::vec3{0, 1, 0});
	// Setup view and projection matrix uniform block
	GLint unif_alignment = 0;
	glGetIntegerv(GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT, &unif_alignment);
	// The light pos will be aligned as a vec4 so there's 4 bytes of space between it and the cam pos
	auto globals_buf = allocator.alloc(3 * sizeof(glm::mat4) + 2 * sizeof(glm::vec4)
			+ sizeof(glm::vec2), unif_alignment);
	{
		char *globals_data = static_cast<char*>(globals_buf.map(GL_UNIFORM_BUFFER,
					GL_MAP_INVALIDATE_RANGE_BIT | GL_MAP_WRITE_BIT));
		glm::mat4 *mats = reinterpret_cast<glm::mat4*>(globals_data);
		mats[0] = glm::perspective(glt::to_radians(75),
				static_cast<float>(WIN_WIDTH) / WIN_HEIGHT, 1.f, 1000.f);
		mats[1] = look_at_mat;
		mats[2] = glm::inverse(glm::transpose(look_at_mat));

		glm::vec3 *eye_pos = reinterpret_cast<glm::vec3*>(globals_data + 3 * sizeof(glm::mat4));
		*eye_pos = glm::vec3{0, 0, 6};
		// The light pos will be aligned as a vec4 so there's 4 bytes of space between it and the cam pos
		glm::vec4 *light_info = reinterpret_cast<glm::vec4*>(globals_data + 3 * sizeof(glm::mat4)
			+ sizeof(glm::vec4));
		*light_info = glm::vec4{light_pos, 0.65f};

		glm::vec2 *viewport_info = reinterpret_cast<glm::vec2*>(globals_data + 3 * sizeof(glm::mat4)
			+ 2 * sizeof(glm::vec4));
		*viewport_info = glm::vec2(WIN_WIDTH, WIN_HEIGHT);

		glBindBufferRange(GL_UNIFORM_BUFFER, 0, globals_buf.buffer, globals_buf.offset, globals_buf.size);
		globals_buf.unmap(GL_UNIFORM_BUFFER);
	}

	// Setup material id attributes
	auto mat_id_buf = allocator.alloc(model_info.size() * sizeof(GLuint));
	{
		GLuint *mat_ids = static_cast<GLuint*>(mat_id_buf.map(GL_ARRAY_BUFFER,
					GL_MAP_INVALIDATE_RANGE_BIT | GL_MAP_WRITE_BIT));
		int i = 0;
		for (const auto &m : model_info){
			mat_ids[i++] = m.second.mat_id;
		}
		mat_id_buf.unmap(GL_ARRAY_BUFFER);
	}
	glEnableVertexAttribArray(3);
	glVertexAttribIPointer(3, 1, GL_UNSIGNED_INT, sizeof(GLuint), (void*)(mat_id_buf.offset));
	glVertexAttribDivisor(3, 1);

	// Setup AO tweaking parameters
	AOParams ao_params{0, 27, 16, 3.5f, 3.8f, 0.8f, 0.0005f, 2, 0.8f};
	auto ao_params_buf = allocator.alloc(4 * sizeof(GLint) + 5 * sizeof(GLfloat), unif_alignment);
	{
		AOParams *p = static_cast<AOParams*>(ao_params_buf.map(GL_UNIFORM_BUFFER,
					GL_MAP_INVALIDATE_RANGE_BIT | GL_MAP_WRITE_BIT));
		*p = ao_params;
		glBindBufferRange(GL_UNIFORM_BUFFER, 5, ao_params_buf.buffer, ao_params_buf.offset, ao_params_buf.size);
		ao_params_buf.unmap(GL_UNIFORM_BUFFER);
	}

	// Setup draw commands for our scene geometry
	auto draw_cmd_buf = allocator.alloc(model_info.size() * sizeof(glt::DrawElemsIndirectCmd));
	{
		glt::DrawElemsIndirectCmd *cmds = static_cast<glt::DrawElemsIndirectCmd*>(
				draw_cmd_buf.map(GL_DRAW_INDIRECT_BUFFER, GL_MAP_INVALIDATE_RANGE_BIT | GL_MAP_WRITE_BIT));
		int draw = 0;
		for (const auto &m : model_info){
			cmds[draw] = glt::DrawElemsIndirectCmd(m.second.indices, 1,
					m.second.index_offset + elem_buf.offset / sizeof(GLuint),
					m.second.vert_offset, draw);
			++draw;
		}
		draw_cmd_buf.unmap(GL_DRAW_INDIRECT_BUFFER);
	}

	GLuint dummy_vao;
	glGenVertexArrays(1, &dummy_vao);

	imgui_impl_init(win);

	//auto camera = glt::ArcBallCamera{look_at_mat, 1000.0, 75.0, {1.0 / WIN_WIDTH, 1.0 / WIN_HEIGHT}};
	auto camera = glt::FlythroughCamera{look_at_mat, 1000.0, 75.0, {1.0 / WIN_WIDTH, 1.0 / WIN_HEIGHT}};
	bool quit = false, camera_updated = false, blur_pass_enabled = true, use_rendered_normals = false,
		 ui_hovered = false;
	int render_mode = FULL;
	uint32_t prev_time = SDL_GetTicks();
	uint32_t cur_time;
	while (!quit){
		cur_time = SDL_GetTicks();
		float elapsed = (cur_time - prev_time) / 1000.f;
		prev_time = cur_time;

		SDL_Event e;
		while (SDL_PollEvent(&e)){
			if (e.type == SDL_QUIT || (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE)){
				quit = true;
				break;
			}
			if (e.type == SDL_KEYDOWN){
				switch (e.key.keysym.sym){
					case SDLK_1:
						render_mode = FULL;
						break;
					case SDLK_2:
						render_mode = AO_ONLY;
						break;
					case SDLK_3:
						render_mode = NO_AO;
						break;
					case SDLK_n:
						use_rendered_normals = !use_rendered_normals;
						break;
					case SDLK_b:
						blur_pass_enabled = !blur_pass_enabled;
						break;
					default:
						break;
				}
				camera_updated = camera.keypress(e.key) || camera_updated;
			}
			else if (e.type == SDL_MOUSEMOTION && !ui_hovered){
				camera_updated = camera.mouse_motion(e.motion, elapsed) || camera_updated;
			}
			else if (e.type == SDL_MOUSEWHEEL && !ui_hovered){
				camera_updated = camera.mouse_scroll(e.wheel, elapsed) || camera_updated;
			}

			// Send events to Imgui
			if (e.type == SDL_MOUSEBUTTONDOWN || e.type == SDL_MOUSEBUTTONUP){
				int button = 0;
				switch (e.button.button){
					case SDL_BUTTON_RIGHT: button = 1; break;
					case SDL_BUTTON_MIDDLE: button = 2; break;
				}
				imgui_impl_mousebuttoncallback(win, button, e.type, 0);
			}
			else if (e.type == SDL_MOUSEWHEEL){
				imgui_impl_scrollcallback(win, e.wheel.x, e.wheel.y);
			}
			else if (e.type == SDL_KEYDOWN || e.type == SDL_KEYUP){
				imgui_impl_keycallback(win, e.key.keysym.sym, e.key.keysym.scancode, e.type, e.key.keysym.mod);
			}
		}

		if (camera_updated){
			char *globals_data = static_cast<char*>(globals_buf.map(GL_UNIFORM_BUFFER, GL_MAP_WRITE_BIT));

			glm::mat4 *mats = reinterpret_cast<glm::mat4*>(globals_data);
			mats[1] = camera.transform();
			mats[2] = glm::inverse(glm::transpose(camera.transform()));
			glm::vec3 *eye_vec = reinterpret_cast<glm::vec3*>(mats + 3);
			*eye_vec = camera.eye_pos();

			globals_buf.unmap(GL_UNIFORM_BUFFER);
		}
		camera_updated = false;

		if (render_mode != NO_AO){
			// Render camera space positions
			glBindFramebuffer(GL_FRAMEBUFFER, depth_pass_fbo);
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
			glBindVertexArray(vao);
			glUseProgram(shader);
			glUniform1ui(depth_pass_unif, 1);
			glMultiDrawElementsIndirect(GL_TRIANGLES, GL_UNSIGNED_INT, (void*)(draw_cmd_buf.offset),
					model_info.size(), sizeof(glt::DrawElemsIndirectCmd));

			glBindFramebuffer(GL_FRAMEBUFFER, ao_pass_fbo);
			glActiveTexture(GL_TEXTURE0 + cspace_pos_tex_unit);
			glGenerateMipmap(GL_TEXTURE_2D);

			// Compute noisy AO values
			glClear(GL_COLOR_BUFFER_BIT);
			glBindVertexArray(dummy_vao);
			glUseProgram(ao_sample_shader);
			glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

			if (blur_pass_enabled){
				// Perform horizontal blur pass
				glBindFramebuffer(GL_FRAMEBUFFER, blur_pass_fbo);
				glClear(GL_COLOR_BUFFER_BIT);
				glUseProgram(blur_pass_shader);
				glUniform2i(blur_axis_unif, 0, 1);
				glUniform1i(blur_ao_in_unif, ao_tex_unit);
				glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

				// Perform vertical blur pass
				glBindFramebuffer(GL_FRAMEBUFFER, ao_pass_fbo);
				glClear(GL_COLOR_BUFFER_BIT);
				glUniform1i(blur_ao_in_unif, blur_pass_intermediate_unit);
				glUniform2i(blur_axis_unif, 1, 0);
				glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
			}

			glBindFramebuffer(GL_FRAMEBUFFER, 0);
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

			glBindVertexArray(vao);
			glUseProgram(shader);
			glUniform1ui(depth_pass_unif, 0);
			glUniform1ui(ao_only_unif, render_mode == AO_ONLY);
			glMultiDrawElementsIndirect(GL_TRIANGLES, GL_UNSIGNED_INT, (void*)(draw_cmd_buf.offset),
					model_info.size(), sizeof(glt::DrawElemsIndirectCmd));
			glUniform1ui(ao_only_unif, 0);
		}
		else {
			glBindFramebuffer(GL_FRAMEBUFFER, ao_pass_fbo);
			glClearColor(1, 1, 1, 1);
			glClear(GL_COLOR_BUFFER_BIT);
			glClearColor(0, 0, 0, 1);

			glBindFramebuffer(GL_FRAMEBUFFER, 0);
			glActiveTexture(GL_TEXTURE0 + ao_tex_unit);
			glGenerateMipmap(GL_TEXTURE_2D);

			glBindVertexArray(vao);
			glUseProgram(shader);
			glUniform1ui(depth_pass_unif, 0);
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
			glMultiDrawElementsIndirect(GL_TRIANGLES, GL_UNSIGNED_INT, (void*)(draw_cmd_buf.offset),
					model_info.size(), sizeof(glt::DrawElemsIndirectCmd));
		}

        ImGuiIO& io = ImGui::GetIO();
        imgui_impl_newframe();

		ImGui::Text("Average %.3f ms/frame (%.1f FPS)", 1000.0f / io.Framerate, io.Framerate);
		ImGui::RadioButton("Full Render", &render_mode, FULL);
		ImGui::RadioButton("AO Only", &render_mode, AO_ONLY);
		ImGui::RadioButton("No AO", &render_mode, NO_AO);
		ImGui::Checkbox("Blur Enabled", &blur_pass_enabled);
		ImGui::Checkbox("Use Rendered Normals", &use_rendered_normals);
		if (ImGui::CollapsingHeader("AO Params")){
			ImGui::SliderInt("Num Samples", &ao_params.n_samples, 1, 64);
			ImGui::SliderInt("Num Turns", &ao_params.turns, 1, 64);
			ImGui::SliderFloat("Ball Radius", &ao_params.ball_radius, 0.1f, 10.f);
			ImGui::SliderFloat("Sigma", &ao_params.sigma, 0.1f, 20.f);
			ImGui::SliderFloat("Kappa", &ao_params.kappa, 0.1f, 10.f);
		}
		if (ImGui::CollapsingHeader("Filter Params")){
			ImGui::SliderInt("Filter Scale", &ao_params.filter_scale, 1, 10);
			ImGui::SliderFloat("Edge Sharpness", &ao_params.edge_sharpness, 0.f, 10.f);
		}
		ui_hovered = ImGui::IsMouseHoveringAnyWindow();

        glViewport(0, 0, (int)io.DisplaySize.x, (int)io.DisplaySize.y);
        ImGui::Render();

		SDL_GL_SwapWindow(win);

		// Update our AO parameters
		{
			ao_params.use_rendered_normals = use_rendered_normals ? 1 : 0;
			AOParams *p = static_cast<AOParams*>(ao_params_buf.map(GL_UNIFORM_BUFFER, GL_MAP_WRITE_BIT));
			*p = ao_params;
			ao_params_buf.unmap(GL_UNIFORM_BUFFER);
		}
	}
	glDeleteVertexArrays(1, &vao);
	glDeleteTextures(textures.textures.size(), textures.textures.data());
	glDeleteTextures(ao_pass_textures.size(), ao_pass_textures.data());
	glDeleteTextures(1, &ao_val_tex);
	glDeleteFramebuffers(1, &depth_pass_fbo);
	glDeleteFramebuffers(1, &ao_pass_fbo);
	glDeleteFramebuffers(1, &blur_pass_fbo);
    imgui_impl_shutdown();
	// Intel driver gives an error when I delete a shader?
	//glDeleteProgram(shader);
}

