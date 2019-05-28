#include <sys/types.h>
#include <sys/stat.h>

#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sysexits.h>
#include <unistd.h>

#define MAXFD 64

int daemonize (int nochdir, int noclose)
{
    int i, fd;
    pid_t pid;
    if((pid = fork()) == -1) {
        return (-1);
    } else if (pid !=0 ) {
        //exit parent process
        _exit (0);
    }
    
    // first child process
    // session reader
    (void) setsid();
    // hup signal ignore
    (void) signal (SIGHUP, SIG_IGN);
    if((pid = fork()) != 0 ) {
        _exit (0);
    }
            
    // daemon process
    if (nochdir == 0) {
        //move to root directory
        (void) chdir("/");
    }
    if (noclose == 0) {
        //every fd close
        for (i = 0; i < MAXFD; i++){
            (void) close (i);
        }

        if ((fd = open("/dev/null", O_RDWR, 0)) != -1) {
            (void) dup2(fd, 0);
            (void) dup2(fd, 1);
            (void) dup2(fd, 2);
            if (fd > 2) {
                (void) close (fd);
            }
        }
        //while(1){
        
        //}
    }
    return (0);
}

#ifdef UNIT_TEST
#include <syslog.h>

int main(int argc, char *argv[])
{
    char buf[256];
    (void) daemonize(0, 0);
    (void) fprintf(stderr, "stderr\n");

    //print current directory
    syslog (LOG_USER|LOG_NOTICE, "daemon:cwd=%s\n", getcwd(buf, sizeof(buf)));
        //todo

    return (EX_OK);
}
#endif




