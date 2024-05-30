/*
 * server.c -- a cheezy multiperson chat server
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#define PORT "9034" // port we're listening on

// get sockaddr in IPv4 or IPv6
void *get_in_addr(struct sockaddr *sa)
{
	if (sa->sa_family == AF_INET) {
		return &(((struct sockaddr_in*)sa)->sin_addr);
	}
	return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int main(void)
{
	fd_set master;		// master file descriptor list
	fd_set read_fds; 	// temp file descriptor list for select()
	int fdmax;		// maximum file descriptor number

	int listener;		// listening file descriptor
	int newfd;		// newly accept()ed socket descriptor
	struct sockaddr_storage remoteaddr; 	// client address
	socklen_t addrlen;

	char buf[256];		// buffer for client data
	int nbytes;

	char remoteIP[INET6_ADDRSTRLEN];

	int yes=1;		// for setsockopt() SO_REUSEADDR, below
	int i, j, rv;

	struct addrinfo hints, *ai, *p;

	FD_ZERO(&master);	// clear the master and temp sets
	FD_ZERO(&read_fds);

	// get a socket and bind it
	// the server will listen on this socket for connections and data
	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;

	// hints.ai_flags = AI_PASSIVE and getaddrinfo(NULL, ...) gets a socket address suitable for bind/accept
	// https://man7.org/linux/man-pages/man3/getaddrinfo.3.html
	if ((rv = getaddrinfo(NULL, PORT, &hints, &ai)) != 0) {
		fprintf(stderr, "server: %s\n", gai_strerror(rv));
		exit(1);
	}

	for (p = ai; p != NULL; p = p->ai_next) {
		// listener = socket file descriptor
		listener = socket(p->ai_family, p->ai_socktype, p-> ai_protocol);
		if (listener < 0) {
			continue;
		}

		// lose the "address already in use" error message
		setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));

		// assign the address to the listener file descriptor
		if (bind(listener, p->ai_addr, p->ai_addrlen) < 0) {
			close(listener);
			continue;
		}

		break;
	}

	// if we got here, it means we didn't get bound
	if (p == NULL) {
		fprintf(stderr, "selectserver: failed to bind\n");
		exit(2);
	}

	freeaddrinfo(ai); // all done with this

	// listen on the listener socket
	// second arg is backlog size - if more connetions on queue, they may be refused ECONNREFUSED
	if (listen(listener, 10) == -1) {
		perror("listen");
		exit(3);
	}

	// add listener to the master set
	FD_SET(listener, &master);

	// keep track of the biggest file descriptor
	fdmax = listener;

	printf("listening for connections...\n");
	// main loop
	for(;;) {
		read_fds = master;	// copy master fd list into temp list read_fds
		// select checks file descriptors in set read_fds and determines which are ready for reading, writing, or have raised an exception
		// in this case, we only care about which are ready for reading
		// select modifies read_fds, only keeping fds that are ready for reading in the set
		if (select(fdmax+1, &read_fds, NULL, NULL, NULL) == -1) {
			perror("select");
			exit(4);
		}
		// run through the existing connections looking for data to read
		for(i = 0; i <= fdmax; i++) {
			// check if i is in read-ready set (read_fds)
			if (FD_ISSET(i, &read_fds)) { // found one
				if (i == listener) {
					// this means we've got a new connection
					addrlen = sizeof remoteaddr;
					// accept connection to listener fd as new file descriptor
					newfd = accept(listener, 
							(struct sockaddr *)&remoteaddr, 
							&addrlen);

					if (newfd == -1) {
						perror("accept");
					} else {
						FD_SET(newfd, &master);  // add to master set
						if (newfd > fdmax) {
							fdmax = newfd;  // keep track of the max
						}
						// inet_ntop converts network address to character string
						printf("server: new connection from %s on "
								"socket %d\n",
								inet_ntop(remoteaddr.ss_family,
									get_in_addr((struct sockaddr*)&remoteaddr),
									remoteIP, INET6_ADDRSTRLEN),
								newfd);
					}
				} else {
					// handle data from a client
					if ((nbytes = recv(i, buf, sizeof buf, 0)) <= 0) {
						// got error or connection closed by client
						if (nbytes == 0) {
							// connection closed
							printf("server: socket %d hung up\n", i);
						} else {
							perror("recv");
						}
						close(i);
						FD_CLR(i, &master);  // remove from master set
					} else {
						// we got some data from client
						for (j = 0; j <= fdmax; j++) {
							// send to everyone (i.e., every fd in the master set)!
							if (FD_ISSET(j, &master)) {
								// except the listener and the sender
								if (j != listener && j != i) {
									if (send(j, buf, nbytes, 0) == -1) {
										perror("send");
									}
								}
							}
						}
					}
				} // END handle data from client
			} // END get new incoming connection
		} // END looping through file descriptors
	} // END for(;;)
	return 0;
}
