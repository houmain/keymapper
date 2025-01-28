#pragma once

extern const char* about_header;
extern const char* about_footer;

extern bool g_verbose_output;
extern bool g_output_color;
extern void(*g_show_notification)(const char* message);
extern void(*g_show_message_box)(const char* title, const char* message);

void message(const char* title, const char* format, ...);
void notify(const char* format, ...);
void error(const char* format, ...);
void verbose(const char* format, ...);
