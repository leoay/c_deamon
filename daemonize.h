#ifndef __DAEMONIZE__H__
#define __DAEMONIZE__H__

#define LOCKFILE "/var/run/nanohat-oled.pid"
extern int isAlreadyRunning();
extern void daemonize(const char *cmd);

#endif

