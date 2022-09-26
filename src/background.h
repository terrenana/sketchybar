#pragma once
#include "image.h"

struct background {
  bool enabled;
  bool overrides_height;

  int padding_left;
  int padding_right;
  int y_offset;
  uint32_t border_width;
  uint32_t corner_radius;

  CGRect bounds;
  struct image image;
  struct shadow shadow;
  struct rgba_color color;
  struct rgba_color border_color;
};

void background_init(struct background* background);
bool background_set_color(struct background* background, uint32_t color);
bool background_set_border_color(struct background* background, uint32_t color);
bool background_set_enabled(struct background* background, bool enabled);
bool background_set_height(struct background* background, uint32_t height);
bool background_set_border_width(struct background* background, uint32_t border_width);
bool background_set_corner_radius(struct background* background, uint32_t corner_radius);
bool background_set_padding_left(struct background* background, uint32_t pad);
bool background_set_padding_right(struct background* background, uint32_t pad);

void background_calculate_bounds(struct background* background, uint32_t x, uint32_t y);
void background_draw(struct background* background, CGContextRef context);

void background_clear_pointers(struct background* background);
void background_destroy(struct background* background);

void background_serialize(struct background* background, char* indent, FILE* rsp, bool detailed);
bool background_parse_sub_domain(struct background* background, FILE* rsp, struct token property, char* message);
