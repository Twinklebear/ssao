// ImGui GLFW binding with OpenGL3 + shaders converted for SDL
// https://github.com/ocornut/imgui

#include <imgui.h>
#include "imgui_impl.h"

#include <SDL.h>
#include "glt/gl_core_4_5.h"

static SDL_Window *imgui_impl_window = NULL;
static double imgui_impl_time = 0.0f;
static bool imgui_impl_mousepressed[3] = { false, false, false };
static float imgui_impl_mousewheel = 0.0f;
static GLuint imgui_impl_fonttexture = 0;
static int imgui_impl_shaderhandle = 0, imgui_impl_verthandle = 0, imgui_impl_fraghandle = 0;
static int imgui_impl_attriblocationtex = 0, imgui_impl_attriblocationprojmat = 0;
static int imgui_impl_attriblocationpos = 0, imgui_impl_attriblocationuv = 0, imgui_impl_attriblocationcolor = 0;
static size_t imgui_impl_vbosize = 0;
static unsigned int imgui_impl_vbohandle = 0, imgui_impl_vaohandle = 0;

// This is the main rendering function that you have to implement and provide to ImGui (via setting up 'RenderDrawListsFn' in the ImGuiIO structure)
// If text or lines are blurry when integrating ImGui in your engine:
// - in your Render function, try translating your projection matrix by (0.5f,0.5f) or (0.375f,0.375f)
static void imgui_impl_renderdrawlists(ImDrawList** const cmd_lists, int cmd_lists_count){
	if (cmd_lists_count == 0){
		return;
	}

	// Setup render state: alpha-blending enabled, no face culling, no depth testing, scissor enabled
	glEnable(GL_BLEND);
	glBlendEquation(GL_FUNC_ADD);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glDisable(GL_CULL_FACE);
	glDisable(GL_DEPTH_TEST);
	glEnable(GL_SCISSOR_TEST);
	glActiveTexture(GL_TEXTURE0 + SDL_IMGUI_TEXTURE_UNIT);

	// Setup orthographic projection matrix
	const float width = ImGui::GetIO().DisplaySize.x;
	const float height = ImGui::GetIO().DisplaySize.y;
	const float ortho_projection[4][4] = {
		{ 2.0f/width,	0.0f,			0.0f,		0.0f },
		{ 0.0f,			2.0f/-height,	0.0f,		0.0f },
		{ 0.0f,			0.0f,			-1.0f,		0.0f },
		{ -1.0f,		1.0f,			0.0f,		1.0f },
	};
	glUseProgram(imgui_impl_shaderhandle);
	glUniform1i(imgui_impl_attriblocationtex, SDL_IMGUI_TEXTURE_UNIT);
	glUniformMatrix4fv(imgui_impl_attriblocationprojmat, 1, GL_FALSE, &ortho_projection[0][0]);

	// Grow our buffer according to what we need
	size_t total_vtx_count = 0;
	for (int n = 0; n < cmd_lists_count; ++n){
		total_vtx_count += cmd_lists[n]->vtx_buffer.size();
	}
	glBindBuffer(GL_ARRAY_BUFFER, imgui_impl_vbohandle);
	size_t needed_vtx_size = total_vtx_count * sizeof(ImDrawVert);
	if (imgui_impl_vbosize < needed_vtx_size){
		// Grow buffer
		imgui_impl_vbosize = needed_vtx_size + 5000 * sizeof(ImDrawVert); 
		glBufferData(GL_ARRAY_BUFFER, imgui_impl_vbosize, NULL, GL_STREAM_DRAW);
	}

	// Copy and convert all vertices into a single contiguous buffer
	unsigned char* buffer_data = (unsigned char*)glMapBuffer(GL_ARRAY_BUFFER, GL_WRITE_ONLY);
	if (!buffer_data){
		return;
	}
	for (int n = 0; n < cmd_lists_count; ++n){
		const ImDrawList* cmd_list = cmd_lists[n];
		memcpy(buffer_data, &cmd_list->vtx_buffer[0], cmd_list->vtx_buffer.size() * sizeof(ImDrawVert));
		buffer_data += cmd_list->vtx_buffer.size() * sizeof(ImDrawVert);
	}
	glUnmapBuffer(GL_ARRAY_BUFFER);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindVertexArray(imgui_impl_vaohandle);

	int cmd_offset = 0;
	for (int n = 0; n < cmd_lists_count; ++n){
		const ImDrawList* cmd_list = cmd_lists[n];
		int vtx_offset = cmd_offset;
		const ImDrawCmd* pcmd_end = cmd_list->commands.end();
		for (const ImDrawCmd* pcmd = cmd_list->commands.begin(); pcmd != pcmd_end; ++pcmd){
			if (pcmd->user_callback){
				pcmd->user_callback(cmd_list, pcmd);
			}
			else {
				glBindTexture(GL_TEXTURE_2D, (GLuint)(intptr_t)pcmd->texture_id);
				glScissor((int)pcmd->clip_rect.x, (int)(height - pcmd->clip_rect.w), (int)(pcmd->clip_rect.z - pcmd->clip_rect.x),
						(int)(pcmd->clip_rect.w - pcmd->clip_rect.y));
				glDrawArrays(GL_TRIANGLES, vtx_offset, pcmd->vtx_count);
			}
			vtx_offset += pcmd->vtx_count;
		}
		cmd_offset = vtx_offset;
	}

	// Restore modified state
	glBindVertexArray(0);
	glUseProgram(0);
	glDisable(GL_SCISSOR_TEST);
	glBindTexture(GL_TEXTURE_2D, 0);
	glDisable(GL_BLEND);
	glEnable(GL_CULL_FACE);
	glEnable(GL_DEPTH_TEST);
}

static const char* imgui_impl_getclipboardtext(){
	return SDL_GetClipboardText();
}

static void imgui_impl_setclipboardtext(const char* text){
	SDL_SetClipboardText(text);
}

void imgui_impl_mousebuttoncallback(SDL_Window*, int button, int action, int mods){
	if (action == SDL_MOUSEBUTTONDOWN && button >= 0 && button < 3){
		imgui_impl_mousepressed[button] = true;
	}
}

void imgui_impl_scrollcallback(SDL_Window*, double xoffset, double yoffset){
	// Use fractional mouse wheel, 1.0 unit 5 lines.
	imgui_impl_mousewheel += (float)yoffset; 
}

void imgui_impl_keycallback(SDL_Window*, int key, int scancode, int action, int mods){
	ImGuiIO& io = ImGui::GetIO();
	if (action == SDL_KEYDOWN){
		io.KeysDown[scancode] = true;
	}
	if (action == SDL_KEYUP){
		io.KeysDown[scancode] = false;
	}
	io.KeyCtrl = (mods & KMOD_CTRL) != 0;
	io.KeyShift = (mods & KMOD_SHIFT) != 0;
	io.KeyAlt = (mods & KMOD_ALT) != 0;
}

void imgui_impl_charcallback(SDL_Window*, const char *text){
	ImGuiIO& io = ImGui::GetIO();
	for (const char *c = text; *c; ++c){
		io.AddInputCharacter((unsigned short)*c);
	}
}

void imgui_impl_createfontstexture(){
	ImGuiIO& io = ImGui::GetIO();

	unsigned char* pixels;
	int width, height;
	// Load as RGBA 32-bits for OpenGL3 demo because it is more likely to be compatible with user's existing shader.
	io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);

	glActiveTexture(GL_TEXTURE0 + SDL_IMGUI_TEXTURE_UNIT);
	glGenTextures(1, &imgui_impl_fonttexture);
	glBindTexture(GL_TEXTURE_2D, imgui_impl_fonttexture);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);

	// Store our identifier
	io.Fonts->TexID = (void *)(intptr_t)imgui_impl_fonttexture;
}

bool imgui_impl_createdeviceobjects(){
	const GLchar *vertex_shader =
		"#version 330\n"
		"uniform mat4 ProjMtx;\n"
		"in vec2 Position;\n"
		"in vec2 UV;\n"
		"in vec4 Color;\n"
		"out vec2 Frag_UV;\n"
		"out vec4 Frag_Color;\n"
		"void main()\n"
		"{\n"
		"	Frag_UV = UV;\n"
		"	Frag_Color = Color;\n"
		"	gl_Position = ProjMtx * vec4(Position.xy,0,1);\n"
		"}\n";

	const GLchar* fragment_shader =
		"#version 330\n"
		"uniform sampler2D Texture;\n"
		"in vec2 Frag_UV;\n"
		"in vec4 Frag_Color;\n"
		"out vec4 Out_Color;\n"
		"void main()\n"
		"{\n"
		"	Out_Color = Frag_Color * texture( Texture, Frag_UV.st);\n"
		"}\n";

	imgui_impl_shaderhandle = glCreateProgram();
	imgui_impl_verthandle = glCreateShader(GL_VERTEX_SHADER);
	imgui_impl_fraghandle = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(imgui_impl_verthandle, 1, &vertex_shader, 0);
	glShaderSource(imgui_impl_fraghandle, 1, &fragment_shader, 0);
	glCompileShader(imgui_impl_verthandle);
	glCompileShader(imgui_impl_fraghandle);
	glAttachShader(imgui_impl_shaderhandle, imgui_impl_verthandle);
	glAttachShader(imgui_impl_shaderhandle, imgui_impl_fraghandle);
	glLinkProgram(imgui_impl_shaderhandle);

	imgui_impl_attriblocationtex = glGetUniformLocation(imgui_impl_shaderhandle, "Texture");
	imgui_impl_attriblocationprojmat = glGetUniformLocation(imgui_impl_shaderhandle, "ProjMtx");
	imgui_impl_attriblocationpos = glGetAttribLocation(imgui_impl_shaderhandle, "Position");
	imgui_impl_attriblocationuv = glGetAttribLocation(imgui_impl_shaderhandle, "UV");
	imgui_impl_attriblocationcolor = glGetAttribLocation(imgui_impl_shaderhandle, "Color");

	glGenBuffers(1, &imgui_impl_vbohandle);

	glGenVertexArrays(1, &imgui_impl_vaohandle);
	glBindVertexArray(imgui_impl_vaohandle);
	glBindBuffer(GL_ARRAY_BUFFER, imgui_impl_vbohandle);
	glEnableVertexAttribArray(imgui_impl_attriblocationpos);
	glEnableVertexAttribArray(imgui_impl_attriblocationuv);
	glEnableVertexAttribArray(imgui_impl_attriblocationcolor);

#define OFFSETOF(TYPE, ELEMENT) ((size_t)&(((TYPE *)0)->ELEMENT))
	glVertexAttribPointer(imgui_impl_attriblocationpos, 2, GL_FLOAT, GL_FALSE, sizeof(ImDrawVert), (GLvoid*)OFFSETOF(ImDrawVert, pos));
	glVertexAttribPointer(imgui_impl_attriblocationuv, 2, GL_FLOAT, GL_FALSE, sizeof(ImDrawVert), (GLvoid*)OFFSETOF(ImDrawVert, uv));
	glVertexAttribPointer(imgui_impl_attriblocationcolor, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(ImDrawVert), (GLvoid*)OFFSETOF(ImDrawVert, col));
#undef OFFSETOF
	glBindVertexArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	imgui_impl_createfontstexture();

	return true;
}

bool imgui_impl_init(SDL_Window* window){
	imgui_impl_window = window;

	ImGuiIO &io = ImGui::GetIO();
	io.KeyMap[ImGuiKey_Tab] = SDL_SCANCODE_TAB;
	io.KeyMap[ImGuiKey_LeftArrow] = SDL_SCANCODE_LEFT;
	io.KeyMap[ImGuiKey_RightArrow] = SDL_SCANCODE_RIGHT;
	io.KeyMap[ImGuiKey_UpArrow] = SDL_SCANCODE_UP;
	io.KeyMap[ImGuiKey_DownArrow] = SDL_SCANCODE_DOWN;
	io.KeyMap[ImGuiKey_Home] = SDL_SCANCODE_HOME;
	io.KeyMap[ImGuiKey_End] = SDL_SCANCODE_END;
	io.KeyMap[ImGuiKey_Delete] = SDL_SCANCODE_DELETE;
	io.KeyMap[ImGuiKey_Backspace] = SDL_SCANCODE_BACKSPACE;
	io.KeyMap[ImGuiKey_Enter] = SDL_SCANCODE_RETURN;
	io.KeyMap[ImGuiKey_Escape] = SDL_SCANCODE_ESCAPE;
	io.KeyMap[ImGuiKey_A] = SDL_SCANCODE_A;
	io.KeyMap[ImGuiKey_C] = SDL_SCANCODE_C;
	io.KeyMap[ImGuiKey_V] = SDL_SCANCODE_V;
	io.KeyMap[ImGuiKey_X] = SDL_SCANCODE_X;
	io.KeyMap[ImGuiKey_Y] = SDL_SCANCODE_Y;
	io.KeyMap[ImGuiKey_Z] = SDL_SCANCODE_Z;

	io.RenderDrawListsFn = imgui_impl_renderdrawlists;
	io.SetClipboardTextFn = imgui_impl_setclipboardtext;
	io.GetClipboardTextFn = imgui_impl_getclipboardtext;
#ifdef SDL_SYSWM_WINDOWS
	// TODO: How does ImGui behave on other operating systems? Should we pass
	// the window handle on Linux or OS X for example?
	SDL_SysWMInfo wm_info;
	if (SDL_GetWindowWMInfo(window, &wm_info)){
		io.ImeWindowHandle = wm_info.win.hdc;
	}
#endif
	return true;
}

void imgui_impl_shutdown(){
	if (imgui_impl_vaohandle){
		glDeleteVertexArrays(1, &imgui_impl_vaohandle);
	}
	if (imgui_impl_vbohandle){
		glDeleteBuffers(1, &imgui_impl_vbohandle);
	}
	imgui_impl_vaohandle = 0;
	imgui_impl_vbohandle = 0;

	glDetachShader(imgui_impl_shaderhandle, imgui_impl_verthandle);
	glDeleteShader(imgui_impl_verthandle);
	imgui_impl_verthandle = 0;

	glDetachShader(imgui_impl_shaderhandle, imgui_impl_fraghandle);
	glDeleteShader(imgui_impl_fraghandle);
	imgui_impl_fraghandle = 0;

	glDeleteProgram(imgui_impl_shaderhandle);
	imgui_impl_shaderhandle = 0;

	if (imgui_impl_fonttexture){
		glDeleteTextures(1, &imgui_impl_fonttexture);
		ImGui::GetIO().Fonts->TexID = 0;
		imgui_impl_fonttexture = 0;
	}
	ImGui::Shutdown();
}

void imgui_impl_newframe(){
	if (!imgui_impl_fonttexture){
		imgui_impl_createdeviceobjects();
	}

	ImGuiIO& io = ImGui::GetIO();

	// Setup display size (every frame to accommodate for window resizing)
	int w, h;
	int display_w, display_h;
	SDL_GetWindowSize(imgui_impl_window, &w, &h);
	SDL_GL_GetDrawableSize(imgui_impl_window, &display_w, &display_h);
	io.DisplaySize = ImVec2((float)display_w, (float)display_h);

	// Setup time step
	double current_time = SDL_GetTicks() / 1000.f;
	// Sometimes when dragging the window you can get a negative delta time and ImGui will assert
	// so just never pass a negative time delta
	io.DeltaTime = imgui_impl_time > 0.0 && current_time > imgui_impl_time ? (float)(current_time - imgui_impl_time) : (float)(1.0f/60.0f);
	imgui_impl_time = current_time;

	// Setup inputs
	// (we already got mouse wheel, keyboard keys & characters from glfw callbacks polled in glfwPollEvents())
	int mouse_state = 0;
	if (imgui_impl_window == SDL_GetMouseFocus()){
		int m_x, m_y;
		mouse_state = SDL_GetMouseState(&m_x, &m_y);
		double mouse_x = m_x, mouse_y = m_y;
		mouse_x *= (float)display_w / w;
		mouse_y *= (float)display_h / h;
		io.MousePos = ImVec2((float)mouse_x, (float)mouse_y);
	}
	else {
		io.MousePos = ImVec2(-1,-1);
	}

	for (int i = 0; i < 3; i++){
		// If a mouse press event came, always pass it as "mouse held this frame", so we don't miss
		// click-release events that are shorter than 1 frame.
		io.MouseDown[i] = imgui_impl_mousepressed[i] || (mouse_state & SDL_BUTTON(i + 1));
		imgui_impl_mousepressed[i] = false;
	}

	io.MouseWheel = imgui_impl_mousewheel;
	imgui_impl_mousewheel = 0.0f;

	// Hide/show hardware mouse cursor
	SDL_ShowCursor(io.MouseDrawCursor ? SDL_DISABLE : SDL_ENABLE);

	// Start the frame
	ImGui::NewFrame();
}

