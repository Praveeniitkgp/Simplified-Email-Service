CC = gcc
CFLAGS = -Wall -Wextra -pthread
LDFLAGS = -pthread

all: mysmtp_server mysmtp_client

mysmtp_server: mysmtp_server.c
	$(CC) $(CFLAGS) -o mysmtp_server mysmtp_server.c $(LDFLAGS)

mysmtp_client: mysmtp_client.c
	$(CC) $(CFLAGS) -o mysmtp_client mysmtp_client.c

clean:
	rm -f mysmtp_server mysmtp_client

.PHONY: all clean