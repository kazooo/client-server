CFLAGS=-std=c++11 -Wall -Wextra -pedantic
VAL=
CLIENT=ftrest
SERVER=ftrestd
CC=g++
RM=rm -rf

all: client server

client:
	$(CC) $(CFLAGS) client.cpp -o $(CLIENT)

server:
	$(CC) $(CFLAGS) server.cpp -o $(SERVER)

clean:
	$(RM) $(CLIENT) $(SERVER)


# mkd http://localhost:1233/bar
# put http://localhost:1233/bar/readme.md ./readme.md
# get http://localhost:1233/bar/out.md
# del http://localhost:1233/bar/readme.md
# rmd http://localhost:1233/bar
# lst http://localhost:1233/
