CFLAGS = -g -Wall `pkg-config --cflags gtk+-2.0`
LDFLAGS = `pkg-config --libs gtk+-2.0`

DEST=/usr/local/bin

all: img

img: img.o
	$(CC) $(CFLAGS) -o img img.o $(LDFLAGS)

install: all
	install -c -m 755 img $(DEST)

uninstall:
	rm -f $(DEST)/img

clean:
	rm -f img *.o *~
