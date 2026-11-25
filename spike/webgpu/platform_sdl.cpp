// SDL3 window, size and events: the same code on every platform.
#include "platform.hpp"

#include <cstdio>

#include <SDL3/SDL.h>
#ifdef SPIKE_IMGUI
#include <imgui_impl_sdl3.h>
#endif

namespace {
SDL_Window* g_window = nullptr;
}

namespace platform {

bool OpenWindow(const char* title, int width, int height) {
	if (!SDL_Init(SDL_INIT_VIDEO)) {
		std::printf("SDL_Init: %s\n", SDL_GetError());
		return false;
	}
	g_window = SDL_CreateWindow(title, width, height, SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);
	if (!g_window) std::printf("SDL_CreateWindow: %s\n", SDL_GetError());
	return g_window != nullptr;
}

SDL_Window* Window() { return g_window; }

void WindowPixelSize(uint32_t& width, uint32_t& height) {
	int w = 0, h = 0;
	SDL_GetWindowSizeInPixels(g_window, &w, &h);
	width = uint32_t(w);
	height = uint32_t(h);
}

// P toggles relative mouse (pointer lock), F toggles fullscreen. Input is printed so a test can read it.
bool PollEvents([[maybe_unused]] bool to_imgui) {
	SDL_Event event;
	while (SDL_PollEvent(&event)) {
#ifdef SPIKE_IMGUI
		if (to_imgui) ImGui_ImplSDL3_ProcessEvent(&event);
#endif
		switch (event.type) {
			case SDL_EVENT_QUIT: return false;
			case SDL_EVENT_KEY_DOWN:
				std::printf("key down '%s' repeat %d\n", SDL_GetKeyName(event.key.key), int(event.key.repeat));
				if (event.key.key == SDLK_P && !event.key.repeat) {
					const bool want = !SDL_GetWindowRelativeMouseMode(g_window);
					const bool ok = SDL_SetWindowRelativeMouseMode(g_window, want);
					std::printf("relative mouse -> %d: %s\n", int(want), ok ? "accepted" : SDL_GetError());
				}
				if (event.key.key == SDLK_F && !event.key.repeat) {
					const bool want = !(SDL_GetWindowFlags(g_window) & SDL_WINDOW_FULLSCREEN);
					const bool ok = SDL_SetWindowFullscreen(g_window, want);
					std::printf("fullscreen -> %d: %s\n", int(want), ok ? "accepted" : SDL_GetError());
				}
				break;
			case SDL_EVENT_KEY_UP: std::printf("key up '%s'\n", SDL_GetKeyName(event.key.key)); break;
			case SDL_EVENT_MOUSE_BUTTON_DOWN:
			case SDL_EVENT_MOUSE_BUTTON_UP:
				std::printf("mouse %s button %d at %.1f,%.1f\n", event.button.down ? "down" : "up", event.button.button,
				            event.button.x, event.button.y);
				break;
			case SDL_EVENT_MOUSE_WHEEL: std::printf("wheel %.2f,%.2f\n", event.wheel.x, event.wheel.y); break;
			case SDL_EVENT_MOUSE_MOTION:
				if (SDL_GetWindowRelativeMouseMode(g_window))
					std::printf("motion delta %.1f,%.1f\n", event.motion.xrel, event.motion.yrel);
				break;
			case SDL_EVENT_FINGER_DOWN: std::printf("finger down %.3f,%.3f\n", event.tfinger.x, event.tfinger.y); break;
			case SDL_EVENT_WINDOW_RESIZED: std::printf("resized %dx%d\n", event.window.data1, event.window.data2); break;
			case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
				std::printf("pixel size %dx%d\n", event.window.data1, event.window.data2);
				break;
			case SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED:
				std::printf("display scale %.2f\n", SDL_GetWindowDisplayScale(g_window));
				break;
			case SDL_EVENT_WINDOW_ENTER_FULLSCREEN: std::printf("enter fullscreen\n"); break;
			case SDL_EVENT_WINDOW_LEAVE_FULLSCREEN: std::printf("leave fullscreen\n"); break;
			case SDL_EVENT_WINDOW_FOCUS_GAINED: std::printf("focus gained\n"); break;
			case SDL_EVENT_WINDOW_FOCUS_LOST: std::printf("focus lost\n"); break;
			default: break;
		}
	}
	return true;
}

void CloseWindow() {
	SDL_DestroyWindow(g_window);
	SDL_Quit();
}

}  // namespace platform
