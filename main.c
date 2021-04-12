/*
NanoHat OLED example
http://wiki.friendlyarm.com/wiki/index.php/NanoHat_OLED
*/

/*
The MIT License (MIT)
Copyright (C) 2017 FriendlyELEC

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/

#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/epoll.h>
#include <sys/types.h>
#include <signal.h>
#include <time.h>  
#include <pthread.h>
#include <dirent.h>
#include <stdarg.h>
#include "daemonize.h"

//{{ daemon
static int _debug = 1;
#define LOG_FILE_NAME "/tmp/code-server.log"
static void _log2file(const char* fmt, va_list vl)
{
        FILE* file_out;
        file_out = fopen(LOG_FILE_NAME,"a+");
        if (file_out == NULL) {
                return;
        }
        vfprintf(file_out, fmt, vl);
        fclose(file_out);
}
void log2file(const char *fmt, ...)
{
        if (_debug) {
                va_list vl;
                va_start(vl, fmt);
                _log2file(fmt, vl);
                va_end(vl);
        }
}
//}}

static int get_work_path(char* buff, int maxlen) {
    ssize_t len = readlink("/proc/self/exe", buff, maxlen);
    if (len == -1 || len == maxlen) {                         
        return -1;                                            
    }                                
    buff[len] = '\0';
                        
    char *pos = strrchr(buff, '/');
    if (pos != 0) {                   
       *pos = '\0';                   
    }              
                   
    return 0;
}            
static char workpath[255];
static int py_pids[128];
static int pid_count = 0;
extern int find_pid_by_name( char* ProcName, int* foundpid);
void send_signal_to_python_process(int signal) {
    int i, rv;
    if (pid_count == 0) {
        rv = find_pid_by_name( "code-server", py_pids);
        for(i=0; py_pids[i] != 0; i++) {
            log2file("found code-server pid: %d\n", py_pids[i]);
            pid_count++;
        }
    }
    if (pid_count > 0) {
        for(i=0; i<pid_count; i++) {
            if (kill(py_pids[i], signal) != 0) { //maybe pid is invalid
                pid_count = 0;
                break;
            }
        }
    }
}

pthread_t view_thread_id = 0;
void* threadfunc(char* arg) {
    pthread_detach(pthread_self());
    if (arg) {
        char* cmd = arg;
        system(cmd);
        free(arg);
    }
}

int load_python_view() {
    int ret;
    char* cmd = (char*)malloc(255);
    sprintf(cmd, "code-server 2>&1 | tee /tmp/code-server.log");
    ret = pthread_create(&view_thread_id, NULL, (void*)threadfunc,cmd);
    if(ret) {
        log2file("create pthread error \n");
        return 1;
    }
    return 0;
}

int find_pid_by_name( char* ProcName, int* foundpid) {
    DIR             *dir;
    struct dirent   *d;
    int             pid, i;
    char            *s;
    int pnlen;

    i = 0;
    foundpid[0] = 0;
    pnlen = strlen(ProcName);

    /* Open the /proc directory. */
    dir = opendir("/proc");
    if (!dir)
    {
        log2file("cannot open /proc");
        return -1;
    }

    /* Walk through the directory. */
    while ((d = readdir(dir)) != NULL) {

        char exe [PATH_MAX+1];
        char path[PATH_MAX+1];
        int len;
        int namelen;

        /* See if this is a process */
        if ((pid = atoi(d->d_name)) == 0)       continue;

        snprintf(exe, sizeof(exe), "/proc/%s/exe", d->d_name);
        if ((len = readlink(exe, path, PATH_MAX)) < 0)
            continue;
        path[len] = '\0';

        /* Find ProcName */
        s = strrchr(path, '/');
        if(s == NULL) continue;
        s++;

        /* we don't need small name len */
        namelen = strlen(s);
        if(namelen < pnlen)     continue;

        if(!strncmp(ProcName, s, pnlen)) {
            /* to avoid subname like search proc tao but proc taolinke matched */
            if(s[pnlen] == ' ' || s[pnlen] == '\0') {
                foundpid[i] = pid;
                i++;
            }
        }
    }
    foundpid[i] = 0;
    closedir(dir);
    return  0;
}

int main(int argc, char** argv) {
    unsigned int value = 0;
    int i, n;
    char ch;
    printf("Hwllo");
    if (isAlreadyRunning() == 1) {
        exit(3);
    }
    daemonize( "code-server" );
    int ret = get_work_path(workpath, sizeof(workpath));
    if (ret != 0) {
        log2file("get_work_path ret error\n");
        return 1;
    }
    sleep(3);

    load_python_view();
    while (1) {
        send_signal_to_python_process(SIGUSR1);
    }
    return 0;
}