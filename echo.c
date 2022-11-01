#include <arpa/inet.h>
#include <assert.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/errno.h>
#include <sys/socket.h>
#include <unistd.h>

void *
memzero(void *b, size_t len)
{
    return memset(b, 0, len);
}

char *progname;

void
panic(const char *s)
{
    fprintf(stderr, "%s: %s\n", progname, s);
    exit(1);
}

void
xerror(void)
{
    panic(strerror(errno));
}

void
print_sockaddr(struct sockaddr *sa)
{
    assert(INET_ADDRSTRLEN < INET6_ADDRSTRLEN);
    char addr[INET6_ADDRSTRLEN];

    if (sa->sa_family == AF_INET) {
        struct sockaddr_in *sa4 = (struct sockaddr_in *)sa;
        inet_ntop(AF_INET, &(sa4->sin_addr), addr, INET_ADDRSTRLEN);
    } else if (sa->sa_family == AF_INET6) {
        struct sockaddr_in6 *sa6 = (struct sockaddr_in6 *)sa;
        inet_ntop(AF_INET6, &(sa6->sin6_addr), addr, INET6_ADDRSTRLEN);
    } else {
        panic("unknown sa_family");
    }

    printf("%s", addr);
}

void
log_connection(struct sockaddr *sa)
{
    printf("Connection from ");
    print_sockaddr(sa);
    printf("\n");
}

// Unlike send(2), which returns -1 on error, sendall returns the number
// of total bytes sent, and ret != length indicates an error;
ssize_t
sendall(int socket, void *buffer, size_t length, int flags)
{
    size_t nremain = length;
    char *p = buffer;

    do {
        size_t nsent = send(socket, p, nremain, flags);
        if (nsent == -1) {
            return length - nremain;
        }

        nremain -= nsent;
        p += nsent;
    } while (nremain > 0);
    
    return length;
}

void
handle_connection(int socket)
{
    char buf[1024];
    ssize_t nrecvd;
    do {
        nrecvd = recv(socket, buf, sizeof(buf), 0);
        if (nrecvd == -1) {
            xerror();
        }

        ssize_t nsent = sendall(socket, buf, nrecvd, 0);
        if (nsent != nrecvd) {
            xerror();
        }
    } while (nrecvd != 0);

    close(socket);
}

int
main(int argc, char *argv[])
{
    struct addrinfo hints, *res;
    int error;
    progname = argv[0];

    memzero(&hints, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    error = getaddrinfo(NULL, "echo", &hints, &res);
    if (error != 0) {
        panic(gai_strerror(error));
    }

    int listener = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (listener == -1) {
        xerror();
    }

    int opt = 1;
    error = setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    if (error == -1) {
        xerror();
    }
    
    error = bind(listener, res->ai_addr, res->ai_addrlen);
    if (error == -1) {
        xerror();
    }

    freeaddrinfo(res);

    error = listen(listener, 5);
    if (error == -1) {
        xerror();
    }

    while (1) {
        struct sockaddr_storage addr;
        socklen_t addr_size = sizeof(addr);
        int s = accept(listener, (struct sockaddr *)&addr, &addr_size);
        if (s == -1) {
            xerror();
        }

        log_connection((struct sockaddr *)&addr);
        handle_connection(s);
    }

    return 0;
}
