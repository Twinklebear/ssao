// ImGui GLFW binding with OpenGL3 + shaders modified for SDL2 instead
// https://github.com/ocornut/imgui

#include <SDL.h>

// TODO: You should define the texture unit that ImGui can use for its texture
#define SDL_IMGUI_TEXTURE_UNIT 31

bool imgui_impl_init(SDL_Window *window);
void imgui_impl_shutdown();
void imgui_impl_newframe();

// Use if you want to reset your rendering device without losing ImGui state.
void imgui_impl_invalidatedeviceobjects();
bool imgui_impl_createdeviceobjects();

// Call these when the corresponding SDL events occur to pass inputs to ImGui

// Handles: SDL_MOUSEBUTTONDOWN and SDL_MOUSEBUTTONUP
void imgui_impl_mousebuttoncallback(SDL_Window *window, int button, int action, int mods);
// Handles: SDL_MOUSEWHEEL
void imgui_impl_scrollcallback(SDL_Window *window, double xoffset, double yoffset);
// Handles: SDL_KEYDOWN and SDL_KEYUP
void imgui_impl_keycallback(SDL_Window *window, int key, int scancode, int action, int mods);
// Handles: SDL_TEXTINPUT
void imgui_impl_charcallback(SDL_Window *window, const char *text);

