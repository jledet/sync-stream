CC ?= gcc

SERVER := server
SERVER_SRC := server.c

CLIENT := client
CLIENT_SRC := client.c

CFLAGS := -Wall $(shell pkg-config --cflags gstreamer-1.0)
LIBS := -lpthread -ldl $(shell pkg-config --libs gstreamer-1.0) -lgstnet-1.0

all: build

.PHONY: build
build: $(SERVER) $(CLIENT)

$(SERVER): $(SERVER_SRC)
	$(CC) $(CFLAGS) -o $@ $< $(LIBS)

$(CLIENT): $(CLIENT_SRC)
	$(CC) $(CFLAGS) -o $@ $< $(LIBS)

.PHONY: clean
clean:
	rm $(CLIENT) $(SERVER)
