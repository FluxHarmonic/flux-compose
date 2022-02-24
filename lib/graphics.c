#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <cglm/cglm.h>
#include <flux-internal.h>
#include <flux.h>
#include <glad/glad.h>
#include <inttypes.h>
#include <math.h>
#include <mesche.h>
#include <stdlib.h>
#include <string.h>

static char output_image_path[1024];
static char flux_thumbnail_str[100];

// When set to a value, this scene will be used in the next frame
// TODO: Don't depend on a global!
static Scene *next_scene = NULL;

// model: affecting the shape and translation of the object
// view: affecting the position of the camera, possibly scale (make camera lens bigger/smaller)
// projection: projecting to screen coordinates

const char *DefaultVertexShaderText =
    GLSL(layout(location = 0) in vec2 a_vec;

         uniform mat4 model; uniform mat4 view; uniform mat4 projection;

         void main() { gl_Position = projection * view * model * vec4(a_vec, 0.0, 1.0); });

const char *DefaultFragmentShaderText =
    GLSL(uniform vec4 color = vec4(1.0, 1.0, 1.0, 1.0); void main() { gl_FragColor = color; });

const char *TexturedVertexShaderText =
    GLSL(layout(location = 0) in vec2 position; layout(location = 1) in vec2 tex_uv;

         uniform mat4 model; uniform mat4 view; uniform mat4 projection;

         out vec2 tex_coords;

         void main() {
           tex_coords = tex_uv;
           gl_Position = projection * view * model * vec4(position, 0.0, 1.0);
         });

const char *TexturedFragmentShaderText =
    GLSL(in vec2 tex_coords;

         uniform sampler2D tex0; uniform vec4 color = vec4(1.0, 1.0, 1.0, 1.0);

         void main() { gl_FragColor = texture(tex0, tex_coords) * color; });

uint8_t flux_graphics_initialized = 0;

struct _FluxRenderContext {
  vec2 screen_size;
  vec2 desired_size;
  mat4 screen_matrix;
  mat4 view_matrix;
};

struct _FluxWindow {
  float *width, *height;
  struct _FluxRenderContext context;
  bool is_resizing;
  GLFWwindow *glfwWindow;
};

// TODO: Eventually store this in the scripting layer memory
FluxWindow preview_window;

void glfw_error_callback(int error, const char *description) {
  flux_log("GLFW error %d: %s\n", error, description);
}

void flux_graphics_window_size_set(FluxWindow window, int width, int height) {
  window->is_resizing = true;
  glfwSetWindowSize(window->glfwWindow, width, height);
}

void flux_graphics_window_size_update(FluxWindow window, int width, int height) {
  // Get the current framebuffer size
  glfwGetFramebufferSize(window->glfwWindow, &width, &height);

  *window->width = width;
  *window->height = height;

  // Update the viewport and recalculate projection matrix
  glViewport(0, 0, width, height);
  glm_ortho(0.f, *window->width, *window->height, 0.f, -1.f, 1.f, window->context.screen_matrix);

  // Window is no longer resizing
  window->is_resizing = false;
}

void flux_graphics_window_size_callback(GLFWwindow *glfwWindow, int width, int height) {
  FluxWindow window;

  flux_log("Window size changed: %dx%d\n", width, height);

  window = glfwGetWindowUserPointer(glfwWindow);

  if (!window) {
    flux_log("Cannot get FluxWindow from GLFWwindow!\n");
    return;
  }

  // Update the screen size and projection matrix
  flux_graphics_window_size_update(window, width, height);
}

FluxWindow flux_graphics_window_create(int width, int height, const char *title) {
  FluxWindow window;
  GLFWwindow *glfwWindow;

  // Make the window float
  glfwWindowHint(GLFW_FLOATING, GLFW_TRUE);
  glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

  // Enable anti-aliasing
  glfwWindowHint(GLFW_SAMPLES, 4);

  // Create the window
  glfwWindow = glfwCreateWindow(width, height, title, NULL, NULL);
  if (!glfwWindow) {
    flux_log("Could not create GLFW window!\n");
    return NULL;
  }

  window = malloc(sizeof(struct _FluxWindow));
  window->glfwWindow = glfwWindow;
  window->width = &window->context.screen_size[0];
  window->height = &window->context.screen_size[1];
  window->context.desired_size[0] = width;
  window->context.desired_size[1] = height;
  window->is_resizing = false;

  *window->width = width;
  *window->height = height;

  // Set the "user pointer" of the GLFW window to our window
  glfwSetWindowUserPointer(glfwWindow, window);

  // Respond to window size changes
  glfwSetWindowSizeCallback(glfwWindow, flux_graphics_window_size_callback);

  // Make the window's context current before loading OpenGL DLLs
  glfwMakeContextCurrent(glfwWindow);

  // Bind to OpenGL functions
  if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
    PANIC("Failed to initialize GLAD!");
    return NULL;
  }

  flux_log("OpenGL Version %d.%d loaded\n", GLVersion.major, GLVersion.minor);

  return window;
}

void flux_graphics_window_show(FluxWindow window) {
  if (window && window->glfwWindow) {
    glfwShowWindow(window->glfwWindow);
  }
}

void flux_graphics_window_destroy(FluxWindow window) {
  if (window != NULL) {
    glfwDestroyWindow(window->glfwWindow);
    window->glfwWindow = NULL;
    free(window);
  }
}

void flux_graphics_render_loop_key_callback(GLFWwindow *window, int key, int scancode, int action,
                                            int mods) {
}

GLuint flux_graphics_shader_compile(const FluxShaderFile *shader_files, uint32_t shader_count) {
  GLuint shader_id = 0;
  GLuint shader_program;
  shader_program = glCreateProgram();

  // Compile the shader files
  for (GLuint i = 0; i < shader_count; i++) {
    shader_id = glCreateShader(shader_files[i].shader_type);
    glShaderSource(shader_id, 1, &shader_files[i].shader_text, NULL);
    glCompileShader(shader_id);

    int success = 0;
    char infoLog[512];
    glGetShaderiv(shader_id, GL_COMPILE_STATUS, &success);
    if (success == GL_FALSE) {
      glGetShaderInfoLog(shader_id, 512, NULL, infoLog);
      PANIC("Shader compilation failed:\n%s\n", infoLog);
    }

    glAttachShader(shader_program, shader_id);
  }

  // Link the full program
  glLinkProgram(shader_program);

  return shader_program;
}

void flux_graphics_shader_mat4_set(unsigned int shader_program_id, const char *uniform_name,
                                   mat4 matrix) {
  unsigned int uniformLoc = glGetUniformLocation(shader_program_id, uniform_name);
  if (uniformLoc == -1) {
    PANIC("Could not find shader matrix parameter: %s\n", uniform_name);
  }

  glUniformMatrix4fv(uniformLoc, 1, GL_FALSE, matrix[0]);
}

void flux_graphics_draw_rect(FluxWindow window, float x, float y, float width, float height) {
  // TODO: This needs a fragment shader to render correctly

  // A different approach with a vertex shader
  // https://stackoverflow.com/a/60440937
}

void flux_graphics_draw_rect_fill(FluxRenderContext context, float x, float y, float w, float h,
                                  vec4 color) {
  static GLuint shader_program = 0;
  static GLuint rect_vertex_array = 0;
  static GLuint rect_vertex_buffer = 0;
  static GLuint rect_element_buffer = 0;

  if (shader_program == 0) {
    const FluxShaderFile shader_files[] = {
        {GL_VERTEX_SHADER, DefaultVertexShaderText},
        {GL_FRAGMENT_SHADER, DefaultFragmentShaderText},
    };
    shader_program = flux_graphics_shader_compile(shader_files, 2);
  }

  // Use the shader
  glUseProgram(shader_program);

  if (rect_vertex_array == 0) {
    glGenVertexArrays(1, &rect_vertex_array);
    glGenBuffers(1, &rect_vertex_buffer);
    glGenBuffers(1, &rect_element_buffer);

    float vertices[] = {
        // Positions
        0.5f,  0.5f,  // top right
        0.5f,  -0.5f, // bottom right
        -0.5f, -0.5f, // bottom left
        -0.5f, 0.5f,  // top left
    };

    unsigned int indices[] = {
        0, 1, 2, // first triangle
        2, 3, 0  // second triangle
    };

    glBindVertexArray(rect_vertex_array);

    glBindBuffer(GL_ARRAY_BUFFER, rect_vertex_buffer);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, rect_element_buffer);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), 0);
    glEnableVertexAttribArray(0);
  } else {
    glBindVertexArray(rect_vertex_array);
  }

  // Model matrix is scaled to size and translated
  mat4 model;
  glm_translate_make(model, (vec3){x + (w / 2.f), y + (h / 2.f), 0.f});
  glm_scale(model, (vec3){w, h, 0.f});

  // Set the uniforms
  glUniformMatrix4fv(glGetUniformLocation(shader_program, "projection"), 1, GL_FALSE,
                     (float *)context->screen_matrix);
  glUniformMatrix4fv(glGetUniformLocation(shader_program, "view"), 1, GL_FALSE,
                     (float *)context->view_matrix);
  glUniformMatrix4fv(glGetUniformLocation(shader_program, "model"), 1, GL_FALSE, (float *)model);
  glUniform4fv(glGetUniformLocation(shader_program, "color"), 1, (float *)color);

  // Draw all 6 indices in the element buffer
  glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
}

void flux_graphics_draw_args_scale(FluxDrawArgs *args, float scale_x, float scale_y) {
  if (scale_x != 0 || scale_y != 0) {
    args->scale_x = scale_x;
    args->scale_y = scale_y;
    args->flags |= FluxDrawScaled;
  }
}

void flux_graphics_draw_args_rotate(FluxDrawArgs *args, float rotation) {
  if (rotation != 0.0) {
    args->rotation = rotation;
    args->flags |= FluxDrawRotated;
  }
}

void flux_graphics_draw_args_center(FluxDrawArgs *args, bool centered) {
  if (centered) {
    args->flags |= FluxDrawCentered;
  }
}

void flux_graphics_draw_texture_ex(FluxRenderContext context, FluxTexture texture, float x, float y,
                                   FluxDrawArgs *args) {
  GLuint shader_program = 0;
  static GLuint default_shader_program = 0;
  static GLuint rect_vertex_array = 0;
  static GLuint rect_vertex_buffer = 0;
  static GLuint rect_element_buffer = 0;

  if (args != NULL) {
    shader_program = args->shader_program;
  }

  // Use the default texture shader if one isn't specified
  if (shader_program == 0) {
    if (default_shader_program == 0) {
      const FluxShaderFile shader_files[] = {
          {GL_VERTEX_SHADER, TexturedVertexShaderText},
          {GL_FRAGMENT_SHADER, TexturedFragmentShaderText},
      };
      default_shader_program = flux_graphics_shader_compile(shader_files, 2);
    }

    shader_program = default_shader_program;
  }

  // Use the shader
  glUseProgram(shader_program);

  if (rect_vertex_array == 0) {
    glGenVertexArrays(1, &rect_vertex_array);
    glGenBuffers(1, &rect_vertex_buffer);
    glGenBuffers(1, &rect_element_buffer);

    // Texture coordinates are 0,0 for bottom left and 1,1 for top right
    float vertices[] = {
        // Positions   // Texture
        0.5f,  0.5f,  1.f, 1.f, // top right
        0.5f,  -0.5f, 1.f, 0.f, // bottom right
        -0.5f, -0.5f, 0.f, 0.f, // bottom left
        -0.5f, 0.5f,  0.f, 1.f, // top left
    };

    unsigned int indices[] = {
        0, 1, 2, // first triangle
        2, 3, 0  // second triangle
    };

    glBindVertexArray(rect_vertex_array);

    glBindBuffer(GL_ARRAY_BUFFER, rect_vertex_buffer);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, rect_element_buffer);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), 0);

    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (2 * sizeof(float)));
  } else {
    glBindVertexArray(rect_vertex_array);
  }

  // Adjust position if texture shouldn't be drawn centered
  if (args && (args->flags & FluxDrawCentered) == 0) {
    x += texture->width / 2.f;
    y += texture->height / 2.f;
  }

  float scale_x = 1.f, scale_y = 1.f;
  if (args && (args->flags & FluxDrawScaled) == FluxDrawScaled) {
    scale_x = args->scale_x;
    scale_y = args->scale_y;
  }

  // Model matrix is scaled to size and translated
  mat4 model;
  glm_translate_make(model, (vec3){x * scale_x, y * scale_y, 0.f});
  glm_scale(model, (vec3){texture->width * scale_x, texture->height * scale_y, 0.f});

  // Rotate and scale as requested
  if (args && (args->flags & FluxDrawRotated) == FluxDrawRotated) {
    glm_rotate(model, glm_rad(args->rotation), (vec3){0.f, 0.f, 1.f});
  }

  mat4 view;
  glm_mat4_identity(view);

  // Set the uniforms
  vec4 color = {1.f, 1.f, 1.f, 1.f};
  glUniformMatrix4fv(glGetUniformLocation(shader_program, "projection"), 1, GL_FALSE,
                     (float *)context->screen_matrix);
  glUniformMatrix4fv(glGetUniformLocation(shader_program, "view"), 1, GL_FALSE,
                     (float *)context->view_matrix);
  glUniformMatrix4fv(glGetUniformLocation(shader_program, "model"), 1, GL_FALSE, (float *)model);
  glUniform4fv(glGetUniformLocation(shader_program, "color"), 1, (float *)color);

  // Bind the texture
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, texture->texture_id);
  glUniform1i(glGetUniformLocation(shader_program, "tex0"), 0);

  // Draw all 6 indices in the element buffer
  glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

  // Reset the texture
  glBindTexture(GL_TEXTURE_2D, 0);
}

void flux_graphics_draw_texture(FluxRenderContext context, FluxTexture texture, float x, float y) {
  flux_graphics_draw_texture_ex(context, texture, x, y, NULL);
}

void flux_graphics_save_to_png(FluxWindow window, const char *output_file_path) {
  int i = 0;
  unsigned char *screen_bytes = NULL;
  unsigned char *image_bytes = NULL;
  size_t image_row_length = 4 * *window->width;
  size_t image_data_size = sizeof(*image_bytes) * image_row_length * *window->height;

  /* flux_log("Rendering window of size %u / %u to file: %s\n", *window->width, *window->height, */
  /*          output_file_path); */

  // Allocate storage for the screen bytes
  // TODO: reduce memory allocation requirements
  screen_bytes = malloc(image_data_size);
  image_bytes = malloc(image_data_size);

  // TODO: Switch context to this window

  // Store the screen contents to a byte array
  glReadPixels(0, 0, *window->width, *window->height, GL_RGBA, GL_UNSIGNED_BYTE, screen_bytes);

  // Flip the rows of the byte array because OpenGL's coordinate system is flipped
  for (i = 0; i < *window->height; i++) {
    memcpy(&image_bytes[image_row_length * i],
           &screen_bytes[image_row_length * ((int)*window->height - (i + 1))],
           sizeof(*image_bytes) * image_row_length);
  }

  // Save image data to a PNG file
  flux_texture_png_save(output_file_path, image_bytes, *window->width, *window->height);

  // Clean up the memory
  free(image_bytes);
  free(screen_bytes);
}

void flux_graphics_render_test(FluxRenderContext context) {
  static FluxTexture logo = NULL;
  static FluxFont jost_font = NULL;

  static FluxDrawArgs draw_args;
  float x, y, scale, amt = 0.f;

  if (!logo) {
    logo = flux_texture_png_load("assets/flux_icon.png");
  }

  // Load a font
  if (!jost_font) {
    char *font_name = "Jost SemiBold";
    char *font_path = flux_font_resolve_path(font_name);
    flux_log("Resolved font path: %s\n", font_path);
    if (!font_path) {
      flux_log("Could not find a file for font: %s\n", font_name);
    } else {
      // Load the font and free the allocation font path
      jost_font = flux_font_load_file(font_path, 100);
      free(font_path);
      font_path = NULL;
    }
  }

  x = 100 + (sin(glfwGetTime() * 7.f) * 100.f);
  y = 100 + (cos(glfwGetTime() * 7.f) * 100.f);

  // TODO: Render some stuff
  flux_graphics_draw_rect_fill(context, x, y, 500, 400, (vec4){1.0, 0.0, 0.0, 1.0});
  flux_graphics_draw_rect_fill(context, 300, 300, 500, 400, (vec4){0.f, 1.f, 0.f, 0.5f});

  // Apply transforms before rendering
  amt = sin(glfwGetTime() * 5.f);
  scale = 1.0 + amt * 0.3;
  flux_graphics_draw_args_scale(&draw_args, scale, scale);
  flux_graphics_draw_args_rotate(&draw_args, amt * 0.1 * 180);

  // Render a texture
  flux_graphics_draw_texture_ex(context, logo, 950, 350, &draw_args);

  // Draw some text if the font got loaded
  if (jost_font) {
    flux_font_draw_text(context, jost_font, "February 3, 2022", 20, 20);
  }
}

void flux_graphics_render_thumbnail(FluxRenderContext context) {
  static FluxTexture logo = NULL;
  static FluxTexture background = NULL;
  static FluxFont jost_font = NULL;

  float scale, amt = 1.f;
  FluxDrawArgs draw_args;
  draw_args.shader_program = 0;

  if (!logo) {
    logo = flux_texture_png_load("/home/daviwil/Notes/Shows/FluxHarmonic/Media/Flux Harmonic.png");
  }
  if (!background) {
    background = flux_texture_png_load(
        "/home/daviwil/Notes/Shows/FluxHarmonic/Media/Flux-Harmonic-BG-Base.png");
  }

  // Load a font
  if (!jost_font) {
    char *font_name = "Jost SemiBold";
    char *font_path = flux_font_resolve_path(font_name);
    flux_log("Resolved font path: %s\n", font_path);
    if (!font_path) {
      flux_log("Could not find a file for font: %s\n", font_name);
    } else {
      // Load the font and free the allocation font path
      jost_font = flux_font_load_file(font_path, 200);
      free(font_path);
      font_path = NULL;
    }
  }

  // Apply transforms before rendering

  // Render the thumbnail
  if (background) {
    scale = context->desired_size[0] / logo->width;
    flux_graphics_draw_args_scale(&draw_args, scale, scale);
    flux_graphics_draw_args_center(&draw_args, false);
    flux_graphics_draw_texture_ex(context, background, 0, 0, &draw_args);
  }

  flux_graphics_draw_rect_fill(context, 0, 0, context->desired_size[0], context->desired_size[1],
                               (vec4){28.f / 255, 30.f / 255, 31.f / 255, 0.93});

  scale = 1700.f / logo->width;
  flux_graphics_draw_args_scale(&draw_args, scale, scale);
  flux_graphics_draw_args_center(&draw_args, false);
  flux_graphics_draw_texture_ex(context, logo, context->desired_size[0] / 2.f - 500,
                                context->desired_size[1] / 5.f, &draw_args);
  /* flux_graphics_draw_texture_ex(context, logo, 2500 / 2.f, 0, &draw_args); */

  // Draw some text if the font got loaded
  if (jost_font) {
    flux_font_draw_text(context, jost_font, flux_thumbnail_str, 405,
                        context->desired_size[1] - 150);
  }
}

void flux_graphics_loop_start(FluxWindow window, MescheRepl *repl) {
  float amt, scale;
  Scene *current_scene = NULL;
  FluxRenderContext context = &window->context;
  GLFWwindow *glfwWindow = window->glfwWindow;

  // TODO: Is this the best place for this?
  // Register a key callback for input handling
  /* glfwSetKeyCallback(window, key_callback); */

  // Initialize the viewport
  flux_graphics_window_size_update(window, *window->width, *window->height);

  // Set the swap interval to prevent tearing
  glfwSwapInterval(1);

  // Enable blending
  glEnable(GL_BLEND);
  glBlendEquationSeparate(GL_FUNC_ADD, GL_FUNC_ADD);
  glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ZERO);

  // Enable textures
  glEnable(GL_TEXTURE_2D);

  // Enable multisampling (anti-aliasing)
  glEnable(GL_MULTISAMPLE);

  while (!glfwWindowShouldClose(glfwWindow)) {
    // Poll for events for this frame
    glfwPollEvents();

    // Read from the REPL asynchronously
    if (repl != NULL) {
      mesche_repl_poll(repl);
    }

    // Clear the screen
    glClearColor(0.0, 0.0, 0.0, 1.0);
    glClear(GL_COLOR_BUFFER_BIT);

    // Translate the scene preview to the appropriate position, factoring in the
    // scaled size of the scene
    float scale = 1.0f;
    glm_mat4_identity(context->view_matrix);
    /* glm_translate(context->view_matrix, (vec3){(*window->width / 2) - (1280 / 2.f), */
    /*                                            (*window->height / 2) - (720 / 2.f), 0.f}); */
    glm_scale(context->view_matrix, (vec3){scale, scale, 1.f});

    // Draw the preview area rect
    /* flux_graphics_draw_rect_fill(context, -1.f, -1.f, 1280 + 1.f, 720 + 1.f, (vec4) { 1.0, 1.0,
     * 0.0, 1.0 }); */

    // Should we switch to a new scene?
    if (next_scene) {
      // Resize the window to fit the scene
      flux_graphics_window_size_set(window, next_scene->width, next_scene->height);

      // Set the current scene to be rendered
      current_scene = next_scene;
      next_scene = NULL;
    }

    // Render the scene
    if (current_scene) {
      flux_scene_render(context, current_scene);
    }

    // Swap the render buffers
    glfwSwapBuffers(glfwWindow);

    // Render the screen to a file if requested and the window isn't waiting for resize
    if (output_image_path[0] != '\0' && !window->is_resizing) {
      flux_log("Saving image to path: %s\n", output_image_path);
      flux_graphics_save_to_png(window, output_image_path);
      output_image_path[0] = '\0';
    }

    // If the there's no reason to keep the loop open, exit immediately
    if (!repl) {
      break;
    }
  }

  flux_log("Render loop is exiting...\n");

  return;
}

int flux_graphics_init() {
  if (flux_graphics_initialized == 0) {
    flux_graphics_initialized = 1;

    // Make sure we're notified about errors
    glfwSetErrorCallback(glfw_error_callback);

    if (!glfwInit()) {
      flux_log("GLFW failed to init!\n");
      return 1;
    }

    // Set OpenGL version and profile
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);

#ifdef __APPLE__
    // Just in case...
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    // Make sure all new windows are hidden by default
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
  } else {
    flux_log("Graphics subsystem already initialized...\n");
  }

  return 0;
}

void flux_graphics_end(void) {
  glfwTerminate();
}

Value flux_graphics_func_show_preview_window(MescheMemory *mem, int arg_count, Value *args) {
  if (arg_count != 2) {
    flux_log("Function requires 2 number parameters.");
  }

  int width = AS_NUMBER(args[0]);
  int height = AS_NUMBER(args[1]);

  FluxWindow window = (FluxWindow)((VM *)mem)->app_context;
  glfwSetWindowSize(window->glfwWindow, width, height);
  flux_graphics_window_show(window);

  // TODO: Return preview_window as a pointer
  return T_VAL;
}

Value flux_graphics_func_render_to_file(MescheMemory *mem, int arg_count, Value *args) {
  if (arg_count != 1) {
    flux_log("Function requires a path to the output file.");
  }

  char *file_path = AS_CSTRING(args[0]);
  flux_log("Received request to save image: %s\n", file_path);
  memcpy(output_image_path, file_path, strlen(file_path));
}

Value flux_graphics_func_flux_harmonic_thumbnail(MescheMemory *mem, int arg_count, Value *args) {
  if (arg_count != 1) {
    flux_log("Function requires a string for the date.");
  }

  char *date_str = AS_CSTRING(args[0]);
  flux_log("Received request to render thumbnail: %s\n", date_str);
  memcpy(flux_thumbnail_str, date_str, strlen(date_str));
}

Value flux_graphics_func_graphics_scene_set(MescheMemory *mem, int arg_count, Value *args) {
  if (arg_count != 1) {
    flux_log("Function requires a scene to set.");
  }

  ObjectPointer *scene_ptr = AS_POINTER(args[0]);
  next_scene = (Scene *)scene_ptr->ptr;
}
