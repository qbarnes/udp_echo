#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/udp.h>
#include <sys/time.h>
#include <time.h>
#include <netdb.h>
#include <string.h>
#include <errno.h>

int
exchange_packets(int sfd, long index, int verbose)
{
	int		buflen;
	char		buf[1024];
	ssize_t		bytes_written, bytes_read;

	buflen = snprintf(buf, sizeof(buf), "hello: %ld", index);

	if (verbose) {
		struct timeval	tv;

		if (gettimeofday(&tv, NULL) == -1) {
			perror("gettimeofday failed");
			return 1;
		}

		printf("%ld.%06ld: Transmitting %d bytes: '%s'.\n",
			tv.tv_sec, tv.tv_usec, buflen, buf);
		fflush(stdout);
	}

	bytes_written = write(sfd, buf, buflen);

	if (bytes_written == -1) {
		fprintf(stderr, "Cannot write message [%s (%d)]\n",
			strerror(errno), errno);
		return 1;
	} else if (bytes_written < buflen) {
		printf("Warning: only wrote %ld bytes when "
			"writing %ld bytes\n", (long)bytes_written,
			(long)buflen);
	}

	if (verbose) {
		struct timeval	tv;

		if (gettimeofday(&tv, NULL) == -1) {
			perror("gettimeofday failed");
			return 1;
		}

		printf("%ld.%06ld: Waiting for reply. (sent %ld bytes)\n",
				tv.tv_sec, tv.tv_usec, bytes_written);
		fflush(stdout);
	}

	bytes_read = read(sfd, buf, sizeof(buf) - 1);

	if (bytes_read < 0) {
		fprintf(stderr, "Cannot read message [%s (%d)]\n",
			strerror(errno), errno);
		return 1;
	}

	buf[bytes_read+1] = '\0';

	if (verbose) {
		struct timeval	tv;

		if (gettimeofday(&tv, NULL) == -1) {
			perror("gettimeofday failed");
			return 1;
		}

		printf("%ld.%06ld: Received %d bytes: '%s'\n",
			tv.tv_sec, tv.tv_usec, buflen, buf);
		fflush(stdout);
	}

	return 0;
}


int
usage(const char *pgm, int exitval)
{
	fprintf(stderr, "Usage: %s [-{4|6}] [-q] [-d msdelay] "
		"[-i idnum] [-n iter] [-p port] "
		"{hostname|ipaddr}\n", pgm);

	fprintf(stderr, "	-4		Use IPv4 protocol (default)\n"
			"	-6		Use IPv6 protocol\n"
			"	-d msdelay	Milliseconds to delay "
						"between packets\n"
			"	-i idnum	ID number to start IDing "
						"packets with\n"
			"	-n iter		Iterations to run for\n"
			"	-q		Be quiet and don't "
						"report transfers\n"
		);

	exit(exitval);
}


int
main(int argc, char **argv)
{
	int		opt;
	long		idnum = 0;
	long		niter = 1;
	useconds_t	usdelay = 0;
	char		*port_arg = "50037";
	int		flag_4 = 0, flag_6 = 0, flag_d = 0, flag_q = 0;
	int		family = AF_INET;
	char		*hostname;
	struct addrinfo	*servres, *rp;
	struct addrinfo	gai_hints;
	int		gai_rv;
	int		sfd;
	long		i;
	int		retval = 0;

	while ((opt = getopt(argc, argv, "46d:i:n:p:q")) != -1) {
		switch(opt) {
		case '4':
			flag_4 = 1;
			family = AF_INET;
			break;
		case '6':
			flag_6 = 1;
			family = AF_INET6;
			break;
		case 'd':
			usdelay = (useconds_t)(atol(optarg) * 1000);
			flag_d = 1;
			break;
		case 'i':
			idnum = atol(optarg);
			break;
		case 'n':
			niter = atol(optarg);
			break;
		case 'p':
			port_arg = optarg;
			break;
		case 'q':
			flag_q = 1;
			break;
		default:
			usage(argv[0], 1);
		}
	}

	if (flag_4 && flag_6) {
		fprintf(stderr, "Options -4 and -6 are mutually exclusive.\n");
		usage(argv[0], 1);
	}

	if (optind+1 == argc)
		hostname = argv[optind];
	else if (optind == argc)
		hostname = "localhost";
	else
		usage(argv[0], 1);

	gai_hints = (struct addrinfo){0};
	gai_hints.ai_family   = family;
	gai_hints.ai_socktype = SOCK_DGRAM;

	gai_rv = getaddrinfo(hostname, port_arg, &gai_hints, &servres);
	if (gai_rv) {
		fprintf(stderr, "Cannot resolve '%s' [%s (%d)]\n",
			hostname, gai_strerror(gai_rv), gai_rv);
		return 1;
	}

	for (rp = servres; rp != NULL; rp = rp->ai_next) {
		if ((sfd = socket(rp->ai_family, rp->ai_socktype,
						rp->ai_protocol)) == -1) {
			fprintf(stderr, "Failed to create socket [%s (%d)]\n",
				strerror(errno), errno);
			return 1;
		}

		if (connect(sfd, rp->ai_addr, rp->ai_addrlen) != -1)
			break;
		
		if (close(sfd) == -1) {
			perror("close failed");
			return 1;
		}
	}

	if (!rp) {
		fprintf(stderr, "Could not connect to '%s'\n", hostname);
		return 1;
	}

	freeaddrinfo(servres);

	for (i = niter;(i > 0) && (retval == 0); --i) {
		retval = exchange_packets(sfd, idnum++, !flag_q);
		if (retval)
			break;

		if (flag_d && i > 1)
			usleep(usdelay);
	}

	if (close(sfd) == -1) {
		perror("close failed");
		return 1;
	}

	return retval;
}
