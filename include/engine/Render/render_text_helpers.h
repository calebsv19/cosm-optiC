#ifndef ENGINE_RENDER_TEXT_HELPERS_H
#define ENGINE_RENDER_TEXT_HELPERS_H

#include <stddef.h>

int getTextWidth(const char* text);
int getTextWidthN(const char* text, int n);
size_t getTextClampedLength(const char* text, int maxWidth);

#endif
