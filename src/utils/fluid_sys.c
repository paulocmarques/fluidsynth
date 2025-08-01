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

#include "fluid_sys.h"


#if READLINE_SUPPORT
#include <readline/readline.h>
#include <readline/history.h>
#endif

#ifdef DBUS_SUPPORT
#include "fluid_rtkit.h"
#endif

#if HAVE_PTHREAD_H && !defined(_WIN32)
// Do not include pthread on windows. It includes winsock.h, which collides with ws2tcpip.h from fluid_sys.h
// It isn't need on Windows anyway.
#include <pthread.h>
#endif

/* WIN32 HACK - Flag used to differentiate between a file descriptor and a socket.
 * Should work, so long as no SOCKET or file descriptor ends up with this bit set. - JG */
#ifdef _WIN32
#define FLUID_SOCKET_FLAG      0x40000000
#else
#define FLUID_SOCKET_FLAG      0x00000000
#define SOCKET_ERROR           -1
#define INVALID_SOCKET         -1
#endif

/* SCHED_FIFO priority for high priority timer threads */
#define FLUID_SYS_TIMER_HIGH_PRIO_LEVEL         10


struct _fluid_timer_t
{
    long msec;

    // Pointer to a function to be executed by the timer.
    // This field is set to NULL once the timer is finished to indicate completion.
    // This allows for timed waits, rather than waiting forever as fluid_timer_join() does.
    fluid_timer_callback_t callback;
    void *data;
    fluid_thread_t *thread;
    int cont;
    int auto_destroy;
};

struct _fluid_server_socket_t
{
    fluid_socket_t socket;
    fluid_thread_t *thread;
    int cont;
    fluid_server_func_t func;
    void *data;
};


static int fluid_istream_gets(fluid_istream_t in, char *buf, int len);

static fluid_log_function_t fluid_log_function[LAST_LOG_LEVEL] =
{
    fluid_default_log_function,
    fluid_default_log_function,
    fluid_default_log_function,
    fluid_default_log_function,
#ifdef DEBUG
    fluid_default_log_function
#else
    NULL
#endif
};
static void *fluid_log_user_data[LAST_LOG_LEVEL] = { NULL };

static const char fluid_libname[] = "fluidsynth";

/**
 * Installs a new log function for a specified log level.
 * @param level Log level to install handler for.
 * @param fun Callback function handler to call for logged messages
 * @param data User supplied data pointer to pass to log function
 * @return The previously installed function.
 */
fluid_log_function_t
fluid_set_log_function(int level, fluid_log_function_t fun, void *data)
{
    fluid_log_function_t old = NULL;

    if((level >= 0) && (level < LAST_LOG_LEVEL))
    {
        old = fluid_log_function[level];
        fluid_log_function[level] = fun;
        fluid_log_user_data[level] = data;
    }

    return old;
}

/**
 * Default log function which prints to the stderr.
 * @param level Log level
 * @param message Log message
 * @param data User supplied data (not used)
 */
void
fluid_default_log_function(int level, const char *message, void *data)
{
    FILE *out;

#if defined(_WIN32)
    out = stdout;
#else
    out = stderr;
#endif

    switch(level)
    {
    case FLUID_PANIC:
        FLUID_FPRINTF(out, "%s: panic: %s\n", fluid_libname, message);
        break;

    case FLUID_ERR:
        FLUID_FPRINTF(out, "%s: error: %s\n", fluid_libname, message);
        break;

    case FLUID_WARN:
        FLUID_FPRINTF(out, "%s: warning: %s\n", fluid_libname, message);
        break;

    case FLUID_INFO:
        FLUID_FPRINTF(out, "%s: %s\n", fluid_libname, message);
        break;

    case FLUID_DBG:
        FLUID_FPRINTF(out, "%s: debug: %s\n", fluid_libname, message);
        break;

    default:
        FLUID_FPRINTF(out, "%s: %s\n", fluid_libname, message);
        break;
    }

    fflush(out);
}

/**
 * Print a message to the log.
 * @param level Log level (#fluid_log_level).
 * @param fmt Printf style format string for log message
 * @param ... Arguments for printf 'fmt' message string
 * @return Always returns #FLUID_FAILED
 */
int
fluid_log(int level, const char *fmt, ...)
{
    if((level >= 0) && (level < LAST_LOG_LEVEL))
    {
        fluid_log_function_t fun = fluid_log_function[level];

        if(fun != NULL)
        {
            char errbuf[1024];
            
            va_list args;
            va_start(args, fmt);
            FLUID_VSNPRINTF(errbuf, sizeof(errbuf), fmt, args);
            va_end(args);
        
            (*fun)(level, errbuf, fluid_log_user_data[level]);
        }
    }

    return FLUID_FAILED;
}

void* fluid_alloc(size_t len)
{
    void* ptr = malloc(len);

#if defined(DEBUG) && !defined(_MSC_VER)
    // garbage initialize allocated memory for debug builds to ease reproducing
    // bugs like 44453ff23281b3318abbe432fda90888c373022b .
    //
    // MSVC++ already garbage initializes allocated memory by itself (debug-heap).
    //
    // 0xCC because
    // * it makes pointers reliably crash when dereferencing them,
    // * floating points are still some valid but insanely huge negative number, and
    // * if for whatever reason this allocated memory is executed, it'll trigger
    //   INT3 (...at least on x86)
    if(ptr != NULL)
    {
        memset(ptr, 0xCC, len);
    }
#endif
    return ptr;
}

/**
 * Open a file with a UTF-8 string, even in Windows
 * @param filename The name of the file to open
 * @param mode The mode to open the file in
 */
FILE *fluid_fopen(const char *filename, const char *mode)
{
#if defined(_WIN32)
    wchar_t *wpath = NULL, *wmode = NULL;
    FILE *file = NULL;
    int length;
    if ((length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, filename, -1, NULL, 0)) == 0)
    {
        FLUID_LOG(FLUID_ERR, "Unable to perform MultiByteToWideChar() conversion for filename '%s'. Error was: '%s'", filename, fluid_get_windows_error());
        errno = EINVAL;
        goto error_recovery;
    }
    
    wpath = FLUID_MALLOC(length * sizeof(wchar_t));
    if (wpath == NULL)
    {
        FLUID_LOG(FLUID_PANIC, "Out of memory.");
        errno = EINVAL;
        goto error_recovery;
    }

    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, filename, -1, wpath, length);

    if ((length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, mode, -1, NULL, 0)) == 0)
    {
        FLUID_LOG(FLUID_ERR, "Unable to perform MultiByteToWideChar() conversion for mode '%s'. Error was: '%s'", mode, fluid_get_windows_error());
        errno = EINVAL;
        goto error_recovery;
    }

    wmode = FLUID_MALLOC(length * sizeof(wchar_t));
    if (wmode == NULL)
    {
        FLUID_LOG(FLUID_PANIC, "Out of memory.");
        errno = EINVAL;
        goto error_recovery;
    }

    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, mode, -1, wmode, length);

    file = _wfopen(wpath, wmode);

error_recovery:
    FLUID_FREE(wpath);
    FLUID_FREE(wmode);

    return file;
#else
    return fopen(filename, mode);
#endif
}

/**
 * Wrapper for free() that satisfies at least C90 requirements.
 *
 * @param ptr Pointer to memory region that should be freed
 *
 * @note Only use this function when the API documentation explicitly says so. Otherwise use
 * adequate \c delete_fluid_* functions.
 *
 * @warning Calling ::free() on memory that is advised to be freed with fluid_free() results in undefined behaviour!
 * (cf.: "Potential Errors Passing CRT Objects Across DLL Boundaries" found in MS Docs)
 *
 * @since 2.0.7
 */
void fluid_free(void* ptr)
{
    free(ptr);
}

/**
 * An improved strtok, still trashes the input string, but is portable and
 * thread safe.  Also skips token chars at beginning of token string and never
 * returns an empty token (will return NULL if source ends in token chars though).
 * NOTE: NOT part of public API
 * @internal
 * @param str Pointer to a string pointer of source to tokenize.  Pointer gets
 *   updated on each invocation to point to beginning of next token.  Note that
 *   token char gets overwritten with a 0 byte.  String pointer is set to NULL
 *   when final token is returned.
 * @param delim String of delimiter chars.
 * @return Pointer to the next token or NULL if no more tokens.
 */
char *fluid_strtok(char **str, char *delim)
{
    char *s, *d, *token;
    char c;

    if(str == NULL || delim == NULL || !*delim)
    {
        FLUID_LOG(FLUID_ERR, "Null pointer");
        return NULL;
    }

    s = *str;

    if(!s)
    {
        return NULL;    /* str points to a NULL pointer? (tokenize already ended) */
    }

    /* skip delimiter chars at beginning of token */
    do
    {
        c = *s;

        if(!c)	/* end of source string? */
        {
            *str = NULL;
            return NULL;
        }

        for(d = delim; *d; d++)	/* is source char a token char? */
        {
            if(c == *d)	/* token char match? */
            {
                s++;		/* advance to next source char */
                break;
            }
        }
    }
    while(*d);		/* while token char match */

    token = s;		/* start of token found */

    /* search for next token char or end of source string */
    for(s = s + 1; *s; s++)
    {
        c = *s;

        for(d = delim; *d; d++)	/* is source char a token char? */
        {
            if(c == *d)	/* token char match? */
            {
                *s = '\0';	/* overwrite token char with zero byte to terminate token */
                *str = s + 1;	/* update str to point to beginning of next token */
                return token;
            }
        }
    }

    /* we get here only if source string ended */
    *str = NULL;
    return token;
}

/**
 * Get time in milliseconds to be used in relative timing operations.
 * @return Monotonic time in milliseconds.
 */
unsigned int fluid_curtime(void)
{
    double now;
    static double initial_time = 0;

    if(initial_time == 0)
    {
        initial_time = fluid_utime();
    }

    now = fluid_utime();

    return (unsigned int)((now - initial_time) / 1000.0);
}


#if !OSAL_embedded

#if defined(_WIN32)      /* Windoze specific stuff */

void
fluid_thread_self_set_prio(int prio_level)
{
    if(prio_level > 0)
    {
        SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);
    }
}


#elif defined(__OS2__)  /* OS/2 specific stuff */

void
fluid_thread_self_set_prio(int prio_level)
{
    if(prio_level > 0)
    {
        DosSetPriority(PRTYS_THREAD, PRTYC_REGULAR, PRTYD_MAXIMUM, 0);
    }
}

#else /* POSIX stuff..  Nice POSIX..  Good POSIX. */

void
fluid_thread_self_set_prio(int prio_level)
{
    struct sched_param priority;

    if(prio_level > 0)
    {

        memset(&priority, 0, sizeof(priority));
        priority.sched_priority = prio_level;

        if(pthread_setschedparam(pthread_self(), SCHED_FIFO, &priority) == 0)
        {
            return;
        }

#ifdef DBUS_SUPPORT
        /* Try to gain high priority via rtkit */

        if(fluid_rtkit_make_realtime(0, prio_level) == 0)
        {
            return;
        }

#endif
        FLUID_LOG(FLUID_WARN, "Failed to set thread to high priority");
    }
}


#endif	// #else    (its POSIX)

#endif	// #if !OSAL_embedded


#if defined(FPE_CHECK) && !defined(_WIN32) && !defined(__OS2__)

/***************************************************************
 *
 *               Floating point exceptions
 *
 *  The floating point exception functions were taken from Ircam's
 *  jMax source code. https://www.ircam.fr/jmax
 *
 *  FIXME: check in config for i386 machine
 *
 *  Currently not used. I leave the code here in case we want to pick
 *  this up again some time later.
 */

/* Exception flags */
#define _FPU_STATUS_IE    0x001  /* Invalid Operation */
#define _FPU_STATUS_DE    0x002  /* Denormalized Operand */
#define _FPU_STATUS_ZE    0x004  /* Zero Divide */
#define _FPU_STATUS_OE    0x008  /* Overflow */
#define _FPU_STATUS_UE    0x010  /* Underflow */
#define _FPU_STATUS_PE    0x020  /* Precision */
#define _FPU_STATUS_SF    0x040  /* Stack Fault */
#define _FPU_STATUS_ES    0x080  /* Error Summary Status */

/* Macros for accessing the FPU status word.  */

/* get the FPU status */
#define _FPU_GET_SW(sw) __asm__ ("fnstsw %0" : "=m" (*&sw))

/* clear the FPU status */
#define _FPU_CLR_SW() __asm__ ("fnclex" : : )

/* Purpose:
 * Checks, if the floating point unit has produced an exception, print a message
 * if so and clear the exception.
 */
unsigned int fluid_check_fpe_i386(char *explanation)
{
    unsigned int s;

    _FPU_GET_SW(s);
    _FPU_CLR_SW();

    s &= _FPU_STATUS_IE | _FPU_STATUS_DE | _FPU_STATUS_ZE | _FPU_STATUS_OE | _FPU_STATUS_UE;

    if(s)
    {
        FLUID_LOG(FLUID_WARN, "FPE exception (before or in %s): %s%s%s%s%s", explanation,
                  (s & _FPU_STATUS_IE) ? "Invalid operation " : "",
                  (s & _FPU_STATUS_DE) ? "Denormal number " : "",
                  (s & _FPU_STATUS_ZE) ? "Zero divide " : "",
                  (s & _FPU_STATUS_OE) ? "Overflow " : "",
                  (s & _FPU_STATUS_UE) ? "Underflow " : "");
    }

    return s;
}

/* Purpose:
 * Clear floating point exception.
 */
void fluid_clear_fpe_i386(void)
{
    _FPU_CLR_SW();
}

#endif	// #if defined(FPE_CHECK) && !defined(_WIN32) && !defined(__OS2__)


/***************************************************************
 *
 *               Profiling (Linux, i586 only)
 *
 */

#if WITH_PROFILING
/* Profiling interface between profiling command shell and audio rendering API
  (FluidProfile_0004.pdf- 3.2.2).
  Macros are in defined in fluid_sys.h.
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
unsigned short fluid_profile_notes = 0; /* number of generated notes */
/* preset bank:0 prog:16 (organ) */
unsigned char fluid_profile_bank = FLUID_PROFILE_DEFAULT_BANK;
unsigned char fluid_profile_prog = FLUID_PROFILE_DEFAULT_PROG;

/* print mode */
unsigned char fluid_profile_print = FLUID_PROFILE_DEFAULT_PRINT;
/* number of measures */
unsigned short fluid_profile_n_prof = FLUID_PROFILE_DEFAULT_N_PROF;
/* measure duration in ms */
unsigned short fluid_profile_dur = FLUID_PROFILE_DEFAULT_DURATION;
/* lock between multiple-shell */
fluid_atomic_int_t fluid_profile_lock = 0;
/**/

/*----------------------------------------------
  Profiling Data
-----------------------------------------------*/
unsigned char fluid_profile_status = PROFILE_STOP; /* command and status */
unsigned int fluid_profile_end_ticks = 0;          /* ending position (in ticks) */
fluid_profile_data_t fluid_profile_data[] =        /* Data duration */
{
    {"synth_write_* ------------>", 1e10, 0.0, 0.0, 0, 0, 0},
    {"synth_one_block ---------->", 1e10, 0.0, 0.0, 0, 0, 0},
    {"synth_one_block:clear ---->", 1e10, 0.0, 0.0, 0, 0, 0},
    {"synth_one_block:one voice->", 1e10, 0.0, 0.0, 0, 0, 0},
    {"synth_one_block:all voices>", 1e10, 0.0, 0.0, 0, 0, 0},
    {"synth_one_block:reverb --->", 1e10, 0.0, 0.0, 0, 0, 0},
    {"synth_one_block:chorus --->", 1e10, 0.0, 0.0, 0, 0, 0},
    {"voice:note --------------->", 1e10, 0.0, 0.0, 0, 0, 0},
    {"voice:release ------------>", 1e10, 0.0, 0.0, 0, 0, 0}
};


/*----------------------------------------------
  Internal profiling API
-----------------------------------------------*/
/* logging profiling data (used on synthesizer instance deletion) */
void fluid_profiling_print(void)
{
    int i;

    printf("fluid_profiling_print\n");

    FLUID_LOG(FLUID_INFO, "Estimated times: min/avg/max (micro seconds)");

    for(i = 0; i < FLUID_PROFILE_NBR; i++)
    {
        if(fluid_profile_data[i].count > 0)
        {
            FLUID_LOG(FLUID_INFO, "%s: %.3f/%.3f/%.3f",
                      fluid_profile_data[i].description,
                      fluid_profile_data[i].min,
                      fluid_profile_data[i].total / fluid_profile_data[i].count,
                      fluid_profile_data[i].max);
        }
        else
        {
            FLUID_LOG(FLUID_DBG, "%s: no profiling available",
                      fluid_profile_data[i].description);
        }
    }
}

/* Macro that returns cpu load in percent (%)
 * @dur: duration (micro second).
 * @sample_rate: sample_rate used in audio driver (Hz).
 * @n_amples: number of samples collected during 'dur' duration.
*/
#define fluid_profile_load(dur,sample_rate,n_samples) \
        (dur * sample_rate / n_samples / 10000.0)


/* prints cpu loads only
*
* @param sample_rate the sample rate of audio output.
* @param out output stream device.
*
* ------------------------------------------------------------------------------
* Cpu loads(%) (sr: 44100 Hz, sp: 22.68 microsecond) and maximum voices
* ------------------------------------------------------------------------------
* nVoices| total(%)|voices(%)| reverb(%)|chorus(%)| voice(%)|estimated maxVoices
* -------|---------|---------|----------|---------|---------|-------------------
*     250|   41.544|   41.544|     0.000|    0.000|    0.163|              612
*/
static void fluid_profiling_print_load(double sample_rate, fluid_ostream_t out)
{
    unsigned int n_voices; /* voices number */
    static const char max_voices_not_available[] = "      not available";
    const char *pmax_voices;
    char max_voices_available[20];

    /* First computes data to be printed */
    double  total, voices, reverb, chorus, all_voices, voice;
    /* voices number */
    n_voices = fluid_profile_data[FLUID_PROF_ONE_BLOCK_VOICES].count ?
               fluid_profile_data[FLUID_PROF_ONE_BLOCK_VOICES].n_voices /
               fluid_profile_data[FLUID_PROF_ONE_BLOCK_VOICES].count : 0;

    /* total load (%) */
    total =  fluid_profile_data[FLUID_PROF_WRITE].count ?
             fluid_profile_load(fluid_profile_data[FLUID_PROF_WRITE].total, sample_rate,
                                fluid_profile_data[FLUID_PROF_WRITE].n_samples) : 0;

    /* reverb load (%) */
    reverb = fluid_profile_data[FLUID_PROF_ONE_BLOCK_REVERB].count ?
             fluid_profile_load(fluid_profile_data[FLUID_PROF_ONE_BLOCK_REVERB].total,
                                sample_rate,
                                fluid_profile_data[FLUID_PROF_ONE_BLOCK_REVERB].n_samples) : 0;

    /* chorus load (%) */
    chorus = fluid_profile_data[FLUID_PROF_ONE_BLOCK_CHORUS].count ?
             fluid_profile_load(fluid_profile_data[FLUID_PROF_ONE_BLOCK_CHORUS].total,
                                sample_rate,
                                fluid_profile_data[FLUID_PROF_ONE_BLOCK_CHORUS].n_samples) : 0;

    /* total voices load: total - reverb - chorus (%) */
    voices = total - reverb - chorus;

    /* One voice load (%): all_voices / n_voices. */
    all_voices = fluid_profile_data[FLUID_PROF_ONE_BLOCK_VOICES].count ?
                 fluid_profile_load(fluid_profile_data[FLUID_PROF_ONE_BLOCK_VOICES].total,
                                    sample_rate,
                                    fluid_profile_data[FLUID_PROF_ONE_BLOCK_VOICES].n_samples) : 0;

    voice = n_voices ?  all_voices / n_voices : 0;

    /* estimated maximum voices number */
    if(voice > 0)
    {
        FLUID_SNPRINTF(max_voices_available, sizeof(max_voices_available),
                       "%17d", (unsigned int)((100.0 - reverb - chorus) / voice));
        pmax_voices = max_voices_available;
    }
    else
    {
        pmax_voices = max_voices_not_available;
    }

    /* Now prints data */
    fluid_ostream_printf(out,
                         " ------------------------------------------------------------------------------\n");
    fluid_ostream_printf(out,
                         " Cpu loads(%%) (sr:%6.0f Hz, sp:%6.2f microsecond) and maximum voices\n",
                         sample_rate, 1000000.0 / sample_rate);
    fluid_ostream_printf(out,
                         " ------------------------------------------------------------------------------\n");
    fluid_ostream_printf(out,
                         " nVoices| total(%%)|voices(%%)| reverb(%%)|chorus(%%)| voice(%%)|estimated maxVoices\n");
    fluid_ostream_printf(out,
                         " -------|---------|---------|----------|---------|---------|-------------------\n");
    fluid_ostream_printf(out,
                         "%8d|%9.3f|%9.3f|%10.3f|%9.3f|%9.3f|%s\n", n_voices, total, voices,
                         reverb, chorus, voice, pmax_voices);
}

/*
* prints profiling data (used by profile shell command: prof_start).
* The function is an internal profiling API between the "profile" command
* prof_start and audio rendering API (see FluidProfile_0004.pdf - 3.2.2).
*
* @param sample_rate the sample rate of audio output.
* @param out output stream device.
*
* When print mode is 1, the function prints all the information (see below).
* When print mode is 0, the function prints only the cpu loads.
*
* ------------------------------------------------------------------------------
* Duration(microsecond) and cpu loads(%) (sr: 44100 Hz, sp: 22.68 microsecond)
* ------------------------------------------------------------------------------
* Code under profiling       |Voices|       Duration (microsecond)   |  Load(%)
*                            |   nbr|       min|       avg|       max|
* ---------------------------|------|--------------------------------|----------
* synth_write_* ------------>|   250|      3.91|   2188.82|   3275.00|  41.544
* synth_one_block ---------->|   250|   1150.70|   2273.56|   3241.47|  41.100
* synth_one_block:clear ---->|   250|      3.07|      4.62|     61.18|   0.084
* synth_one_block:one voice->|     1|      4.19|      9.02|   1044.27|   0.163
* synth_one_block:all voices>|   250|   1138.41|   2259.11|   3217.73|  40.839
* synth_one_block:reverb --->| no profiling available
* synth_one_block:chorus --->| no profiling available
* voice:note --------------->| no profiling available
* voice:release ------------>| no profiling available
* ------------------------------------------------------------------------------
* Cpu loads(%) (sr: 44100 Hz, sp: 22.68 microsecond) and maximum voices
* ------------------------------------------------------------------------------
* nVoices| total(%)|voices(%)| reverb(%)|chorus(%)| voice(%)|estimated maxVoices
* -------|---------|---------|----------|---------|---------|-------------------
*     250|   41.544|   41.544|     0.000|    0.000|    0.163|              612
*/
void fluid_profiling_print_data(double sample_rate, fluid_ostream_t out)
{
    int i;

    if(fluid_profile_print)
    {
        /* print all details: Duration(microsecond) and cpu loads(%) */
        fluid_ostream_printf(out,
                             " ------------------------------------------------------------------------------\n");
        fluid_ostream_printf(out,
                             " Duration(microsecond) and cpu loads(%%) (sr:%6.0f Hz, sp:%6.2f microsecond)\n",
                             sample_rate, 1000000.0 / sample_rate);
        fluid_ostream_printf(out,
                             " ------------------------------------------------------------------------------\n");
        fluid_ostream_printf(out,
                             " Code under profiling       |Voices|       Duration (microsecond)   |  Load(%%)\n");
        fluid_ostream_printf(out,
                             "                            |   nbr|       min|       avg|       max|\n");
        fluid_ostream_printf(out,
                             " ---------------------------|------|--------------------------------|----------\n");

        for(i = 0; i < FLUID_PROFILE_NBR; i++)
        {
            unsigned int count = fluid_profile_data[i].count;

            if(count > 0)
            {
                /* data are available */

                if(FLUID_PROF_WRITE <= i && i <= FLUID_PROF_ONE_BLOCK_CHORUS)
                {
                    double load = fluid_profile_load(fluid_profile_data[i].total, sample_rate,
                                                     fluid_profile_data[i].n_samples);
                    fluid_ostream_printf(out, " %s|%6d|%10.2f|%10.2f|%10.2f|%8.3f\n",
                                         fluid_profile_data[i].description, /* code under profiling */
                                         fluid_profile_data[i].n_voices / count, /* voices number */
                                         fluid_profile_data[i].min,              /* minimum duration */
                                         fluid_profile_data[i].total / count,    /* average duration */
                                         fluid_profile_data[i].max,              /* maximum duration */
                                         load);                                  /* cpu load */
                }
                else
                {
                    /* note and release duration */
                    fluid_ostream_printf(out, " %s|%6d|%10.0f|%10.0f|%10.0f|\n",
                                         fluid_profile_data[i].description, /* code under profiling */
                                         fluid_profile_data[i].n_voices / count,
                                         fluid_profile_data[i].min,              /* minimum duration */
                                         fluid_profile_data[i].total / count,    /* average duration */
                                         fluid_profile_data[i].max);             /* maximum duration */
                }
            }
            else
            {
                /* data aren't available */
                fluid_ostream_printf(out,
                                     " %s| no profiling available\n", fluid_profile_data[i].description);
            }
        }
    }

    /* prints cpu loads only */
    fluid_profiling_print_load(sample_rate, out);/* prints cpu loads */
}

/*
 Returns true if the user cancels the current profiling measurement.
 Actually this is implemented using the <ENTER> key. To add this functionality:
 1) Adds #define FLUID_PROFILE_CANCEL in fluid_sys.h.
 2) Adds the necessary code inside fluid_profile_is_cancel().

 When FLUID_PROFILE_CANCEL is not defined, the function return FALSE.
*/
int fluid_profile_is_cancel_req(void)
{
#ifdef FLUID_PROFILE_CANCEL

#if defined(_WIN32)      /* Windows specific stuff */
    /* Profile cancellation is supported for Windows */
    /* returns TRUE if key <ENTER> is depressed */
    return(GetAsyncKeyState(VK_RETURN) & 0x1);

#elif defined(__OS2__)  /* OS/2 specific stuff */
    /* Profile cancellation isn't yet supported for OS2 */
    /* For OS2, replaces the following  line with the function that returns
    true when the keyboard key <ENTER> is depressed */
    return FALSE; /* default value */

#else   /* POSIX stuff */
    /* Profile cancellation is supported for Linux */
    /* returns true is <ENTER> is depressed */
    {
        /* Here select() is used to poll the standard input to see if an input
         is ready. As the standard input is usually buffered, the user
         needs to depress <ENTER> to set the input to a "ready" state.
        */
        struct timeval tv;
        fd_set fds;    /* just one fds need to be polled */
        tv.tv_sec = 0; /* Setting both values to 0, means a 0 timeout */
        tv.tv_usec = 0;
        FD_ZERO(&fds); /* reset fds */
        FD_SET(STDIN_FILENO, &fds); /* sets fds to poll standard input only */
        select(STDIN_FILENO + 1, &fds, NULL, NULL, &tv); /* polling */
        return (FD_ISSET(0, &fds)); /* returns true if standard input is ready */
    }
#endif /* OS stuff */

#else /* FLUID_PROFILE_CANCEL not defined */
    return FALSE; /* default value */
#endif /* FLUID_PROFILE_CANCEL */
}

/**
* Returns status used in shell command "prof_start".
* The function is an internal profiling API between the "profile" command
* prof_start and audio rendering API (see FluidProfile_0004.pdf - 3.2.2).
*
* @return status
* - PROFILE_READY profiling data are ready.
* - PROFILE_RUNNING, profiling data are still under acquisition.
* - PROFILE_CANCELED, acquisition has been cancelled by the user.
* - PROFILE_STOP, no acquisition in progress.
*
* When status is PROFILE_RUNNING, the caller can do passive waiting, or other
* work before recalling the function later.
*/
int fluid_profile_get_status(void)
{
    /* Checks if user has requested to cancel the current measurement */
    /* Cancellation must have precedence over other status */
    if(fluid_profile_is_cancel_req())
    {
        fluid_profile_start_stop(0, 0); /* stops the measurement */
        return PROFILE_CANCELED;
    }

    switch(fluid_profile_status)
    {
    case PROFILE_READY:
        return PROFILE_READY; /* profiling data are ready */

    case PROFILE_START:
        return PROFILE_RUNNING;/* profiling data are under acquisition */

    default:
        return PROFILE_STOP;
    }
}

/**
*  Starts or stops profiling measurement.
*  The function is an internal profiling API between the "profile" command
*  prof_start and audio rendering API (see FluidProfile_0004.pdf - 3.2.2).
*
*  @param end_tick end position of the measure (in ticks).
*  - If end_tick is greater then 0, the function starts a measure if a measure
*    isn't running. If a measure is already running, the function does nothing
*    and returns.
*  - If end_tick is 0, the function stops a measure.
*  @param clear_data,
*  - If clear_data is 0, the function clears fluid_profile_data before starting
*    a measure, otherwise, the data from the started measure will be accumulated
*    within fluid_profile_data.
*/
void fluid_profile_start_stop(unsigned int end_ticks, short clear_data)
{
    if(end_ticks)    /* This is a "start" request */
    {
        /* Checks if a measure is already running */
        if(fluid_profile_status != PROFILE_START)
        {
            short i;
            fluid_profile_end_ticks = end_ticks;

            /* Clears profile data */
            if(clear_data == 0)
            {
                for(i = 0; i < FLUID_PROFILE_NBR; i++)
                {
                    fluid_profile_data[i].min = 1e10;/* min sets to max value */
                    fluid_profile_data[i].max = 0;   /* maximum sets to min value */
                    fluid_profile_data[i].total = 0; /* total duration microsecond */
                    fluid_profile_data[i].count = 0;    /* data count */
                    fluid_profile_data[i].n_voices = 0; /* voices number */
                    fluid_profile_data[i].n_samples = 0;/* audio samples number */
                }
            }

            fluid_profile_status = PROFILE_START;	/* starts profiling */
        }

        /* else do nothing when profiling is already started */
    }
    else /* This is a "stop" request */
    {
        /* forces the current running profile (if any) to stop */
        fluid_profile_status = PROFILE_STOP;	/* stops profiling */
    }
}

#endif /* WITH_PROFILING */

/***************************************************************
 *
 *               Threads
 *
 */

fluid_pointer_t
fluid_thread_high_prio(fluid_pointer_t data)
{
    fluid_thread_info_t *info = data;

    fluid_thread_self_set_prio(info->prio_level);

    info->func(info->data);
    FLUID_FREE(info);

    return NULL;
}


static fluid_thread_return_t
fluid_timer_run(void *data)
{
    fluid_timer_t *timer;
    int count = 0;
    int cont;
    long start;
    long delay;

    timer = (fluid_timer_t *)data;

    /* keep track of the start time for absolute positioning */
    start = fluid_curtime();

    while(timer->cont)
    {
        cont = (*timer->callback)(timer->data, fluid_curtime() - start);

        count++;

        if(!cont)
        {
            break;
        }

        /* to avoid incremental time errors, calculate the delay between
           two callbacks bringing in the "absolute" time (count *
           timer->msec) */
        delay = (count * timer->msec) - (fluid_curtime() - start);

        if(delay > 0)
        {
            fluid_msleep(delay);
        }
    }

    FLUID_LOG(FLUID_DBG, "Timer thread finished");
    timer->callback = NULL;

    if(timer->auto_destroy)
    {
        FLUID_FREE(timer);
    }

    return FLUID_THREAD_RETURN_VALUE;
}

fluid_timer_t *
new_fluid_timer(int msec, fluid_timer_callback_t callback, void *data,
                int new_thread, int auto_destroy, int high_priority)
{
    fluid_timer_t *timer;

    timer = FLUID_NEW(fluid_timer_t);

    if(timer == NULL)
    {
        FLUID_LOG(FLUID_ERR, "Out of memory");
        return NULL;
    }

    timer->msec = msec;
    timer->callback = callback;
    timer->data = data;
    timer->cont = TRUE ;
    timer->thread = NULL;
    timer->auto_destroy = auto_destroy;

    if(new_thread)
    {
        timer->thread = new_fluid_thread("timer", fluid_timer_run, timer, high_priority
                                         ? FLUID_SYS_TIMER_HIGH_PRIO_LEVEL : 0, FALSE);

        if(!timer->thread)
        {
            FLUID_FREE(timer);
            return NULL;
        }
    }
    else
    {
        fluid_timer_run(timer);   /* Run directly, instead of as a separate thread */

        if(auto_destroy)
        {
            /* do NOT return freed memory */
            return NULL;
        }
    }

    return timer;
}

void
delete_fluid_timer(fluid_timer_t *timer)
{
    int auto_destroy;
    fluid_return_if_fail(timer != NULL);

    auto_destroy = timer->auto_destroy;

    timer->cont = 0;
    fluid_timer_join(timer);

    /* Shouldn't access timer now if auto_destroy enabled, since it has been destroyed */

    if(!auto_destroy)
    {
        FLUID_FREE(timer);
    }
}

int
fluid_timer_join(fluid_timer_t *timer)
{
    int auto_destroy;

    if(timer->thread)
    {
        auto_destroy = timer->auto_destroy;
        fluid_thread_join(timer->thread);

        if(!auto_destroy)
        {
            timer->thread = NULL;
        }
    }

    return FLUID_OK;
}

int
fluid_timer_is_running(const fluid_timer_t *timer)
{
    // for unit test usage only
    return timer != NULL && timer->callback != NULL;
}

long fluid_timer_get_interval(const fluid_timer_t * timer)
{
    // for unit test usage only
    if (timer == NULL)
        return 0;
    return timer->msec;
}


/***************************************************************
 *
 *               Sockets and I/O
 *
 */

/**
 * Get standard in stream handle.
 * @return Standard in stream.
 */
fluid_istream_t
fluid_get_stdin(void)
{
    return STDIN_FILENO;
}

/**
 * Get standard output stream handle.
 * @return Standard out stream.
 */
fluid_ostream_t
fluid_get_stdout(void)
{
    return STDOUT_FILENO;
}

/**
 * Read a line from an input stream.
 * @return 0 if end-of-stream, -1 if error, non zero otherwise
 */
int
fluid_istream_readline(fluid_istream_t in, fluid_ostream_t out, char *prompt,
                       char *buf, int len)
{
#if READLINE_SUPPORT

    if(in == fluid_get_stdin())
    {
        char *line;

        line = readline(prompt);

        if(line == NULL)
        {
            return -1;
        }

        FLUID_SNPRINTF(buf, len, "%s", line);
        buf[len - 1] = 0;

        if(buf[0] != '\0')
        {
            add_history(buf);
        }

        free(line);
        return 1;
    }
    else
#endif
    {
        fluid_ostream_printf(out, "%s", prompt);
        return fluid_istream_gets(in, buf, len);
    }
}

/**
 * Reads a line from an input stream (socket).
 * @param in The input socket
 * @param buf Buffer to store data to
 * @param len Maximum length to store to buf
 * @return 1 if a line was read, 0 on end of stream, -1 on error
 */
static int
fluid_istream_gets(fluid_istream_t in, char *buf, int len)
{
    char c;
    int n;

    buf[len - 1] = 0;

    while(--len > 0)
    {
#ifndef _WIN32
        n = read(in, &c, 1);

        if(n == -1)
        {
            return -1;
        }

#else

        /* Handle read differently depending on if its a socket or file descriptor */
        if(!(in & FLUID_SOCKET_FLAG))
        {
            // usually read() is supposed to return '\n' as last valid character of the user input
            // when compiled with compatibility for WinXP however, read() may return 0 (EOF) rather than '\n'
            // this would cause the shell to exit early
            n = read(in, &c, 1);

            if(n == -1)
            {
                return -1;
            }
        }
        else
        {
#ifdef NETWORK_SUPPORT
            n = recv(in & ~FLUID_SOCKET_FLAG, &c, 1, 0);
            if(n == SOCKET_ERROR)
#endif
            {
                return -1;
            }
        }

#endif

        if(n == 0)
        {
            *buf = 0;
            // return 1 if read from stdin, else 0, to fix early exit of shell
            return (in == STDIN_FILENO);
        }

        if(c == '\n')
        {
            *buf = 0;
            return 1;
        }

        /* Store all characters excluding CR */
        if(c != '\r')
        {
            *buf++ = c;
        }
    }

    return -1;
}

/**
 * Send a printf style string with arguments to an output stream (socket).
 * @param out Output stream
 * @param format printf style format string
 * @param ... Arguments for the printf format string
 * @return Number of bytes written or -1 on error
 */
int
fluid_ostream_printf(fluid_ostream_t out, const char *format, ...)
{
    char buf[4096];
    va_list args;
    int len;

    va_start(args, format);
    len = FLUID_VSNPRINTF(buf, 4095, format, args);
    va_end(args);

    if(len == 0)
    {
        return 0;
    }

    if(len < 0)
    {
        printf("fluid_ostream_printf: buffer overflow");
        return -1;
    }

    buf[4095] = 0;

#ifndef _WIN32
    return write(out, buf, FLUID_STRLEN(buf));
#else
    {
        int retval;

        /* Handle write differently depending on if its a socket or file descriptor */
        if(!(out & FLUID_SOCKET_FLAG))
        {
            return write(out, buf, (unsigned int)FLUID_STRLEN(buf));
        }

#ifdef NETWORK_SUPPORT
        /* Socket */
        retval = send(out & ~FLUID_SOCKET_FLAG, buf, (int)FLUID_STRLEN(buf), 0);
        return retval != SOCKET_ERROR ? retval : -1;
#else
        return -1;
#endif
    }
#endif
}

#ifdef NETWORK_SUPPORT

int fluid_server_socket_join(fluid_server_socket_t *server_socket)
{
    return fluid_thread_join(server_socket->thread);
}

static int fluid_socket_init(void)
{
#ifdef _WIN32
    WSADATA wsaData;
    int res = WSAStartup(MAKEWORD(2, 2), &wsaData);

    if(res != 0)
    {
        FLUID_LOG(FLUID_ERR, "Server socket creation error: WSAStartup failed: %d", res);
        return FLUID_FAILED;
    }

#endif

    return FLUID_OK;
}

static void fluid_socket_cleanup(void)
{
#ifdef _WIN32
    WSACleanup();
#endif
}

static int fluid_socket_get_error(void)
{
#ifdef _WIN32
    return (int)WSAGetLastError();
#else
    return errno;
#endif
}

fluid_istream_t fluid_socket_get_istream(fluid_socket_t sock)
{
    return sock | FLUID_SOCKET_FLAG;
}

fluid_ostream_t fluid_socket_get_ostream(fluid_socket_t sock)
{
    return sock | FLUID_SOCKET_FLAG;
}

void fluid_socket_close(fluid_socket_t sock)
{
    if(sock != INVALID_SOCKET)
    {
#ifdef _WIN32
        closesocket(sock);

#else
        close(sock);
#endif
    }
}

static fluid_thread_return_t fluid_server_socket_run(void *data)
{
    fluid_server_socket_t *server_socket = (fluid_server_socket_t *)data;
    fluid_socket_t client_socket;
#ifdef IPV6_SUPPORT
    struct sockaddr_in6 addr;
#else
    struct sockaddr_in addr;
#endif

#ifdef HAVE_INETNTOP
#ifdef IPV6_SUPPORT
    char straddr[INET6_ADDRSTRLEN];
#else
    char straddr[INET_ADDRSTRLEN];
#endif /* IPV6_SUPPORT */
#endif /* HAVE_INETNTOP */

    socklen_t addrlen = sizeof(addr);
    int r;
    FLUID_MEMSET((char *)&addr, 0, sizeof(addr));

    FLUID_LOG(FLUID_DBG, "Server listening for connections");

    while(server_socket->cont)
    {
        client_socket = accept(server_socket->socket, (struct sockaddr *)&addr, &addrlen);

        FLUID_LOG(FLUID_DBG, "New client connection");

        if(client_socket == INVALID_SOCKET)
        {
            if(server_socket->cont)
            {
                FLUID_LOG(FLUID_ERR, "Got error %d while trying to accept connection", fluid_socket_get_error());
            }

            server_socket->cont = 0;
            return FLUID_THREAD_RETURN_VALUE;
        }
        else
        {
#ifdef HAVE_INETNTOP

#ifdef IPV6_SUPPORT
            inet_ntop(AF_INET6, &addr.sin6_addr, straddr, sizeof(straddr));
#else
            inet_ntop(AF_INET, &addr.sin_addr, straddr, sizeof(straddr));
#endif

            r = server_socket->func(server_socket->data, client_socket,
                                    straddr);
#else
            r = server_socket->func(server_socket->data, client_socket,
                                    inet_ntoa(addr.sin_addr));
#endif

            if(r != 0)
            {
                fluid_socket_close(client_socket);
            }
        }
    }

    FLUID_LOG(FLUID_DBG, "Server closing");

    return FLUID_THREAD_RETURN_VALUE;
}

fluid_server_socket_t *
new_fluid_server_socket(int port, fluid_server_func_t func, void *data)
{
    fluid_server_socket_t *server_socket;
    struct sockaddr_in addr4;
#ifdef IPV6_SUPPORT
    struct sockaddr_in6 addr6;
#endif
    const struct sockaddr *addr;
    size_t addr_size;
    fluid_socket_t sock;

    fluid_return_val_if_fail(func != NULL, NULL);

    if(fluid_socket_init() != FLUID_OK)
    {
        return NULL;
    }

    FLUID_MEMSET(&addr4, 0, sizeof(addr4));
    addr4.sin_family = AF_INET;
    addr4.sin_port = htons((uint16_t)port);
    addr4.sin_addr.s_addr = htonl(INADDR_ANY);

#ifdef IPV6_SUPPORT
    FLUID_MEMSET(&addr6, 0, sizeof(addr6));
    addr6.sin6_family = AF_INET6;
    addr6.sin6_port = htons((uint16_t)port);
    addr6.sin6_addr = in6addr_any;
#endif

#ifdef IPV6_SUPPORT
    sock = socket(AF_INET6, SOCK_STREAM, 0);
    addr = (const struct sockaddr *) &addr6;
    addr_size = sizeof(addr6);

    if(sock == INVALID_SOCKET)
    {
        FLUID_LOG(FLUID_WARN, "Got error %d while trying to create IPv6 server socket (will try with IPv4)", fluid_socket_get_error());

        sock = socket(AF_INET, SOCK_STREAM, 0);
        addr = (const struct sockaddr *) &addr4;
        addr_size = sizeof(addr4);
    }

#else
    sock = socket(AF_INET, SOCK_STREAM, 0);
    addr = (const struct sockaddr *) &addr4;
    addr_size = sizeof(addr4);
#endif

    if(sock == INVALID_SOCKET)
    {
        FLUID_LOG(FLUID_ERR, "Got error %d while trying to create server socket", fluid_socket_get_error());
        fluid_socket_cleanup();
        return NULL;
    }
    
    if(bind(sock, addr, (int) addr_size) == SOCKET_ERROR)
    {
        FLUID_LOG(FLUID_ERR, "Got error %d while trying to bind server socket", fluid_socket_get_error());
        fluid_socket_close(sock);
        fluid_socket_cleanup();
        return NULL;
    }

    if(listen(sock, SOMAXCONN) == SOCKET_ERROR)
    {
        FLUID_LOG(FLUID_ERR, "Got error %d while trying to listen on server socket", fluid_socket_get_error());
        fluid_socket_close(sock);
        fluid_socket_cleanup();
        return NULL;
    }

    server_socket = FLUID_NEW(fluid_server_socket_t);

    if(server_socket == NULL)
    {
        FLUID_LOG(FLUID_ERR, "Out of memory");
        fluid_socket_close(sock);
        fluid_socket_cleanup();
        return NULL;
    }

    server_socket->socket = sock;
    server_socket->func = func;
    server_socket->data = data;
    server_socket->cont = 1;

    server_socket->thread = new_fluid_thread("server", fluid_server_socket_run, server_socket,
                            0, FALSE);

    if(server_socket->thread == NULL)
    {
        FLUID_FREE(server_socket);
        fluid_socket_close(sock);
        fluid_socket_cleanup();
        return NULL;
    }

    return server_socket;
}

void delete_fluid_server_socket(fluid_server_socket_t *server_socket)
{
    fluid_return_if_fail(server_socket != NULL);

    server_socket->cont = 0;

    if(server_socket->socket != INVALID_SOCKET)
    {
        fluid_socket_close(server_socket->socket);
    }

    if(server_socket->thread)
    {
        fluid_thread_join(server_socket->thread);
        delete_fluid_thread(server_socket->thread);
    }

    FLUID_FREE(server_socket);

    // Should be called the same number of times as fluid_socket_init()
    fluid_socket_cleanup();
}

#endif // NETWORK_SUPPORT

FILE* fluid_file_open(const char* path, const char** errMsg)
{
    static const char ErrExist[] = "File does not exist.";
    static const char ErrRegular[] = "File is not regular, refusing to open it.";
    static const char ErrNull[] = "File does not exists or insufficient permissions to open it.";
    
    FILE* handle = NULL;
    
    if(!fluid_file_test(path, FLUID_FILE_TEST_EXISTS))
    {
        if(errMsg != NULL)
        {
            *errMsg = ErrExist;
        }
    }
    else if(!fluid_file_test(path, FLUID_FILE_TEST_IS_REGULAR))
    {
        if(errMsg != NULL)
        {
            *errMsg = ErrRegular;
        }
    }
    else if((handle = FLUID_FOPEN(path, "rb")) == NULL)
    {
        if(errMsg != NULL)
        {
            *errMsg = ErrNull;
        }
    }
    
    return handle;
}

fluid_long_long_t fluid_file_tell(FILE* f)
{
#ifdef _WIN32
    // On Windows, long is only a 32 bit integer. Thus ftell() does not support to handle files >2GiB.
    // We should use _ftelli64() in this case, however its availability depends on MS CRT and might not be
    // available on WindowsXP, Win98, etc.
    //
    // The web recommends to fallback to _telli64() in this case. However, it's return value differs from
    // _ftelli64() on Win10: https://github.com/FluidSynth/fluidsynth/pull/629#issuecomment-602238436
    //
    // Thus, we use fgetpos().
    fpos_t pos;
    if(fgetpos(f, &pos) != 0)
    {
        return (fluid_long_long_t)-1L;
    }
    return pos;
#else
    return ftell(f);
#endif
}

#if defined(_WIN32) || defined(__CYGWIN__)
// not thread-safe!
#define FLUID_WINDOWS_MEX_ERROR_LEN    1024

char* fluid_get_windows_error(void)
{
#ifdef _UNICODE
    TCHAR err[FLUID_WINDOWS_MEX_ERROR_LEN];
    static char ascii_err[FLUID_WINDOWS_MEX_ERROR_LEN];
#else
    static TCHAR err[FLUID_WINDOWS_MEX_ERROR_LEN];
#endif

    FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM,
                  NULL,
                  GetLastError(),
                  MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US),
                  err,
                  sizeof(err)/sizeof(err[0]),
                  NULL);

#ifdef _UNICODE
    WideCharToMultiByte(CP_UTF8, 0, err, -1, ascii_err, sizeof(ascii_err)/sizeof(ascii_err[0]), 0, 0);
    return ascii_err;
#else
    return err;
#endif
}
#endif

static int fluid_strallocv_internal(char ***current, int *count, int add)
{
    int i;
    char **new;

    new = FLUID_REALLOC(*current, sizeof(char *) * (*count + add));
    if (new == NULL)
    {
        if (*current != NULL)
            fluid_strfreev_internal(*current);
        return FALSE;
    }

    for (i = 0; i < add; i++)
        new[*count + i] = NULL;

    *current = new;
    *count += add;
    return TRUE;
}

int fluid_shell_parse_argv_internal(const char *line, int *argcp, char ***argvp)
{
    enum parse_state {
        STATE_NORMAL,
        STATE_ESCAPE_NORMAL,
        STATE_ESCAPE_DOUBLE_QUOTE,
        STATE_SINGLE_QUOTE,
        STATE_DOUBLE_QUOTE,
        STATE_COMMENT
    };

    enum parse_state state = STATE_NORMAL;
    size_t line_length;
    char *buffer = NULL;
    char *token = NULL;
    int length = 0;
    int max = 0;
    char current;

    if (line == NULL || argcp == NULL || argvp == NULL)
        return FALSE;

    line_length = strlen(line);
    if (line_length == 0)
        return FALSE;

    buffer = (char *)FLUID_MALLOC(line_length + 1);
    if (buffer == NULL)
        return FALSE;

    *argcp = 0;
    *argvp = NULL;

    #define append() buffer[length++] = current;

    do
    {
        current = *line++;
        if (current == 0 && state != STATE_NORMAL)
            break;

        switch (state)
        {
        case STATE_NORMAL:
        {
            switch (current)
            {
            case '\\':
                state = STATE_ESCAPE_NORMAL;
                break;
            case '\'':
                state = STATE_SINGLE_QUOTE;
                break;
            case '"':
                state = STATE_DOUBLE_QUOTE;
                break;

            case ' ':
            case '\t':
            case '\n':
            case '\0':
                if (length == 0)
                    break;

                buffer[length] = 0;
                token = FLUID_STRDUP(buffer);

                if (token == NULL || (*argcp >= max && !fluid_strallocv_internal(argvp, &max, 10)))
                {
                    FLUID_FREE(token);
                    FLUID_FREE(buffer);
                    return FALSE;
                }

                buffer[length] = 0;
                (*argvp)[(*argcp)++] = token;
                length = 0;
                break;

            case '#':
                if (length == 0)
                {
                    state = STATE_COMMENT;
                    break;
                }

                // fall through

            default:
                append();
                break;
            }

            break;
        }

        case STATE_ESCAPE_NORMAL:
        {
            state = STATE_NORMAL;
            switch (current)
            {
            case '\n':
                break;
            default:
                append();
                break;
            }
            break;
        }

        case STATE_ESCAPE_DOUBLE_QUOTE:
        {
            state = STATE_DOUBLE_QUOTE;
            switch (current)
            {
            case '"':
            case '\\':
            /* the ones below aren't useful, they are only here to mimic GLib */
            case '`':
            case '$':
            case '\n':
            {
                append();
                break;
            }

            default:
            {
                /* fill back the dropped backslash, this does not increase length  */
                buffer[length++] = '\\';
                append();
                break;
            }
            }

            break;
        }

        case STATE_SINGLE_QUOTE:
        {
            switch (current)
            {
            case '\'':
                state = STATE_NORMAL;
                break;
            default:
                append();
                break;
            }
            break;
        }

        case STATE_DOUBLE_QUOTE:
        {
            switch (current)
            {
            case '\\':
                state = STATE_ESCAPE_DOUBLE_QUOTE;
                break;
            case '"':
                state = STATE_NORMAL;
                break;
            default:
                append();
                break;
            }
            break;
        }

        case STATE_COMMENT:
            break;
        }
    } while (current != 0);

    FLUID_FREE(buffer);

    if (state != STATE_NORMAL && state != STATE_COMMENT)
    {
        fluid_strfreev_internal(*argvp);
        return FALSE;
    }

    return *argcp > 0;
}

void fluid_strfreev_internal(char **argvp)
{
    int i = 0;

    if (argvp == NULL)
        return;

    for (; argvp[i] != NULL; i++)
    {
        FLUID_FREE(argvp[i]);
    }

    FLUID_FREE(argvp);
}
