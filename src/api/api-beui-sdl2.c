/* -*- coding: utf-8; tab-width: 8; indent-tabs-mode: t; -*- */

/*
 * Noct Programming Language
 * Copyright (c) 2025, 2026, Awe Morris
 */

/* SDL2-backed BeUI HAL for desktop hosts. */

#include <noct/beui.h>

#define SDL_MAIN_HANDLED
#include <SDL.h>

#include <limits.h>
#include <stdlib.h>
#include <string.h>

#define BEUI_SDL2_WIDTH 640U
#define BEUI_SDL2_HEIGHT 400U

struct beui_sdl2_context {
	SDL_Window *window;
	SDL_Surface *framebuffer;
	SDL_AudioDeviceID audio_device;
	unsigned audio_channels;
	int video_initialized;
	int timer_initialized;
	int audio_initialized;
	int alive;
};

static struct beui_sdl2_context sdl2_context;
static struct noct_beui_hal sdl2_hal;

static uint32_t
sdl2_color(uint32_t color)
{
	return 0xff000000U | (color & 0x00ffffffU);
}

static int
sdl2_pattern_bit(uint64_t pattern, unsigned x, unsigned y)
{
	uint8_t row;

	row = (uint8_t)(pattern >> ((y & 7U) * 8U));
	return (row & (uint8_t)(0x80U >> (x & 7U))) != 0;
}

static void
sdl2_put_pixel(struct beui_sdl2_context *context, unsigned x, unsigned y,
	       uint32_t color)
{
	uint32_t *row;

	row = (uint32_t *)((uint8_t *)context->framebuffer->pixels +
			   y * (unsigned)context->framebuffer->pitch);
	row[x] = sdl2_color(color);
}

static int
sdl2_lock_framebuffer(struct beui_sdl2_context *context)
{
	if (context == NULL || context->framebuffer == NULL)
		return 0;
	return !SDL_MUSTLOCK(context->framebuffer) ||
		SDL_LockSurface(context->framebuffer) == 0;
}

static void
sdl2_unlock_framebuffer(struct beui_sdl2_context *context)
{
	if (SDL_MUSTLOCK(context->framebuffer))
		SDL_UnlockSurface(context->framebuffer);
}

static int
sdl2_enter(void *opaque, struct noct_beui_display_info *info)
{
	struct beui_sdl2_context *context;

	context = opaque;
	if (context == NULL || info == NULL)
		return 0;
	if (!context->video_initialized) {
		SDL_SetMainReady();
		if (SDL_InitSubSystem(SDL_INIT_VIDEO | SDL_INIT_EVENTS) != 0)
			return 0;
		context->video_initialized = 1;
	}
	context->window = SDL_CreateWindow("Noct BeUI",
					   SDL_WINDOWPOS_CENTERED,
					   SDL_WINDOWPOS_CENTERED,
					   (int)BEUI_SDL2_WIDTH,
					   (int)BEUI_SDL2_HEIGHT,
					   SDL_WINDOW_SHOWN);
	if (context->window == NULL)
		return 0;
	context->framebuffer = SDL_CreateRGBSurfaceWithFormat(
		0, (int)BEUI_SDL2_WIDTH, (int)BEUI_SDL2_HEIGHT, 32,
		SDL_PIXELFORMAT_ARGB8888);
	if (context->framebuffer == NULL) {
		SDL_DestroyWindow(context->window);
		context->window = NULL;
		return 0;
	}
	SDL_FillRect(context->framebuffer, NULL, sdl2_color(0));
	context->alive = 1;
	info->width = BEUI_SDL2_WIDTH;
	info->height = BEUI_SDL2_HEIGHT;
	info->bits_per_pixel = 32;
	info->stride = (unsigned)context->framebuffer->pitch;
	return 1;
}

static void
sdl2_audio_stop(void *opaque)
{
	struct beui_sdl2_context *context;

	context = opaque;
	if (context != NULL && context->audio_device != 0) {
		SDL_ClearQueuedAudio(context->audio_device);
		SDL_CloseAudioDevice(context->audio_device);
		context->audio_device = 0;
		context->audio_channels = 0;
	}
}

static void
sdl2_leave(void *opaque)
{
	struct beui_sdl2_context *context;

	context = opaque;
	if (context == NULL)
		return;
	sdl2_audio_stop(context);
	if (context->framebuffer != NULL) {
		SDL_FreeSurface(context->framebuffer);
		context->framebuffer = NULL;
	}
	if (context->window != NULL) {
		SDL_DestroyWindow(context->window);
		context->window = NULL;
	}
	context->alive = 0;
}

static int
sdl2_poll_events(void *opaque)
{
	struct beui_sdl2_context *context;
	SDL_Event event;

	context = opaque;
	if (context == NULL || context->window == NULL)
		return 0;
	while (SDL_PollEvent(&event)) {
		if (event.type == SDL_QUIT ||
		    (event.type == SDL_WINDOWEVENT &&
		     event.window.windowID == SDL_GetWindowID(context->window) &&
		     event.window.event == SDL_WINDOWEVENT_CLOSE))
			context->alive = 0;
	}
	return context->alive;
}

static int
sdl2_fill(void *opaque, const struct noct_beui_rect *rect, uint32_t color)
{
	struct beui_sdl2_context *context;
	SDL_Rect destination;

	context = opaque;
	if (context == NULL || context->framebuffer == NULL || rect == NULL)
		return 0;
	destination.x = (int)rect->x;
	destination.y = (int)rect->y;
	destination.w = (int)rect->width;
	destination.h = (int)rect->height;
	return SDL_FillRect(context->framebuffer, &destination,
			    sdl2_color(color)) == 0;
}

static int
sdl2_line(void *opaque, unsigned x0, unsigned y0, unsigned x1,
	  unsigned y1, uint32_t color)
{
	struct beui_sdl2_context *context;
	int x;
	int y;
	int target_x;
	int target_y;
	int delta_x;
	int delta_y;
	int step_x;
	int step_y;
	int error;

	context = opaque;
	if (!sdl2_lock_framebuffer(context))
		return 0;
	x = (int)x0;
	y = (int)y0;
	target_x = (int)x1;
	target_y = (int)y1;
	delta_x = target_x >= x ? target_x - x : x - target_x;
	step_x = x < target_x ? 1 : -1;
	delta_y = target_y >= y ? y - target_y : target_y - y;
	step_y = y < target_y ? 1 : -1;
	error = delta_x + delta_y;
	for (;;) {
		int twice_error;

		sdl2_put_pixel(context, (unsigned)x, (unsigned)y, color);
		if (x == target_x && y == target_y)
			break;
		twice_error = error * 2;
		if (twice_error >= delta_y) {
			error += delta_y;
			x += step_x;
		}
		if (twice_error <= delta_x) {
			error += delta_x;
			y += step_y;
		}
	}
	sdl2_unlock_framebuffer(context);
	return 1;
}

static int
sdl2_pattern_fill(void *opaque, const struct noct_beui_rect *rect,
		   uint32_t color, uint64_t pattern)
{
	struct beui_sdl2_context *context;
	unsigned x;
	unsigned y;

	context = opaque;
	if (rect == NULL || !sdl2_lock_framebuffer(context))
		return 0;
	for (y = rect->y; y < rect->y + rect->height; y++) {
		for (x = rect->x; x < rect->x + rect->width; x++) {
			if (sdl2_pattern_bit(pattern, x, y))
				sdl2_put_pixel(context, x, y, color);
		}
	}
	sdl2_unlock_framebuffer(context);
	return 1;
}

static uint32_t
sdl2_image_pixel(const struct noct_beui_image *image, unsigned x, unsigned y)
{
	const uint8_t *pixel;

	pixel = image->pixels + y * image->stride;
	if (image->format == NOCT_BEUI_IMAGE_INDEX8)
		return image->palette[pixel[x]];
	pixel += x * 3U;
	return ((uint32_t)pixel[0] << 16) | ((uint32_t)pixel[1] << 8) |
		pixel[2];
}

static int
sdl2_draw_image_common(struct beui_sdl2_context *context, unsigned x,
			unsigned y, const struct noct_beui_image *image,
			uint64_t pattern, int patterned)
{
	unsigned source_x;
	unsigned source_y;

	if (image == NULL || image->pixels == NULL ||
	    !sdl2_lock_framebuffer(context))
		return 0;
	for (source_y = 0; source_y < image->height; source_y++) {
		for (source_x = 0; source_x < image->width; source_x++) {
			if (!patterned || sdl2_pattern_bit(pattern, x + source_x,
							 y + source_y))
				sdl2_put_pixel(context, x + source_x, y + source_y,
					       sdl2_image_pixel(image, source_x,
								source_y));
		}
	}
	sdl2_unlock_framebuffer(context);
	return 1;
}

static int
sdl2_draw_image(void *opaque, unsigned x, unsigned y,
		 const struct noct_beui_image *image)
{
	return sdl2_draw_image_common(opaque, x, y, image, 0, 0);
}

static int
sdl2_draw_image_pattern(void *opaque, unsigned x, unsigned y,
			 const struct noct_beui_image *image,
			 uint64_t pattern)
{
	return sdl2_draw_image_common(opaque, x, y, image, pattern, 1);
}

static int
sdl2_flush(void *opaque, const struct noct_beui_rect *rectangles,
	   size_t rectangle_count)
{
	struct beui_sdl2_context *context;
	SDL_Surface *window_surface;

	(void)rectangles;
	(void)rectangle_count;
	context = opaque;
	if (context == NULL || context->window == NULL ||
	    context->framebuffer == NULL)
		return 0;
	window_surface = SDL_GetWindowSurface(context->window);
	if (window_surface == NULL ||
	    SDL_BlitSurface(context->framebuffer, NULL, window_surface, NULL) != 0)
		return 0;
	return SDL_UpdateWindowSurface(context->window) == 0;
}

/* A compact built-in 5x7 desktop font.  Lowercase intentionally shares
 * uppercase shapes; non-ASCII codepoints get a visible fallback box. */
static void
sdl2_ascii_glyph(uint32_t codepoint, uint8_t rows[7])
{
	static const char alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
	static const uint8_t glyphs[][7] = {
		{14,17,17,31,17,17,17}, {30,17,17,30,17,17,30},
		{14,17,16,16,16,17,14}, {30,17,17,17,17,17,30},
		{31,16,16,30,16,16,31}, {31,16,16,30,16,16,16},
		{14,17,16,23,17,17,15}, {17,17,17,31,17,17,17},
		{14,4,4,4,4,4,14}, {7,2,2,2,18,18,12},
		{17,18,20,24,20,18,17}, {16,16,16,16,16,16,31},
		{17,27,21,21,17,17,17}, {17,25,21,19,17,17,17},
		{14,17,17,17,17,17,14}, {30,17,17,30,16,16,16},
		{14,17,17,17,21,18,13}, {30,17,17,30,20,18,17},
		{15,16,16,14,1,1,30}, {31,4,4,4,4,4,4},
		{17,17,17,17,17,17,14}, {17,17,17,17,17,10,4},
		{17,17,17,21,21,21,10}, {17,17,10,4,10,17,17},
		{17,17,10,4,4,4,4}, {31,1,2,4,8,16,31},
		{14,17,19,21,25,17,14}, {4,12,4,4,4,4,14},
		{14,17,1,2,4,8,31}, {30,1,1,14,1,1,30},
		{2,6,10,18,31,2,2}, {31,16,16,30,1,1,30},
		{14,16,16,30,17,17,14}, {31,1,2,4,8,8,8},
		{14,17,17,14,17,17,14}, {14,17,17,15,1,1,14}
	};
	const char *found;

	memset(rows, 0, 7);
	if (codepoint >= 'a' && codepoint <= 'z')
		codepoint -= 'a' - 'A';
	found = strchr(alphabet, (int)codepoint);
	if (found != NULL) {
		memcpy(rows, glyphs[found - alphabet], 7);
		return;
	}
	switch (codepoint) {
	case '.': rows[6] = 4; break;
	case ',': rows[5] = 4; rows[6] = 8; break;
	case ':': rows[2] = 4; rows[5] = 4; break;
	case ';': rows[2] = 4; rows[5] = 4; rows[6] = 8; break;
	case '-': rows[3] = 31; break;
	case '_': rows[6] = 31; break;
	case '+': rows[2] = 4; rows[3] = 31; rows[4] = 4; break;
	case '/': rows[0] = 1; rows[1] = 2; rows[2] = 2; rows[3] = 4;
		rows[4] = 8; rows[5] = 8; rows[6] = 16; break;
	case '\\': rows[0] = 16; rows[1] = 8; rows[2] = 8; rows[3] = 4;
		rows[4] = 2; rows[5] = 2; rows[6] = 1; break;
	case '!': rows[0] = 4; rows[1] = 4; rows[2] = 4; rows[3] = 4;
		rows[5] = 4; break;
	case '?': rows[0] = 14; rows[1] = 17; rows[2] = 1; rows[3] = 2;
		rows[4] = 4; rows[6] = 4; break;
	case '(': rows[0] = 2; rows[1] = 4; rows[2] = 8; rows[3] = 8;
		rows[4] = 8; rows[5] = 4; rows[6] = 2; break;
	case ')': rows[0] = 8; rows[1] = 4; rows[2] = 2; rows[3] = 2;
		rows[4] = 2; rows[5] = 4; rows[6] = 8; break;
	case '[': rows[0] = 14; rows[1] = 8; rows[2] = 8; rows[3] = 8;
		rows[4] = 8; rows[5] = 8; rows[6] = 14; break;
	case ']': rows[0] = 14; rows[1] = 2; rows[2] = 2; rows[3] = 2;
		rows[4] = 2; rows[5] = 2; rows[6] = 14; break;
	case '=': rows[2] = 31; rows[4] = 31; break;
	case '*': rows[1] = 21; rows[2] = 14; rows[3] = 31;
		rows[4] = 14; rows[5] = 21; break;
	case ' ': break;
	default: rows[0] = 14; rows[1] = 17; rows[2] = 1; rows[3] = 2;
		rows[4] = 4; rows[6] = 4; break;
	}
}

static int
sdl2_glyph_measure(void *opaque, uint32_t codepoint, unsigned *width,
		   unsigned *height)
{
	(void)opaque;
	if (width == NULL || height == NULL)
		return 0;
	*width = codepoint < 0x80U ? 8U : 16U;
	*height = 16U;
	return 1;
}

static int
sdl2_glyph_draw(void *opaque, unsigned x, unsigned y, uint32_t codepoint,
		uint32_t foreground, uint32_t background)
{
	struct beui_sdl2_context *context;
	struct noct_beui_rect rect;
	uint8_t rows[7];
	unsigned width;
	unsigned row;
	unsigned column;

	context = opaque;
	if (context == NULL || context->framebuffer == NULL)
		return 0;
	width = codepoint < 0x80U ? 8U : 16U;
	rect.x = x;
	rect.y = y;
	rect.width = width;
	rect.height = 16;
	if (!sdl2_fill(context, &rect, background))
		return 0;
	if (!sdl2_lock_framebuffer(context))
		return 0;
	if (codepoint >= 0x80U) {
		for (column = 1; column + 1 < width; column++) {
			sdl2_put_pixel(context, x + column, y + 1, foreground);
			sdl2_put_pixel(context, x + column, y + 14, foreground);
		}
		for (row = 1; row < 15; row++) {
			sdl2_put_pixel(context, x + 1, y + row, foreground);
			sdl2_put_pixel(context, x + width - 2, y + row, foreground);
		}
		sdl2_unlock_framebuffer(context);
		return 1;
	}
	sdl2_ascii_glyph(codepoint, rows);
	for (row = 0; row < 7; row++) {
		for (column = 0; column < 5; column++) {
			if ((rows[row] & (uint8_t)(16U >> column)) != 0) {
				sdl2_put_pixel(context, x + column + 1,
					       y + row * 2U + 1U, foreground);
				sdl2_put_pixel(context, x + column + 1,
					       y + row * 2U + 2U, foreground);
			}
		}
	}
	sdl2_unlock_framebuffer(context);
	return 1;
}

static int
sdl2_pointer_start(void *opaque,
		   const struct noct_beui_display_info *display)
{
	struct beui_sdl2_context *context;

	context = opaque;
	if (context == NULL || context->window == NULL || display == NULL)
		return 0;
	SDL_WarpMouseInWindow(context->window, (int)(display->width / 2U),
			      (int)(display->height / 2U));
	return 1;
}

static void
sdl2_pointer_stop(void *opaque)
{
	(void)opaque;
}

static int
sdl2_pointer_poll(void *opaque, struct noct_beui_pointer_event *event)
{
	struct beui_sdl2_context *context;
	int x;
	int y;
	uint32_t buttons;

	context = opaque;
	if (context == NULL || context->window == NULL || event == NULL)
		return -1;
	buttons = SDL_GetMouseState(&x, &y);
	event->x = x < 0 ? 0U : (unsigned)x;
	event->y = y < 0 ? 0U : (unsigned)y;
	event->buttons = 0;
	if ((buttons & SDL_BUTTON(SDL_BUTTON_LEFT)) != 0)
		event->buttons |= NOCT_BEUI_BUTTON_LEFT;
	if ((buttons & SDL_BUTTON(SDL_BUTTON_RIGHT)) != 0)
		event->buttons |= NOCT_BEUI_BUTTON_RIGHT;
	if ((buttons & SDL_BUTTON(SDL_BUTTON_MIDDLE)) != 0)
		event->buttons |= NOCT_BEUI_BUTTON_MIDDLE;
	return 1;
}

static uint64_t
sdl2_milliseconds(void *opaque)
{
	struct beui_sdl2_context *context;
	uint64_t counter;
	uint64_t frequency;

	context = opaque;
	if (context != NULL && !context->timer_initialized) {
		SDL_SetMainReady();
		if (SDL_InitSubSystem(SDL_INIT_TIMER) == 0)
			context->timer_initialized = 1;
	}
	counter = SDL_GetPerformanceCounter();
	frequency = SDL_GetPerformanceFrequency();
	if (frequency == 0)
		return 0;
	return counter / frequency * 1000U +
		(counter % frequency) * 1000U / frequency;
}

static int
sdl2_audio_start(void *opaque, unsigned sample_rate, unsigned channels)
{
	struct beui_sdl2_context *context;
	SDL_AudioSpec desired;
	SDL_AudioSpec obtained;

	context = opaque;
	if (context == NULL || sample_rate == 0 ||
	    (channels != 1 && channels != 2))
		return 0;
	if (!context->audio_initialized) {
		SDL_SetMainReady();
		if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0)
			return 0;
		context->audio_initialized = 1;
	}
	memset(&desired, 0, sizeof(desired));
	desired.freq = (int)sample_rate;
	desired.format = AUDIO_S16SYS;
	desired.channels = (uint8_t)channels;
	desired.samples = 1024;
	context->audio_device = SDL_OpenAudioDevice(NULL, 0, &desired, &obtained,
						    SDL_AUDIO_ALLOW_FREQUENCY_CHANGE);
	if (context->audio_device == 0 || obtained.format != AUDIO_S16SYS ||
	    obtained.channels != channels) {
		sdl2_audio_stop(context);
		return 0;
	}
	context->audio_channels = channels;
	SDL_PauseAudioDevice(context->audio_device, 0);
	return 1;
}

static int
sdl2_audio_poll(void *opaque)
{
	struct beui_sdl2_context *context;

	context = opaque;
	return context != NULL && context->audio_device != 0 &&
		SDL_GetAudioDeviceStatus(context->audio_device) != SDL_AUDIO_STOPPED;
}

static int
sdl2_audio_write(void *opaque, const int16_t *samples, size_t frame_count)
{
	struct beui_sdl2_context *context;
	size_t bytes;

	context = opaque;
	if (context == NULL || context->audio_device == 0 || samples == NULL ||
	    frame_count > UINT_MAX / context->audio_channels / sizeof(*samples))
		return 0;
	bytes = frame_count * context->audio_channels * sizeof(*samples);
	return SDL_QueueAudio(context->audio_device, samples, (uint32_t)bytes) == 0;
}

static SDL_Scancode
sdl2_key_scancode(int key)
{
	switch (key) {
	case NOCT_BEUI_KEY_ESCAPE: return SDL_SCANCODE_ESCAPE;
	case NOCT_BEUI_KEY_BACKSPACE: return SDL_SCANCODE_BACKSPACE;
	case NOCT_BEUI_KEY_TAB: return SDL_SCANCODE_TAB;
	case NOCT_BEUI_KEY_ENTER: return SDL_SCANCODE_RETURN;
	case NOCT_BEUI_KEY_PAGE_UP: return SDL_SCANCODE_PAGEUP;
	case NOCT_BEUI_KEY_PAGE_DOWN: return SDL_SCANCODE_PAGEDOWN;
	case NOCT_BEUI_KEY_INSERT: return SDL_SCANCODE_INSERT;
	case NOCT_BEUI_KEY_DELETE: return SDL_SCANCODE_DELETE;
	case NOCT_BEUI_KEY_UP: return SDL_SCANCODE_UP;
	case NOCT_BEUI_KEY_LEFT: return SDL_SCANCODE_LEFT;
	case NOCT_BEUI_KEY_RIGHT: return SDL_SCANCODE_RIGHT;
	case NOCT_BEUI_KEY_DOWN: return SDL_SCANCODE_DOWN;
	case NOCT_BEUI_KEY_HOME: return SDL_SCANCODE_HOME;
	case NOCT_BEUI_KEY_END: return SDL_SCANCODE_END;
	case NOCT_BEUI_KEY_F1: return SDL_SCANCODE_F1;
	case NOCT_BEUI_KEY_F2: return SDL_SCANCODE_F2;
	case NOCT_BEUI_KEY_F3: return SDL_SCANCODE_F3;
	case NOCT_BEUI_KEY_F4: return SDL_SCANCODE_F4;
	case NOCT_BEUI_KEY_F5: return SDL_SCANCODE_F5;
	case NOCT_BEUI_KEY_F6: return SDL_SCANCODE_F6;
	case NOCT_BEUI_KEY_F7: return SDL_SCANCODE_F7;
	case NOCT_BEUI_KEY_F8: return SDL_SCANCODE_F8;
	case NOCT_BEUI_KEY_F9: return SDL_SCANCODE_F9;
	case NOCT_BEUI_KEY_F10: return SDL_SCANCODE_F10;
	default:
		if (key >= 0x20 && key < 0x7f)
			return SDL_GetScancodeFromKey((SDL_Keycode)key);
		return SDL_SCANCODE_UNKNOWN;
	}
}

static int
sdl2_is_key_down(void *opaque, int key)
{
	struct beui_sdl2_context *context;
	const uint8_t *keyboard;
	SDL_Scancode scancode;

	context = opaque;
	if (context == NULL || context->window == NULL)
		return -1;
	SDL_PumpEvents();
	keyboard = SDL_GetKeyboardState(NULL);
	if (key == NOCT_BEUI_KEY_SHIFT)
		return keyboard[SDL_SCANCODE_LSHIFT] ||
			keyboard[SDL_SCANCODE_RSHIFT];
	scancode = sdl2_key_scancode(key);
	if (scancode == SDL_SCANCODE_UNKNOWN)
		return -1;
	return keyboard[scancode] != 0;
}

static void
sdl2_drain_input(void *opaque)
{
	(void)opaque;
	SDL_PumpEvents();
	SDL_FlushEvents(SDL_KEYDOWN, SDL_KEYUP);
}

bool
noct_register_api_beui_sdl2(NoctEnv *env)
{
	memset(&sdl2_hal, 0, sizeof(sdl2_hal));
	sdl2_hal.display.context = &sdl2_context;
	sdl2_hal.display.enter = sdl2_enter;
	sdl2_hal.display.leave = sdl2_leave;
	sdl2_hal.display.poll_events = sdl2_poll_events;
	sdl2_hal.display.fill = sdl2_fill;
	sdl2_hal.display.line = sdl2_line;
	sdl2_hal.display.pattern_fill = sdl2_pattern_fill;
	sdl2_hal.display.draw_image = sdl2_draw_image;
	sdl2_hal.display.draw_image_pattern = sdl2_draw_image_pattern;
	sdl2_hal.display.flush = sdl2_flush;
	sdl2_hal.glyph.context = &sdl2_context;
	sdl2_hal.glyph.measure = sdl2_glyph_measure;
	sdl2_hal.glyph.draw = sdl2_glyph_draw;
	sdl2_hal.pointer.context = &sdl2_context;
	sdl2_hal.pointer.start = sdl2_pointer_start;
	sdl2_hal.pointer.stop = sdl2_pointer_stop;
	sdl2_hal.pointer.poll = sdl2_pointer_poll;
	sdl2_hal.clock.context = &sdl2_context;
	sdl2_hal.clock.milliseconds = sdl2_milliseconds;
	sdl2_hal.audio.context = &sdl2_context;
	sdl2_hal.audio.start = sdl2_audio_start;
	sdl2_hal.audio.stop = sdl2_audio_stop;
	sdl2_hal.audio.poll = sdl2_audio_poll;
	sdl2_hal.audio.write = sdl2_audio_write;
	sdl2_hal.input.context = &sdl2_context;
	sdl2_hal.input.is_key_down = sdl2_is_key_down;
	sdl2_hal.input.drain = sdl2_drain_input;
	return noct_register_api_beui_with_hal(env, &sdl2_hal);
}
