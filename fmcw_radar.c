/*

 fmcw_radar based on sensd, and tty_talk
 
 GPL copyright.
 Robert Olsson  <robert@herjulf.se>  also code taken from:

 file is part of the minicom communications package,
 *		Copyright 1991-1995 Miquel van Smoorenburg.
 *
 *		This program is free software; you can redistribute it and/or
 *		modify it under the terms of the GNU General Public License
 *		as published by the Free Software Foundation; either version
 *		2 of the License, or (at your option) any later version.

 and

 * Based on.... serial port tester
 * Doug Hughes - Auburn University College of Engineering
 * first non-baud argument is tty (e.g. /dev/term/a)
 * second argument is file name (e.g. /etc/hosts)
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <linux/types.h>
#include <sys/stat.h>
#include <stdint.h>
#include <unistd.h>
#include <termio.h>
#include <sys/fcntl.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <time.h>
#include <sys/poll.h>
#include "devtag-allinone.h"

#define NOB 134

#define VERSION "1.0 2020-02-03"
#define P_LOCK "/var/lock"

char lockfile[128]; /* UUCP lock file of terminal */
char dial_tty[128];
char username[16];
int pid;
int retry = 6;

int date = 1, utime =0, gmt=0;

int ext_trigger = 0;

void usage(void)
{
  printf("\nfmcw_radar version %s\n", VERSION);
  
  printf("\nfmcw_radar parses fmcw radar dev on serial port\n");
  printf("fmcw_radar [-BAUDRATE] [-d] [-thresh level] device command\n");
  printf(" Valid baudrates 4800, 9600, 19200, 38400, 57600 (Default), 115200 bps\n");
  printf(" -thresh level is noise filer 1-44\n");
  printf(" fmcw_radar can handle devtag\n");

  printf("\nExample 1: Simple\n  fmcw_radar  /dev/ttyUSB0\n");
  printf("\nExample 2: Debug\n  fmcw_radar -d /dev/ttyUSB0\n");
  printf("\nExample 3: Sensitive\n  fmcw_radar -thresh 4 /dev/ttyUSB0\n");
  
  exit(-1);
}

/*
 * Find out name to use for lockfile when locking tty.
 */

char *mbasename(char *s, char *res, int reslen)
{
  char *p;
  
  if (strncmp(s, "/dev/", 5) == 0) {
    /* In /dev */
    strncpy(res, s + 5, reslen - 1);
    res[reslen-1] = 0;
    for (p = res; *p; p++)
      if (*p == '/')
        *p = '_';
  } else {
    /* Outside of /dev. Do something sensible. */
    if ((p = strrchr(s, '/')) == NULL)
      p = s;
    else
      p++;
    strncpy(res, p, reslen - 1);
    res[reslen-1] = 0;
  }
  return res;
}

int lockfile_create(void)
{
  int fd, n;
  char buf[81];

  n = umask(022);
  /* Create lockfile compatible with UUCP-1.2  and minicom */
  if ((fd = open(lockfile, O_WRONLY | O_CREAT | O_EXCL, 0666)) < 0) {
    return 0;
  } else {
    snprintf(buf, sizeof(buf),  "%05d fmcw_radar %.20s\n", (int) getpid(), 
	     username);

    write(fd, buf, strlen(buf));
    close(fd);
  }
  umask(n);
  return 1;
}

void lockfile_remove(void)
{
  if (lockfile[0])
    unlink(lockfile);
}

int have_lock_dir(void)
{
 struct stat stt;
  char buf[128];

  if ( stat(P_LOCK, &stt) == 0) {

    snprintf(lockfile, sizeof(lockfile),
                       "%s/LCK..%s",
                       P_LOCK, mbasename(dial_tty, buf, sizeof(buf)));
  }
  else {
    printf("Lock directory %s does not exist\n", P_LOCK);
	exit(-1);
  }
  return 1;
}

int get_lock()
{
  char buf[128];
  int fd, n = 0;

  have_lock_dir();

  if((fd = open(lockfile, O_RDONLY)) >= 0) {
    n = read(fd, buf, 127);
    close(fd);
    if (n > 0) {
      pid = -1;
      if (n == 4)
        /* Kermit-style lockfile. */
        pid = *(int *)buf;
      else {
        /* Ascii lockfile. */
        buf[n] = 0;
        sscanf(buf, "%d", &pid);
      }
      if (pid > 0 && kill((pid_t)pid, 0) < 0 &&
          errno == ESRCH) {
        printf("Lockfile is stale. Overriding it..\n");
        sleep(1);
        unlink(lockfile);
      } else
        n = 0;
    }
    if (n == 0) {
      if(retry == 1) /* Last retry */
	printf("Device %s is locked.\n", dial_tty);
      return 0;
    }
  }
  lockfile_create();
  return 1;
}

#define BUFLEN 80
uint8_t buf[BUFLEN];

void print_date(char *datebuf)
{
  time_t raw_time;
  struct tm *tp;
  char buf[256];

  *datebuf = 0;
  time ( &raw_time );

  if(gmt)
    tp = gmtime ( &raw_time );
  else
    tp = localtime ( &raw_time );

  if(date) {
	  sprintf(buf, "%04d-%02d-%02d %2d:%02d:%02d ",
		  tp->tm_year+1900, tp->tm_mon+1, 
		  tp->tm_mday, tp->tm_hour, 
		  tp->tm_min, tp->tm_sec);
	  strcat(datebuf, buf);
  }
  if(utime) {
	  sprintf(buf, "UT=%ld ", raw_time);
	  strcat(datebuf, buf);
  }
}

void
my_wait(int timeout)
{
  int rc;
  rc = poll(NULL, 0, timeout);

  if (rc < 0)  {
    perror("  poll() failed");
  }
  if (rc == 0)  {
    /* TIMEOUT: Try (re)-connect to proxy */
  }
}

int main(int ac, char *av[]) 
{
	struct termios tp, old;
	int fd;
	char io[BUFSIZ];
	int res;
	long baud;
	unsigned char RTT[NOB];
	unsigned int YCTa = 0, YCTb = 0, YCT1=0;
	double dist;
	unsigned sweep_ok = 0, sweep_tot = 0;
	int i;
	int debug = 0;
	int thresh = 5;
	
       	if(ac == 1) 
	  usage();

	for(i = 1; (i < ac) && (av[i][0] == '-'); i++)  {
	    if (strcmp(av[i], "-300") == 0) 
	      baud = B300;

	    else if (strcmp(av[i], "-600") == 0) 
	      baud = B600;

	    else if (strcmp(av[i], "-1200") == 0) 
	      baud = B1200;

	    else if (strcmp(av[i], "-2400") == 0) 
	      baud = B2400;

	    else if (strcmp(av[i], "-4800") == 0) 
	      baud = B4800;

	    else if (strcmp(av[i], "-9600") == 0)
	      baud = B9600;

	    else if (strcmp(av[i], "-19200") == 0)
	      baud = B19200;

	    else if (strcmp(av[i], "-38400") == 0)
	      baud = B38400;

	    else if (strcmp(av[i], "-57600") == 0)
	      baud = B57600;

	    else if (strcmp(av[i], "-115200") == 0)
	      baud = B115200;

	    else if (strcmp(av[i], "-utime") == 0) 
	      utime = 1;

	    else if (strncmp(av[i], "-d", 2) == 0) {
	      debug = 1;
	    }
	    else if (strncmp(av[i], "-thresh", 2) == 0) 
	      thresh = atoi(av[++i]);
	}

	if(debug) {
	  printf("thesh = %d\n", thresh);
	}

	baud = B57600;
		
	strncpy(dial_tty, devtag_get(av[i]), sizeof(dial_tty));

	while (! get_lock()) {
	    if(--retry == 0)
	      exit(-1);
	    sleep(1);
	}

	if ((fd = open(devtag_get(av[i]), O_RDWR | O_NOCTTY | O_NONBLOCK)) < 0) {
	  perror("bad terminal device, try another");
	  exit(-1);
	}
	
	fcntl(fd, F_GETFL);
	fcntl(fd, F_SETFL, O_RDWR);

	if (tcgetattr(fd, &old) < 0) {
		perror("Couldn't get term attributes");
		exit(-1);
	}

  /* input modes - clear indicated ones giving: no break, no CR to NL, 
     no parity check, no strip char, no start/stop output (sic) control */
   tp.c_iflag &= ~(IGNBRK | BRKINT | ICRNL | INPCK | ISTRIP | IXON);
  /* output modes - clear giving: no post processing such as NL to CR+NL */
  tp.c_oflag &= ~(OPOST);
  /* control modes - set 8 bit chars */
  tp.c_cflag |= (CS8);
  /* local modes - clear giving: echoing off, canonical off (no erase with 
     backspace, ^U,...),  no extended functions, no signal chars (^Z,^C) */
  tp.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
  /* control chars - set return condition: min number of bytes and timer */
  tp.c_cc[VMIN] = 5; tp.c_cc[VTIME] = 8; /* after 5 bytes or .8 seconds
					      after first byte seen      */
  tp.c_cc[VMIN] = 0; tp.c_cc[VTIME] = 0; /* immediate - anything       */
  tp.c_cc[VMIN] = 2; tp.c_cc[VTIME] = 0; /* after two bytes, no timer  */
  tp.c_cc[VMIN] = 0; tp.c_cc[VTIME] = 8; /* after a byte or .8 seconds */
  
  cfsetospeed(&tp, baud);
  cfsetispeed(&tp, baud);

  /* put terminal in raw mode after flushing */
  if (tcsetattr(fd, TCSAFLUSH, &tp) < 0) perror("can't set raw mode");

	while(1) {
sync:	  
	  for(i = 0; i < 3; i++) {
	    res = read(fd, &RTT[i], 1);

	    if(res != 1)  {
	      my_wait(50);
	      goto sync;
	    }
	  
	    if(RTT[i] != 0xff)
	      goto sync;
	  }
	  
	  res = read(fd, &RTT[3], NOB-3);
	  sweep_tot++;

	  if(debug) {
	    for (int j = 0; j < NOB; j++){
	      printf(" %02X", RTT[j]);	    
	    }
	    printf("\n");
	  }

	  if(res != (NOB-3)) {

	    if(debug) {
	      printf("Wrong len=%d\n", res);
	    }
	    my_wait(100);
	    goto sync;
	  }

	  if( RTT[NOB-1] || RTT[NOB-2] || RTT[NOB-3] ) {
	    if(debug) {
	      printf("NOT NULL %02X \n", RTT[NOB-1]);
	    }
	    my_wait(100);
	    goto sync;
	  }

	  /* Calc obstacle distance of maximum reflection intensity */
	  YCTa = RTT[3];      
	  YCTb = RTT[4];
	  YCT1 = (YCTa << 8) + YCTb;
	  printf("D:  %-5u ", YCT1);

	  sweep_ok++;

	  for(int i = 6; i < NOB-3; i++) {
	    if(RTT[i] > thresh) {
	      dist = (i-6) * 12.6; /* Calculate distance */
	      /* Output the obstacle distance */
	      printf("%-3.0f_%-u ", dist, RTT[i]);
	    }
	  }
	  printf("\n");
	  fflush(stdout);

	  if(debug) {
	    printf("Sweep: ok=%-u, tot=%-u ratio=%-4.2f\n", sweep_ok, sweep_tot,
		   (float) sweep_ok/(float) sweep_tot);
	  }
	} /* while(1) */
	
	if (tcsetattr(fd, TCSANOW, &old) < 0) {
	  perror("Couldn't restore term attributes");
	  exit(-1);
	}

	lockfile_remove();
	exit(0);

	if (tcsetattr(fd, TCSANOW, &old) < 0) {
	  perror("Couldn't restore term attributes");
	}
	exit(-1);
}
