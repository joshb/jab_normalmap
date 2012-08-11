CC=gcc
CFLAGS=-Wall -I/usr/X11R6/include
LDFLAGS=-pthread -L/usr/X11R6/lib -lm -lGL -lGLU -lSDL
OBJS=main.o my_endian.o pcx.o scene.o

jab_normalmap:	$(OBJS)
	$(CC) $(LDFLAGS) $(OBJS) -o jab_normalmap

clean:
	rm -f jab_normalmap
	rm -f $(OBJS)

main.o: main.c
my_endian.o: my_endian.c
pcx.o: pcx.c
scene.o: scene.c
