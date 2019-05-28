#include <sys/param.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>

#include <ctype.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <unistd.h>

#define MAXFD 64

int *argc_;
char ***argv_;
char ***envp_;

void send_recv_loop (int acc);
void sig_hangup_handler (int sig)
{
    int i;
    (void) fprintf(stderr, "sig_hangup_handler(%d)\n", sig);
    for (i = 3; i < MAXFD; i++) {
        (void) close(i);
    }

    if (execve((*argv_)[0], (*argv_), (*envp_)) == -1) {
        perror ("execve");
    }
}


int server_socket (const char *portnm)
{
    char nbuf[NI_MAXHOST], sbuf[NI_MAXSERV];
    struct addrinfo hints, *res0;
    int soc, opt, errcode;
    socklen_t opt_len;

    (void) memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    
    if((errcode = getaddrinfo(NULL, portnm, &hints, &res0)) != 0){
        (void) fprintf(stderr, "getaddrinfo():%s\n", gai_strerror(errcode));
        return (-1);
    }

    if((errcode = getnameinfo(res0->ai_addr, res0->ai_addrlen,
                              nbuf, sizeof(nbuf),
                              sbuf, sizeof(sbuf),
                              NI_NUMERICHOST | NI_NUMERICSERV)) != 0) {
        (void) fprintf(stderr, "getnameinfo() : %s\n", gai_strerror(errcode));
        freeaddrinfo(res0);
        return (-1);
    }
    (void) fprintf(stderr, "port=%s\n", sbuf); 
    
    //create socket
    if ((soc = socket(res0->ai_family, res0->ai_socktype, res0->ai_protocol)) == -1)
    {
        perror ("socket");
        freeaddrinfo(res0);
        return (-1);
    }
    //setting socket option 
    opt = 1;
    opt_len = sizeof(opt);
    if (setsockopt(soc, SOL_SOCKET, SO_REUSEADDR, &opt, opt_len) == -1) {
        perror ("setsockopt");
        (void) close(soc);
        freeaddrinfo(res0);
        return (-1);
    }

    //bind
    if (bind(soc, res0->ai_addr, res0->ai_addrlen) == -1) {
        perror ("bind");
        (void) close(soc);
        freeaddrinfo(res0);
        return (-1);    
    }

    //listen
    if (listen (soc, SOMAXCONN) == -1) {
        perror ("listen"); 
        (void) close(soc);
        freeaddrinfo(res0);
        return (-1);  
    }
    freeaddrinfo(res0);
    return (soc);

}

void accept_loop (int soc)
{
    char hbuf[NI_MAXHOST], sbuf[NI_MAXSERV];
    struct sockaddr_storage from;

    int acc;
    socklen_t len;
    for (;;)
    {
        len = (socklen_t) sizeof(from);
        //accept
        if((acc = accept (soc, (struct sockaddr*) &from, &len)) == -1) {
            if (errno != EINTR) {
                perror ("accept");
            }
        } else {
            (void) getnameinfo ((struct sockaddr*) &from, len,
                                hbuf, sizeof(hbuf),
                                sbuf, sizeof(sbuf),
                                NI_NUMERICHOST | NI_NUMERICSERV);
            (void) fprintf (stderr, "accept : %s:%s\n", hbuf, sbuf); 
            
            //Loop of Send/Recv
            send_recv_loop(acc);
            (void) close (acc);
            acc = 0;
        }
    }
}

size_t mystrlcat (char *dst, const char *src, size_t size)
{
    const char *ps;
    char *pd, *pde;
    size_t dlen, lest;

    for (pd = dst, lest = size; *pd != '\0' && lest !=0; pd++, lest--);
    dlen = pd - dst;
    if (size - dlen == 0)
        return (dlen + strlen(src));
    
    pde = dst + size - 1;
    for (ps = src; *ps != '\0' && pd < pde; pd++, ps++) 
        *pd = *ps;

    for (; pd <= pde; pd++)
        *pd = '\0';
    
    while (*ps++)

    return (dlen + (ps - src - 1));        
}

void send_recv_loop (int acc)
{
    char buf[10], *ptr;
    ssize_t len;
    int cnt = 0;
    for (;;) {
        //recv
        if ((len = recv(acc, buf, sizeof(buf), 0)) == -1) {
            perror("recv");
            break;
        } else {
            cnt += 1;
            fprintf(stderr, "%d -> %s from recv\n", cnt, buf); 
        }
        if(len == 0) {
            //end of file
            (void) fprintf(stderr, "recv:EOF\n");
            break;
        }
        buf[len] = '\0';
        if((ptr = strpbrk(buf, "\r\n")) != NULL) {
            *ptr = '\0';
        }
        (void) fprintf(stderr, "[client]%s\n", buf);
        (void) mystrlcat(buf, ":OK\r\n", sizeof(buf));
        len = (ssize_t) strlen(buf);
        //fprintf (stderr, ">> DEBUG : len is %d\n", len);
        //response
        if((len = send (acc, buf, (ssize_t) len, 0)) == -1) {
            perror ("send");
            break;
        } else {
            fprintf(stderr, ">>>>%d : %d(bytes) send %s\n", cnt, len, buf);
        }
    }
}

int main(int argc, char *argv[], char *envp[])
{
    struct sigaction sa;
    int soc;

    if (argc <= 1) {
        fprintf(stderr, "re-exec port"); return (EX_USAGE);
    }

    argc_ = &argc;
    argv_ = &argv;
    envp_ = &envp;

    (void) sigaction (SIGHUP, (struct sigaction *)NULL, &sa);
    sa.sa_handler = sig_hangup_handler;
    sa.sa_flags = SA_NODEFER;
    (void) sigaction (SIGHUP, &sa, (struct sigaction *)NULL);
    (void) fprintf(stderr, "sigaction():end\n");

    if ((soc = server_socket (argv[1])) == -1) {
        (void) fprintf(stderr, "server_socket(%s):error\n", argv[1]);
        return (EX_UNAVAILABLE);
    }
    (void) fprintf(stderr, "ready for accept");
    accept_loop(soc);
    (void) close(soc);
    return (EX_OK);
}
