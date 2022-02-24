#ifndef __FLUX_INTERNAL_H
#define __FLUX_INTERNAL_H

#include <flux.h>
#include <stdio.h>

// Vector -------------------------------------------

struct _Vector {
  uint32_t length;
  void *start_item;
  size_t buffer_size;
  size_t buffer_usage;
  VectorItemSizeFunc item_size_func;
};

// Graphics -------------------------------------------

// Texture -------------------------------------------

struct _FluxTexture {
  uint32_t width;
  uint32_t height;
  uint32_t texture_id;
};

// Fonts ----------------------------------------------

extern void flux_font_print_all(const char *family_name);

// Scene ---------------------------------------------

typedef struct {
  float center_x, center_y;
  float scale;
  // TODO: Add pointer to current scene
} FluxSceneView;

#endif
