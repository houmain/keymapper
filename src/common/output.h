#pragma once

extern bool g_verbose_output;
extern bool g_output_color;

void message(const char* format, ...);
void error(const char* format, ...);
void verbose(const char* format, ...);
