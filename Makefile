CFLAGS = -O -Wall

targets = udp_server udp_client

all: $(targets)

clean clobber distclean:
	rm -rf -- $(targets)
