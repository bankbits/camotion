CFLAGS=-c -Wall -ggdb -march=armv7-a -mfpu=neon -O2 `pkg-config --cflags gtk+-3.0`
LIBS = -lm -lpil -lpthread

all: cm

cm: camera.o main.o util.o
	$(CC) main.o camera.o util.o $(LIBS) -g -o cm 

main.o: main.c
	$(CC) $(CFLAGS) main.c

camera.o: camera.c
	$(CC) $(CFLAGS) camera.c

util.o: util.c
	$(CC) $(CFLAGS) util.c

#viewer.o: viewer.c
#	$(CC) $(CFLAGS) viewer.c

clean:
	rm -rf *o cm

