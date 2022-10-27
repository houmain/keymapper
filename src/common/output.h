#pragma once

extern const char* about_header;
extern const char* about_footer;

extern bool g_verbose_output;
extern bool g_output_color;

void message(const char* format, ...);
void error(const char* format, ...);
void verbose(const char* format, ...);
