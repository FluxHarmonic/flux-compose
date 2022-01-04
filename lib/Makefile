CC=gcc
SDL=sdl2 SDL2_gfx
CFLAGS=`pkg-config --cflags ${SDL}` -Wall -fPIC
LIBS=`pkg-config --libs sdl2 ${SDL}` -lpthread

OUT=obj
DIRS=$(OUT) ../bin
OBJ=$(OUT)/lib.o

.PHONY: all clean

all: ../bin/libflux.so

$(OUT)/%.o: %.c
	$(CC) -c $(CFLAGS) $< -o $@

../bin/libflux.so: $(OBJ)
	${CC} $(LIBS) -shared -o ../bin/libflux.so $(OBJ)

clean:
	rm -rf $(OUT)/*

$(shell mkdir -p $(DIRS) > /dev/null)
