/* FluidSynth - A Software Synthesizer
 *
 * Copyright (C) 2003  Peter Hanappe and others.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2.1 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see
 * <https://www.gnu.org/licenses/>.
 */


/*
 * @file fluid_sys.h
 *
 * This header contains a bunch of (mostly) system and machine
 * dependent functions:
 *
 * - timers
 * - current time in milliseconds and microseconds
 * - debug logging
 * - profiling
 * - memory locking
 * - checking for floating point exceptions
 *
 * fluidsynth's wrapper OSAL so to say; include it in .c files, be careful to include
 * it in fluidsynth's private header files (see comment in fluid_coreaudio.c)
 */

#ifndef _FLUID_SYS_H
#define _FLUID_SYS_H

#include "fluidsynth_priv.h"

#if HAVE_MATH_H
#include <math.h>
#endif

#if HAVE_ERRNO_H
#include <errno.h>
#endif

#if HAVE_STDARG_H
#include <stdarg.h>
#endif

#if HAVE_UNISTD_H
#include <unistd.h>
#endif

#if HAVE_FCNTL_H
#include <fcntl.h>
#endif

#if HAVE_SYS_MMAN_H
#include <sys/mman.h>
#endif

#if HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif

#if HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif

#if HAVE_SYS_TIME_H
#include <sys/time.h>
#endif

#if HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif

#if HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif

#if HAVE_NETINET_TCP_H
#include <netinet/tcp.h>
#endif

#if HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif

#if HAVE_LIMITS_H
#include <limits.h>
#endif

#if HAVE_OPENMP
#include <omp.h>
#endif

#if HAVE_IO_H
#include <io.h> // _open(), _close(), read(), write() on windows
#endif

#if HAVE_SIGNAL_H
#include <signal.h>
#endif

/** Integer types  */
#if HAVE_STDINT_H
#include <stdint.h>
#endif


#if OSAL_glib
#include "fluid_sys_glib.h"
#elif OSAL_embedded
#include "fluid_sys_embedded.h"
#elif OSAL_cpp11
#include "fluid_sys_cpp11.h"
#else
#error "no OS abstraction configured"
#endif


/*
 * CYGWIN has its own version of <windows.h>, which can be
 * safely included together with POSIX includes.
 * Thanks to this, CYGWIN can also run audio output and MIDI
 * input drivers from traditional interfaces of Windows.
 */
#if defined(__CYGWIN__) && HAVE_WINDOWS_H
#include <windows.h>
#include <wchar.h>
#endif

#if defined(_WIN32) && HAVE_WINDOWS_H
#include <winsock2.h>
#include <ws2tcpip.h>	/* Provides also socklen_t */

/* WIN32 special defines */
#define STDIN_FILENO 0
#define STDOUT_FILENO 1
#define STDERR_FILENO 2

#ifdef _MSC_VER
#pragma warning(disable : 4244)
#pragma warning(disable : 4101)
#pragma warning(disable : 4305)
#pragma warning(disable : 4996)
#endif

/*
 * Required by Windows-specific sf_wchar_open() from old libsndfile
 * versions before v1.1.0, that takes a UTF16_BE encoded filename.
 * Note that FluidSynth needs libsndfile >= v1.2.1 anyway.
 */
#define ENABLE_SNDFILE_WINDOWS_PROTOTYPES 1

#endif

/* Darwin special defines (taken from config_macosx.h) */
#ifdef DARWIN
# define MACINTOSH
# define __Types__
#endif

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Macro used for safely accessing a message from a GError and using a default
 * message if it is NULL.
 * @param err Pointer to a GError to access the message field of.
 * @return Message string
 */
#define fluid_gerror_message(err)  ((err) ? err->message : "No error details")

#if defined(_WIN32) || defined(__CYGWIN__)
char* fluid_get_windows_error(void);
#endif

#define FLUID_VERSION_CHECK(major, minor, patch) ((major<<16)|(minor<<8)|(patch))

/* Integer<->pointer conversion */
#define FLUID_POINTER_TO_UINT(x)  ((unsigned int)(uintptr_t)(x))
#define FLUID_UINT_TO_POINTER(x)  ((void *)(uintptr_t)(x))
#define FLUID_POINTER_TO_INT(x)   ((signed int)(intptr_t)(x))
#define FLUID_INT_TO_POINTER(x)   ((void *)(intptr_t)(x))

/*
 * Utility functions
 */
#if FLUID_IS_BIG_ENDIAN
#define FLUID_FOURCC(_a, _b, _c, _d) \
    (uint32_t)(((uint32_t)(_a) << 24) | ((uint32_t)(_b) << 16) | ((uint32_t)(_c) << 8) | (uint32_t)(_d))
#else
#define FLUID_FOURCC(_a, _b, _c, _d) \
    (uint32_t)(((uint32_t)(_d) << 24) | ((uint32_t)(_c) << 16) | ((uint32_t)(_b) << 8) | (uint32_t)(_a)) 
#endif

char *fluid_strtok(char **str, char *delim);

#if defined(__OS2__)
#define INCL_DOS
#include <os2.h>

/* Define socklen_t if not provided */
#if !HAVE_SOCKLEN_T
typedef int socklen_t;
#endif
#endif

/**
    Time functions

 */

unsigned int fluid_curtime(void);
double fluid_utime(void);


/**
    Timers

 */

/* if the callback function returns 1 the timer will continue; if it
   returns 0 it will stop */
typedef int (*fluid_timer_callback_t)(void *data, unsigned int msec);

typedef struct _fluid_timer_t fluid_timer_t;

fluid_timer_t *new_fluid_timer(int msec, fluid_timer_callback_t callback,
                               void *data, int new_thread, int auto_destroy,
                               int high_priority);

void delete_fluid_timer(fluid_timer_t *timer);
int fluid_timer_join(fluid_timer_t *timer);
int fluid_timer_stop(fluid_timer_t *timer);
int fluid_timer_is_running(const fluid_timer_t *timer);
long fluid_timer_get_interval(const fluid_timer_t * timer);


/* Atomic operations */

static FLUID_INLINE void
fluid_atomic_float_set(fluid_atomic_float_t *fptr, float val)
{
    int32_t ival;
    memcpy(&ival, &val, 4);
    fluid_atomic_int_set((fluid_atomic_int_t *)fptr, ival);
}

static FLUID_INLINE float
fluid_atomic_float_get(fluid_atomic_float_t *fptr)
{
    int32_t ival;
    float fval;
    ival = fluid_atomic_int_get((fluid_atomic_int_t *)fptr);
    memcpy(&fval, &ival, 4);
    return fval;
}


/* Threads */

typedef struct
{
    fluid_thread_func_t func;
    void *data;
    int prio_level;
} fluid_thread_info_t;

fluid_thread_t *new_fluid_thread(const char *name, fluid_thread_func_t func, void *data,
                                 int prio_level, int detach);
void delete_fluid_thread(fluid_thread_t *thread);
void fluid_thread_self_set_prio(int prio_level);

int fluid_thread_join(fluid_thread_t *thread);


/* Sockets and I/O */

int fluid_istream_readline(fluid_istream_t in, fluid_ostream_t out, char *prompt, char *buf, int len);
int fluid_ostream_printf(fluid_ostream_t out, const char *format, ...);

#if defined(_WIN32)
typedef SOCKET fluid_socket_t;
#else
typedef int fluid_socket_t;
#endif

/* The function should return 0 if no error occurred, non-zero
   otherwise. If the function return non-zero, the socket will be
   closed by the server. */
typedef int (*fluid_server_func_t)(void *data, fluid_socket_t client_socket, char *addr);

fluid_server_socket_t *new_fluid_server_socket(int port, fluid_server_func_t func, void *data);
void delete_fluid_server_socket(fluid_server_socket_t *sock);
int fluid_server_socket_join(fluid_server_socket_t *sock);
void fluid_socket_close(fluid_socket_t sock);
fluid_istream_t fluid_socket_get_istream(fluid_socket_t sock);
fluid_ostream_t fluid_socket_get_ostream(fluid_socket_t sock);

/* File access */
FILE* fluid_file_open(const char* filename, const char** errMsg);
fluid_long_long_t fluid_file_tell(FILE* f);


/* Profiling */
#if WITH_PROFILING
/** profiling interface between Profiling command shell and Audio
    rendering  API (FluidProfile_0004.pdf- 3.2.2)
*/

/*
  -----------------------------------------------------------------------------
  Shell task side |    Profiling interface               |  Audio task side
  -----------------------------------------------------------------------------
  profiling       |    Internal     |      |             |      Audio
  command   <---> |<-- profiling -->| Data |<--macros -->| <--> rendering
  shell           |    API          |      |             |      API

*/

/* default parameters for shell command "prof_start" in fluid_sys.c */
#define FLUID_PROFILE_DEFAULT_BANK 0       /* default bank */
#define FLUID_PROFILE_DEFAULT_PROG 16      /* default prog (organ) */
#define FLUID_PROFILE_FIRST_KEY 12         /* first key generated */
#define FLUID_PROFILE_LAST_KEY 108         /* last key generated */
#define FLUID_PROFILE_DEFAULT_VEL 64       /* default note velocity */
#define FLUID_PROFILE_VOICE_ATTEN -0.04f   /* gain attenuation per voice (dB) */


#define FLUID_PROFILE_DEFAULT_PRINT 0      /* default print mode */
#define FLUID_PROFILE_DEFAULT_N_PROF 1     /* default number of measures */
#define FLUID_PROFILE_DEFAULT_DURATION 500 /* default duration (ms)  */


extern unsigned short fluid_profile_notes; /* number of generated notes */
extern unsigned char fluid_profile_bank;   /* bank,prog preset used by */
extern unsigned char fluid_profile_prog;   /* generated notes */
extern unsigned char fluid_profile_print;  /* print mode */

extern unsigned short fluid_profile_n_prof;/* number of measures */
extern unsigned short fluid_profile_dur;   /* measure duration in ms */
extern fluid_atomic_int_t fluid_profile_lock ; /* lock between multiple shell */
/**/

/*----------------------------------------------
  Internal profiling API (in fluid_sys.c)
-----------------------------------------------*/
/* Starts a profiling measure used in shell command "prof_start" */
void fluid_profile_start_stop(unsigned int end_ticks, short clear_data);

/* Returns status used in shell command "prof_start" */
int fluid_profile_get_status(void);

/* Prints profiling data used in shell command "prof_start" */
void fluid_profiling_print_data(double sample_rate, fluid_ostream_t out);

/* Returns True if profiling cancellation has been requested */
int fluid_profile_is_cancel_req(void);

/* For OS that implement <ENTER> key for profile cancellation:
 1) Adds #define FLUID_PROFILE_CANCEL
 2) Adds the necessary code inside fluid_profile_is_cancel() see fluid_sys.c
*/
#if defined(_WIN32)      /* Profile cancellation is supported for Windows */
#define FLUID_PROFILE_CANCEL

#elif defined(__OS2__)  /* OS/2 specific stuff */
/* Profile cancellation isn't yet supported for OS2 */

#else   /* POSIX stuff */
#define FLUID_PROFILE_CANCEL /* Profile cancellation is supported for linux */
#include <unistd.h> /* STDIN_FILENO */
#include <sys/select.h> /* select() */
#endif /* posix */

/* logging profiling data (used on synthesizer instance deletion) */
void fluid_profiling_print(void);

/*----------------------------------------------
  Profiling Data (in fluid_sys.c)
-----------------------------------------------*/
/** Profiling data. Keep track of min/avg/max values to profile a
    piece of code. */
typedef struct _fluid_profile_data_t
{
    const char *description;        /* name of the piece of code under profiling */
    double min, max, total;   /* duration (microsecond) */
    unsigned int count;       /* total count */
    unsigned int n_voices;    /* voices number */
    unsigned int n_samples;   /* audio samples number */
} fluid_profile_data_t;

enum
{
    /* commands/status  (profiling interface) */
    PROFILE_STOP,    /* command to stop a profiling measure */
    PROFILE_START,   /* command to start a profile measure */
    PROFILE_READY,   /* status to signal that a profiling measure has finished
	                    and ready to be printed */
    /*- State returned by fluid_profile_get_status() -*/
    /* between profiling commands and internal profiling API */
    PROFILE_RUNNING, /* a profiling measure is running */
    PROFILE_CANCELED,/* a profiling measure has been canceled */
};

/* Data interface */
extern unsigned char fluid_profile_status ;       /* command and status */
extern unsigned int fluid_profile_end_ticks;      /* ending position (in ticks) */
extern fluid_profile_data_t fluid_profile_data[]; /* Profiling data */

/*----------------------------------------------
  Probes macros
-----------------------------------------------*/
/** Macro to obtain a time reference used for the profiling */
#define fluid_profile_ref() fluid_utime()

/** Macro to create a variable and assign the current reference time for profiling.
 * So we don't get unused variable warnings when profiling is disabled. */
#define fluid_profile_ref_var(name)     double name = fluid_utime()

/**
 * Profile identifier numbers. List all the pieces of code you want to profile
 * here. Be sure to add an entry in the fluid_profile_data table in
 * fluid_sys.c
 */
enum
{
    FLUID_PROF_WRITE,
    FLUID_PROF_ONE_BLOCK,
    FLUID_PROF_ONE_BLOCK_CLEAR,
    FLUID_PROF_ONE_BLOCK_VOICE,
    FLUID_PROF_ONE_BLOCK_VOICES,
    FLUID_PROF_ONE_BLOCK_REVERB,
    FLUID_PROF_ONE_BLOCK_CHORUS,
    FLUID_PROF_VOICE_NOTE,
    FLUID_PROF_VOICE_RELEASE,
    FLUID_PROFILE_NBR	/* number of profile probes */
};
/** Those macros are used to calculate the min/avg/max. Needs a profile number, a
    time reference, the voices and samples number. */

/* local macro : acquiere data */
#define fluid_profile_data(_num, _ref, voices, samples)\
{\
	double _now = fluid_utime();\
	double _delta = _now - _ref;\
	fluid_profile_data[_num].min = _delta < fluid_profile_data[_num].min ?\
                                   _delta : fluid_profile_data[_num].min; \
	fluid_profile_data[_num].max = _delta > fluid_profile_data[_num].max ?\
                                   _delta : fluid_profile_data[_num].max;\
	fluid_profile_data[_num].total += _delta;\
	fluid_profile_data[_num].count++;\
	fluid_profile_data[_num].n_voices += voices;\
	fluid_profile_data[_num].n_samples += samples;\
	_ref = _now;\
}

/** Macro to collect data, called from inner functions inside audio
    rendering API */
#define fluid_profile(_num, _ref, voices, samples)\
{\
	if ( fluid_profile_status == PROFILE_START)\
	{	/* acquires data */\
		fluid_profile_data(_num, _ref, voices, samples)\
	}\
}

/** Macro to collect data, called from audio rendering API (fluid_write_xxxx()).
 This macro control profiling ending position (in ticks).
*/
#define fluid_profile_write(_num, _ref, voices, samples)\
{\
	if (fluid_profile_status == PROFILE_START)\
	{\
		/* acquires data first: must be done before checking that profile is
           finished to ensure at least one valid data sample.
		*/\
		fluid_profile_data(_num, _ref, voices, samples)\
		if (fluid_synth_get_ticks(synth) >= fluid_profile_end_ticks)\
		{\
			/* profiling is finished */\
			fluid_profile_status = PROFILE_READY;\
		}\
	}\
}

#else

/* No profiling */
#define fluid_profiling_print()
#define fluid_profile_ref()  0
#define fluid_profile_ref_var(name)
#define fluid_profile(_num,_ref,voices, samples)
#define fluid_profile_write(_num,_ref, voices, samples)
#endif /* WITH_PROFILING */

/**

    Memory locking

    Memory locking is used to avoid swapping of the large block of
    sample data.
 */

#if defined(HAVE_SYS_MMAN_H) && !defined(__OS2__)
#define fluid_mlock(_p,_n)      mlock(_p, _n)
#define fluid_munlock(_p,_n)    munlock(_p,_n)
#else
#define fluid_mlock(_p,_n)      0
#define fluid_munlock(_p,_n)
#endif


/**

    Floating point exceptions

    fluid_check_fpe() checks for "unnormalized numbers" and other
    exceptions of the floating point processor.
*/
#ifdef FPE_CHECK
#define fluid_check_fpe(expl) fluid_check_fpe_i386(expl)
#define fluid_clear_fpe() fluid_clear_fpe_i386()
unsigned int fluid_check_fpe_i386(char *explanation_in_case_of_fpe);
void fluid_clear_fpe_i386(void);
#else
#define fluid_check_fpe(expl)
#define fluid_clear_fpe()
#endif


/* System control */
void fluid_msleep(unsigned int msecs);

/**
 * Advances the given \c ptr to the next \c alignment byte boundary.
 * Make sure you've allocated an extra of \c alignment bytes to avoid a buffer overflow.
 *
 * @note \c alignment must be a power of two
 * @return Returned pointer is guaranteed to be aligned to \c alignment boundary and in range \f[ ptr <= returned_ptr < ptr + alignment \f].
 */
static FLUID_INLINE void *fluid_align_ptr(const void *ptr, unsigned int alignment)
{
    uintptr_t ptr_int = (uintptr_t)ptr;
    unsigned int offset = ptr_int & (alignment - 1);
    unsigned int add = (alignment - offset) & (alignment - 1); // advance the pointer to the next alignment boundary
    ptr_int += add;

    /* assert alignment is power of two */
    FLUID_ASSERT(!(alignment == 0) && !(alignment & (alignment - 1)));

    return (void *)ptr_int;
}

#define FLUID_DEFAULT_ALIGNMENT (64U)


/* Shell parsing */
int fluid_shell_parse_argv_internal(const char *command_line, int *argcp, char ***argvp);
void fluid_strfreev_internal(char **argvp);

#ifdef __cplusplus
}
#endif
#endif /* _FLUID_SYS_H */
