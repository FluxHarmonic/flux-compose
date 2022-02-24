# add_compile_definitions(SPNG_USE_MINIZ)

target_sources(flux PRIVATE file.c log.c mem.c scene.c vector.c
  graphics.c texture.c font.c vendor/spng/spng.c vendor/miniz/miniz.c
  vendor/glad/src/glad.c)
