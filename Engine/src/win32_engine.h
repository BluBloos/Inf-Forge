#pragma once

#include <windows.h>

typedef struct win32_backbuffer {
    BITMAPINFO info;
	void *memory;
	int width;
	int height;
	int pitch;
	int bytesPerPixel;
} win32_backbuffer_t;