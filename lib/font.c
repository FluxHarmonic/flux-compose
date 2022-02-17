#define GLFW_INCLUDE_NONE

#include <GLFW/glfw3.h>
#include <flux-internal.h>
#include <flux.h>
#include <fontconfig/fontconfig.h>
#include <ft2build.h>
#include <glad/glad.h>
#include <inttypes.h>
#include <mesche.h>
#include <stdbool.h>
#include <string.h>
#include FT_FREETYPE_H

#define ASCII_CHAR_START 32
#define ASCII_CHAR_END 126

typedef struct {
  struct _FluxTexture texture;
  int32_t bearing_x;
  int32_t bearing_y;
  uint32_t advance;
} FluxFontChar;

struct _FluxFont {
  FluxFontChar chars[ASCII_CHAR_END - ASCII_CHAR_START];
};

const char *FontVertexShaderText =
    GLSL(layout(location = 0) in vec2 position; layout(location = 1) in vec2 tex_uv;

         uniform mat4 model; uniform mat4 view; uniform mat4 projection;

         out vec2 tex_coords;

         void main() {
           tex_coords = tex_uv;
           gl_Position = projection * view * model * vec4(position, 0.0, 1.0);
         });

const char *FontFragmentShaderText =
    GLSL(in vec2 tex_coords;

         uniform sampler2D tex0; uniform vec4 color = vec4(1.0, 1.0, 1.0, 1.0);

         void main() {
           vec4 sampled = vec4(1.0, 1.0, 1.0, texture(tex0, tex_coords).r);
           gl_FragColor = color * sampled;
         });

FluxFont flux_font_load_file(const char *font_path, int font_size) {
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
    flux_log("Could not load font: %s\n", font_path);
    return NULL;
  } else {
    flux_log("Face \"%s\" has %ld glyphs\n", face->family_name, face->num_glyphs);
  }

  // Specify the size of the face needed
  FT_Set_Pixel_Sizes(face, 0, font_size);

  // Initialize the font in memory
  FluxFont flux_font = malloc(sizeof(struct _FluxFont));
  memset(flux_font, 0, sizeof(*flux_font));

  // Remove alignment restriction
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

  // Does the font have kerning?
  /* flux_log("Has kerning: %d\n", FT_HAS_KERNING(face)); */

  // Load glyphs for each character
  for (char_id = ASCII_CHAR_START; char_id < ASCII_CHAR_END; char_id++) {
    // Load the character glyph and information
    /* flux_log("Loading glyph for char: %c\n", char_id); */
    if (FT_Load_Char(face, char_id, FT_LOAD_RENDER)) {
      flux_log("Failed to load Glyph: %c\n", char_id);
      return -1;
    }

    // Assign glyph metrics
    current_char = &flux_font->chars[char_id - ASCII_CHAR_START];
    current_char->texture.width = face->glyph->bitmap.width;
    current_char->texture.height = face->glyph->bitmap.rows;
    current_char->bearing_x = face->glyph->bitmap_left;
    current_char->bearing_y = face->glyph->bitmap_top;
    current_char->advance = face->glyph->advance.x;

    // Create the texture and copy the glyph bitmap into it
    glGenTextures(1, &current_char->texture.texture_id);
    glBindTexture(GL_TEXTURE_2D, current_char->texture.texture_id);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, current_char->texture.width,
                 current_char->texture.height, 0, GL_RED, GL_UNSIGNED_BYTE,
                 face->glyph->bitmap.buffer);

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

void flux_font_draw_text(FluxRenderContext context, FluxFont font, const char *text, float pos_x,
                         float pos_y) {
  float x, y;
  uint8_t i = 0;
  uint8_t num_chars = 0;
  FluxFontChar *current_char = NULL;
  static FluxDrawArgs draw_args;

  if (draw_args.shader_program == 0) {
    const FluxShaderFile shader_files[] = {
        {GL_VERTEX_SHADER, FontVertexShaderText},
        {GL_FRAGMENT_SHADER, FontFragmentShaderText},
    };

    draw_args.shader_program = flux_graphics_shader_compile(shader_files, 2);
  }

  num_chars = strlen(text);

  for (i = 0; i < num_chars; i++) {
    // Get the char information
    current_char = font->chars + (text[i] - ASCII_CHAR_START);

    x = pos_x + current_char->bearing_x;
    y = pos_y - current_char->bearing_y;

    flux_graphics_draw_texture_ex(context, &current_char->texture, x, y, &draw_args);

    pos_x += current_char->advance >> 6;
  }

  glBindTexture(GL_TEXTURE_2D, 0);
}

char *flux_font_resolve_path(const char *font_name) {
  char *font_path = NULL;

  // Initialize fontconfig library
  FcConfig *config = FcInitLoadConfigAndFonts();

  // Configure the search pattern
  FcPattern *pattern = FcNameParse((FcChar8 *)font_name);
  FcConfigSubstitute(config, pattern, FcMatchPattern);
  FcDefaultSubstitute(pattern);

  // Find a font that matches the query pattern
  FcResult result;
  FcPattern *font = FcFontMatch(config, pattern, &result);
  if (font) {
    FcChar8 *file_path = NULL;
    if (FcPatternGetString(font, FC_FILE, 0, &file_path) == FcResultMatch) {
      font_path = strdup((char *)file_path);
    }
  }

  FcPatternDestroy(font);
  FcPatternDestroy(pattern);
  FcConfigDestroy(config);

  return font_path;
}

void flux_font_print_all(const char *family_name) {
  // Initialize fontconfig library
  FcConfig *config = FcInitLoadConfigAndFonts();

  // Create a pattern to find all fonts
  FcPattern *pattern = NULL;
  if (family_name) {
    pattern = FcNameParse((FcChar8 *)family_name);
  } else {
    pattern = FcPatternCreate();
  }

  // Build a font set from the pattern
  FcObjectSet *object_set = FcObjectSetBuild(FC_FAMILY, FC_STYLE, FC_LANG, FC_FILE, (char *)0);
  FcFontSet *font_set = FcFontList(config, pattern, object_set);

  if (font_set) {
    flux_log("Font count: %d\n", font_set->nfont);
    for (int i = 0; font_set && i < font_set->nfont; ++i) {
      FcPattern *font = font_set->fonts[i];
      FcChar8 *file, *style, *family;
      if (FcPatternGetString(font, FC_FILE, 0, &file) == FcResultMatch &&
          FcPatternGetString(font, FC_FAMILY, 0, &family) == FcResultMatch &&
          FcPatternGetString(font, FC_STYLE, 0, &style) == FcResultMatch) {
        flux_log("Font path: %s (family %s, style %s)\n", file, family, style);
      }
    }

    FcFontSetDestroy(font_set);
    FcObjectSetDestroy(object_set);
  } else {
    flux_log("No fonts found!\n");
  }

  if (pattern) {
    FcPatternDestroy(pattern);
  }

  FcConfigDestroy(config);
}

Value flux_graphics_func_load_font_internal(MescheMemory *mem, int arg_count, Value *args) {
  if (arg_count != 3) {
    flux_log("Function requires 3 parameters.");
  }

  FluxFont *font = NULL;

  char *family = AS_CSTRING(args[0]);
  char *weight = AS_CSTRING(args[1]);
  double size = AS_NUMBER(args[3]);

  char font_spec[100];

  sprintf(font_spec, "%s %s", family, weight);

  char *font_path = flux_font_resolve_path(font_spec);
  flux_log("Resolved font path: %s\n", font_path);
  if (!font_path) {
    flux_log("Could not find a file for font: %s\n", font_spec);
  } else {
    // Load the font and free the allocation font path
    font = flux_font_load_file(font_path, (int)size);
    free(font_path);
    font_path = NULL;
  }

  return OBJECT_VAL(mesche_object_make_pointer((VM *)mem, font, true));
}
