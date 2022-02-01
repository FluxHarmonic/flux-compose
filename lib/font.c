#include <GLFW/glfw3.h>
#include <flux.h>
#include <ft2build.h>
#include <inttypes.h>
#include <string.h>
#include FT_FREETYPE_H

#define ASCII_CHAR_START 32
#define ASCII_CHAR_END 126

typedef struct {
  uint32_t texture_id;
  int32_t size_x;
  int32_t size_y;
  int32_t bearing_x;
  int32_t bearing_y;
  uint32_t advance;
} FluxFontChar;

struct _FluxFont {
  FluxFontChar chars[ASCII_CHAR_END - ASCII_CHAR_START];
};

FluxFont flux_font_load_file(const char *font_path, uint8_t font_size) {
  char char_id = 0;
  FluxFontChar *current_char;

  static FT_Library ft;
  if (FT_Init_FreeType(&ft)) {
    flux_log("Could not load FreeType library\n");
    return NULL;
  }

  // Load the font file with FreeType
  FT_Face face;
  if (FT_New_Face(ft, font_path, 0, &face)) {
    flux_log("Failed to load font: %s\n", font_path);
    return -1;
  }

  if (face == NULL) {
    flux_log("NO FACE\n");
  } else {
    flux_log("Face has \"%s\" %ld glyphs\n", face->family_name, face->num_glyphs);
  }

  // Specify the size of the face needed
  FT_Set_Pixel_Sizes(face, 0, font_size);

  // Initialize the font in memory
  FluxFont flux_font = malloc(sizeof(struct _FluxFont));
  memset(flux_font, 0, sizeof(*flux_font));

  // Remove alignment restriction
  // TODO: Do I need to reverse this afterward?
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

  // Load glyphs for each character
  for (char_id = ASCII_CHAR_START; char_id < ASCII_CHAR_END + 1; char_id++) {
    // Load the character glyph and information
    /* flux_log("Loading glyph for char: %c\n", char_id); */
    if (FT_Load_Char(face, char_id, FT_LOAD_RENDER)) {
      flux_log("Failed to load Glyph: %c\n", char_id);
      return -1;
    }

    // Assign glyph metrics
    current_char = &flux_font->chars[char_id - ASCII_CHAR_START];
    current_char->size_x = face->glyph->bitmap.width;
    current_char->size_y = face->glyph->bitmap.rows;
    current_char->bearing_x = face->glyph->bitmap_left;
    current_char->bearing_y = face->glyph->bitmap_top;

    flux_log("Glyph size is: %d / %d\n", current_char->size_x, current_char->size_y);

    // Create the texture and copy the glyph bitmap into it
    glGenTextures(1, &current_char->texture_id);
    glBindTexture(GL_TEXTURE_2D, current_char->texture_id);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, current_char->size_x, current_char->size_y, 0, GL_RED,
                 GL_UNSIGNED_BYTE, face->glyph->bitmap.buffer);

    // Set texture options to render the glyph correctly
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  }

  // Free the face
  // TODO: Fix this!
  /* FT_Done_Face(face); */

  return flux_font;
}

void flux_font_draw_text(FluxFont font, const char *text, float pos_x, float pos_y) {
  char c = 0;
  uint8_t i = 0;
  uint8_t num_chars = 0;
  FluxFontChar *current_char = NULL;

  num_chars = strlen(text);

  for (i = 0; i < num_chars; i++) {
    // Get the char information
    current_char = font->chars + (text[i] - ASCII_CHAR_START);
    flux_log("%d / %d\n", current_char->size_x, current_char->size_y);

    glBindTexture(GL_TEXTURE_2D, current_char->texture_id);

    glBegin(GL_QUADS);

    glTexCoord2d(0, 0);
    glVertex2f(pos_x, pos_y);

    glTexCoord2d(1.0, 0);
    glVertex2f(pos_x + current_char->size_x, pos_y);

    glTexCoord2d(1.0, 1.0);
    glVertex2f(pos_x + current_char->size_x, pos_y + current_char->size_y);

    glTexCoord2d(0, 1.0);
    glVertex2f(pos_x, pos_y + current_char->size_y);

    glEnd();

    pos_x += current_char->size_x;
  }

  glBindTexture(GL_TEXTURE_2D, 0);
}
