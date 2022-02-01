#include <flux.h>
#include <ft2build.h>
#include FT_FREETYPE_H

void *flux_font_load_file(const char* font_path) {
  FT_Library ft;
  if (FT_Init_FreeType(&ft))
  {
      flux_log("Could not load FreeType library\n");
      return NULL;
  }
}
