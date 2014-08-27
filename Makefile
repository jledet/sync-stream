CC ?= clang

SERVER := server
SERVER_SRC := server.c

CLIENT := client
CLIENT_SRC := client.c

CFLAGS := -Wall
INCLUDES := -I/usr/include/gstreamer-1.0 -I/usr/include/glib-2.0 -I/usr/lib/glib-2.0/include
LIBS := -lpthread -ldl -lgstreamer-1.0 -lgobject-2.0 -lglib-2.0 -lgstnet-1.0

all: build

.PHONY: build
build: $(SERVER) $(CLIENT)

$(SERVER): $(SERVER_SRC)
	$(CC) $(CFLAGS) $(INCLUDES) $(LIBS) -o $@ $<

$(CLIENT): $(CLIENT_SRC)
	$(CC) $(CFLAGS) $(INCLUDES) $(LIBS) -o $@ $<

.PHONY: clean
clean:
	rm $(CLIENT) $(SERVER)
