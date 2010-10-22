/* Implementation of the POSIX sleep function using nanosleep.
   Copyright (C) 1996, 1997, 1998, 1999 Free Software Foundation, Inc.
   This file is part of the GNU C Library.
   Contributed by Ulrich Drepper <drepper@cygnus.com>, 1996.

   The GNU C Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   The GNU C Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with the GNU C Library; see the file COPYING.LIB.  If not,
   write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.  */

#include <errno.h>
#include <time.h>
#include <unistd.h>
/* Want fast and small __sigemptyset/__sigaddset/__sigismember: */
/* TODO: make them available (to everybody) without this hack */
#ifndef __USE_EXTERN_INLINES
# define __USE_EXTERN_INLINES 1
#endif
#include <signal.h>



/* version perusing nanosleep */
#if defined __UCLIBC_HAS_REALTIME__

#if 0
/* This is a quick and dirty, but not 100% compliant with
 * the stupid SysV SIGCHLD vs. SIG_IGN behaviour.  It is
 * fine unless you are messing with SIGCHLD...  */
unsigned int sleep (unsigned int sec)
{
	unsigned int res;
	struct timespec ts = { .tv_sec = (long int) seconds, .tv_nsec = 0 };
	res = nanosleep(&ts, &ts);
	if (res) res = (unsigned int) ts.tv_sec + (ts.tv_nsec >= 500000000L);
	return res;
}

#else

/* We are going to use the `nanosleep' syscall of the kernel.  But the
   kernel does not implement the sstupid SysV SIGCHLD vs. SIG_IGN
   behaviour for this syscall.  Therefore we have to emulate it here.  */
unsigned int sleep (unsigned int seconds)
{
    struct timespec ts = { .tv_sec = (long int) seconds, .tv_nsec = 0 };
    sigset_t set;
    struct sigaction oact;
    unsigned int result;

    /* This is not necessary but some buggy programs depend on this.  */
    if (seconds == 0) {
# ifdef CANCELLATION_P
	CANCELLATION_P (THREAD_SELF);
# endif
	return 0;
    }

    /* Linux will wake up the system call, nanosleep, when SIGCHLD
       arrives even if SIGCHLD is ignored.  We have to deal with it
       in libc.  */

    __sigemptyset (&set);
    __sigaddset (&set, SIGCHLD);

    /* Is SIGCHLD set to SIG_IGN? */
    sigaction (SIGCHLD, NULL, &oact); /* never fails */
    if (oact.sa_handler == SIG_IGN) {
	/* Yes.  Block SIGCHLD, save old mask.  */
	sigprocmask (SIG_BLOCK, &set, &set); /* never fails */
    }

    /* Run nanosleep, with SIGCHLD blocked if SIGCHLD is SIG_IGNed.  */
    result = nanosleep (&ts, &ts);

    if (!__sigismember (&set, SIGCHLD)) {
	/* We did block SIGCHLD, and old mask had no SIGCHLD bit.
	   IOW: we need to unblock SIGCHLD now. Do it.  */
	/* this sigprocmask call never fails, thus never updates errno,
	   and therefore we don't need to save/restore it.  */
	sigprocmask (SIG_SETMASK, &set, NULL); /* never fails */
    }

    if (result != 0)
	/* Round remaining time.  */
	result = (unsigned int) ts.tv_sec + (ts.tv_nsec >= 500000000L);

    return result;
}
#endif
#else /* __UCLIBC_HAS_REALTIME__ */
/* no nanosleep, use signals and alarm() */
static void sleep_alarm_handler(int attribute_unused sig)
{
}
unsigned int sleep (unsigned int seconds)
{
    struct sigaction act, oact;
    sigset_t set, oset;
    unsigned int result, remaining;
    time_t before, after;
    int old_errno = errno;

    /* This is not necessary but some buggy programs depend on this.  */
    if (seconds == 0)
	return 0;

    /* block SIGALRM */
    __sigemptyset (&set);
    __sigaddset (&set, SIGALRM);
    sigprocmask (SIG_BLOCK, &set, &oset); /* can't fail */

    act.sa_handler = sleep_alarm_handler;
    act.sa_flags = 0;
    act.sa_mask = oset;
    sigaction(SIGALRM, &act, &oact); /* never fails */

    before = time(NULL);
    remaining = alarm(seconds);
    if (remaining && remaining > seconds) {
	/* restore user's alarm */
	sigaction(SIGALRM, &oact, NULL);
	alarm(remaining); /* restore old alarm */
	sigsuspend(&oset);
	after = time(NULL);
    } else {
	sigsuspend (&oset);
	after = time(NULL);
	sigaction (SIGALRM, &oact, NULL);
    }
    result = after - before;
    alarm(remaining > result ? remaining - result : 0);
    sigprocmask (SIG_SETMASK, &oset, NULL);

    __set_errno(old_errno);

    return result > seconds ? 0 : seconds - result;
}
#endif /* __UCLIBC_HAS_REALTIME__ */
libc_hidden_def(sleep)
