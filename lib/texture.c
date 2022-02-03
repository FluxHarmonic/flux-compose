#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <flux-internal.h>
#include <flux.h>
#include <inttypes.h>
#include <spng.h>
#include <string.h>

FluxTexture flux_texture_png_load(char *file_path) {
  FILE *png;
  int ret = 0;
  int fmt = SPNG_FMT_PNG;
  unsigned int texture_id = 0;
  spng_ctx *ctx = NULL;
  size_t image_data_size = 0;
  const size_t limit = 1024 * 1024 * 64;
  struct spng_ihdr header;
  unsigned char *image_bytes = NULL;
  FluxTexture texture = NULL;

  png = flux_file_open(file_path, "rb");
  ctx = spng_ctx_new(0);

  if (png == NULL) {
    flux_log("Could not load file at path: %s\n", file_path);
  }

  if (ctx == NULL) {
    flux_log("Could not create spng context!\n");
  }

  // Configure the decoder
  spng_set_crc_action(ctx, SPNG_CRC_USE, SPNG_CRC_USE);
  spng_set_chunk_limits(ctx, limit, limit);

  // Process the file data
  spng_set_png_file(ctx, png);
  ret = spng_get_ihdr(ctx, &header);
  if (ret) {
    flux_log("Error reading PNG file header: %s\n", spng_strerror(ret));
  }

  ret = spng_decoded_image_size(ctx, fmt, &image_data_size);
  if (ret) {
  }

  // Allocate space for the image data
  image_bytes = malloc(image_data_size);

  // Decode the image data
  ret = spng_decode_image(ctx, image_bytes, image_data_size, SPNG_FMT_RGBA8, 0);
  if (ret) {
    flux_log("Error decoding PNG file data: %s\n", spng_strerror(ret));
  }

  // Create the texture in video memory
  // TODO: Add options for smoothing and mipmaps here
  glGenTextures(1, &texture_id);
  glBindTexture(GL_TEXTURE_2D, texture_id);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                  GL_NEAREST); // GL_NEAREST = no smoothing
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, header.width, header.height, 0, GL_RGBA, GL_UNSIGNED_BYTE,
               image_bytes);

  // Unbind the current texture to unblock future renders
  glBindTexture(GL_TEXTURE_2D, 0);

  // Allocate the actual FluxTexture
  texture = malloc(sizeof(struct _FluxTexture));
  texture->width = header.width;
  texture->height = header.height;
  texture->texture_id = texture_id;

  // TODO: Free the image data

  return texture;
}

void flux_texture_png_save(const char *file_path, const unsigned char *image_data,
                           const uint32_t width, const uint32_t height) {
  int ret = 0;
  FILE *out_file;
  spng_ctx *ctx = NULL;
  struct spng_ihdr header;

  out_file = flux_file_open(file_path, "wb");
  ctx = spng_ctx_new(SPNG_CTX_ENCODER);

  memset(&header, 0, sizeof(header));
  header.width = width;
  header.height = height;
  header.bit_depth = 8;
  header.color_type = SPNG_COLOR_TYPE_TRUECOLOR_ALPHA;

  spng_set_option(ctx, SPNG_ENCODE_TO_BUFFER, 0);

  ret = spng_set_ihdr(ctx, &header);

  if (ret) {
    flux_log("Error setting PNG header data: %s\n", spng_strerror(ret));
  }

  ret = spng_set_png_file(ctx, out_file);

  if (ret) {
    flux_log("Error setting PNG output file: %s\n", spng_strerror(ret));
  }

  ret = spng_encode_image(ctx, image_data, sizeof(*image_data) * 4 * width * height, SPNG_FMT_PNG,
                          SPNG_ENCODE_FINALIZE);

  if (ret) {
    flux_log("Error encoding PNG data: %s\n", spng_strerror(ret));
  }

  // TODO: Clean up context, etc

  fflush(out_file);
  fclose(out_file);
}
