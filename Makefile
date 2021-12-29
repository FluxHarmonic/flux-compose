# Use GCC, if you have it installed.
CC=gcc

# Tell the C compiler where to find <libguile.h>
CFLAGS=`pkg-config --cflags guile-3.0 sdl2`

# Tell the linker what libraries to use and where to find them.
LIBS=`pkg-config --libs guile-3.0 sdl2` -lpthread

bin/flux-studio: bin/main.o
	${CC} bin/main.o ${LIBS} -o bin/flux-studio

bin/main.o: src/core/main.c
	${CC} -c ${CFLAGS} src/core/main.c -o bin/main.o
