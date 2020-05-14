/*
 * Automatic Framerate Daemon for AMLogic S905/S912-based boxes.
 * Copyright (C) 2017-2019 Andrey Zabolotnyi <zapparello@ya.ru>
 *
 * For copying conditions, see file COPYING.txt.
 *
 * Main header file
 */

#ifndef __AFRD_H__
#define __AFRD_H__

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <errno.h>
#include <poll.h>

#include "mstime.h"
#include "cfg_parse.h"

// uncomment for more verbose debug messages
//#define AFRD_DEBUG

// afrd API is available through localhost:50505
#define AFRD_API_PORT			50505

#define DEFAULT_HDMI_DEV		"/sys/class/amhdmitx/amhdmitx0"
#define DEFAULT_HDMI_STATE		"/sys/class/switch/hdmi/state"
#define DEFAULT_VIDEO_MODE		"/sys/class/display/mode"
#define DEFAULT_VDEC_SYSFS		"/sys/class/vdec"
#define DEFAULT_HDCP_AUTHENTICATED	"/sys/module/hdmitx20/parameters/hdmi_authenticated"
#define DEFAULT_SWITCH_DELAY_ON		250
#define DEFAULT_SWITCH_DELAY_OFF	5000
#define DEFAULT_SWITCH_DELAY_RETRY	500
#define DEFAULT_SWITCH_TIMEOUT		3000
#define DEFAULT_SWITCH_BLACKOUT		150
#define DEFAULT_SWITCH_IGNORE		200
#define DEFAULT_SWITCH_HDMI		2000
#define DEFAULT_SWITCH_HDCP		2000
#define DEFAULT_MODE_PREFER_EXACT	0
#define DEFAULT_MODE_USE_FRACT		0

#define ARRAY_SIZE(x)			(sizeof (x) / sizeof (x [0]))

/// display mode information
typedef struct
{
	char name [32];
	int width;
	int height;
	int framerate;
	bool interlaced;
	bool fractional;
} display_mode_t;

#define HZ_FMT		"%u.%02u"
#define HZ_ARGS(hz)	((hz) >> 8), ((100 * ((hz) & 255) + 128) >> 8)

// printf ("mode: "DISPMODE_FMT, DISPMODE_ARGS(mode, display_mode_hz (&mode)))
#define DISPMODE_FMT			"%s (%ux%u@"HZ_FMT"Hz%s)"
#define DISPMODE_ARGS(mode, hz) \
	(mode).name, (mode).width, (mode).height, HZ_ARGS (hz), \
	(mode).interlaced ? ", interlaced" : ""

// make a .8 fixed-point number, i integer and f fractional part (0 to 999)
#define FP8(i,f)	(((i) << 8) | (((f) * 256 + 500) / 1000))

// program name
extern const char *g_program;
// program version
extern const char *g_version;
// program version suffix
extern const char *g_ver_sfx;
// path to PID file
extern const char *g_pidfile;
// program build date/time
extern const char *g_bdate;
// the file name of the active config
extern const char *g_config;
// the global config
extern struct cfg_struct *g_cfg;
// trace calls if non-zero
extern int g_verbose;
// set asynchronously to 1 to initiate shutdown
extern volatile int g_shutdown;
// sysfs path to hdmi interface
extern const char *g_hdmi_dev;
// sysfs path to current display mode
extern const char *g_mode_path;

// the list of supported video modes
extern display_mode_t *g_modes;
// number of video modes in the list
extern int g_modes_n;
// current video mode
extern display_mode_t g_current_mode;
// true if screen is disabled
extern bool g_blackened;
// the delay before switching display mode
extern int g_mode_switch_delay;

// trace calls if g_verbose != 0
extern void trace (int level, const char *format, ...) __attribute__((format(printf,2,3)));
// enable logging trace()s to file
extern void trace_log (const char *logfn);
// flush written log entries to disk
extern void trace_sync ();

#ifdef AFRD_DEBUG
#  define dtrace(args...)	trace (args)
#else
#  define dtrace(args...)
#endif

extern int afrd_init ();
extern int afrd_run ();
extern void afrd_fini ();
extern void afrd_emerg ();

// read the list of all supported display modes and current display mode
extern int display_modes_init ();
// free the list of supported modes
extern void display_modes_fini ();
// query the current video mode
extern void display_mode_get_current ();
// check if two display modes have same attributes
extern bool display_mode_equal (display_mode_t *mode1, display_mode_t *mode2);
// return display mode refresh rate in 24.8 fixed-point format
extern int display_mode_hz (display_mode_t *mode);
// set fractional framerate if that is closer to hz (24.8 fixed-point)
extern void display_mode_set_hz (display_mode_t *mode, int hz);
// switch video mode
extern void display_mode_switch (display_mode_t *mode, bool force);
// disable the screen
extern void display_mode_null ();

// detect current HDCP mode
extern void hdcp_init ();
// terminate HDCP stuff
extern void hdcp_fini ();
// restore HDCP state as detected
extern void hdcp_restore (bool force);
// check if HDCP is supported but disabled and enable it back if so
extern void hdcp_check ();

// load config from file
extern int load_config (const char *config);
// superstructure on cfg_parse
extern const char *cfg_get_str (const char *key, const char *defval);
extern int cfg_get_int (const char *key, int defval);

// helper functions for sysfs
extern char *sysfs_read (const char *device_attr);
// unlike _read, removes trailing spaces and newlines
extern char *sysfs_get_str (const char *device, const char *attr);
extern int sysfs_get_int (const char *device, const char *attr);

extern int sysfs_write (const char *device_attr, const char *value);
extern int sysfs_set_str (const char *device, const char *attr, const char *value);
extern int sysfs_set_int (const char *device, const char *attr, int value);

extern int sysfs_exists (const char *device_attr);

// " \t\r\n"
extern const char *spaces;

// Return strlen(starts) if str starts with it, 0 otherwise
extern int strskip (const char *str, const char *starts);
// Move backwards from eol down to start, replacing the last space with a \0
extern void strip_trailing_spaces (char *eol, const char *start);
// Evaluates the number pointed by line until a non-digit is encountered
extern int parse_int (char **line);
// Similar but first looks for number prefix in line and sets ok to false on error
extern unsigned long find_ulong (const char *str, const char *prefix, bool *ok);
// Same but returns a unsigned long long
extern unsigned long long find_ulonglong (const char *str, const char *prefix, bool *ok);

typedef struct
{
	// number of elements in the list
	int size;
	// array of string pointers
	char **data;
} strlist_t;

// Load a space-separated list from config key
extern bool strlist_load (strlist_t *list, const char *key, const char *desc);
// Free a string list
extern void strlist_free (strlist_t *list);
// Check if string list contains selected value
extern bool strlist_contains (strlist_t *list, const char *str);

/// afrd statistics in shared memory
typedef struct
{
	/// CRC32 checksum, also 'modified' flag, all data except first & last fields
	uint32_t crc32;
	/// sizeof (afrd_shmem_t)
	uint16_t size;
	/// afrd is enabled?
	bool enabled;
	/// display refresh rate is switched?
	bool switched;
	/// display is blackened
	bool blackened;
	/// afrd version major, minor, micro
	uint8_t ver_major, ver_minor, ver_micro;
	/// afrd build date (zero-terminated)
	char bdate [24];
	/// current display refresh rate
	uint32_t current_hz;
	/// original display refresh rate
	uint32_t original_hz;
	/// afrd version suffix
	char ver_sfx [8];
	/// a copy of crc32 from first field
	uint32_t crc32_copy;
} __attribute__((packed)) afrd_shmem_t;

// afrd statistics
extern afrd_shmem_t g_afrd_stats;

// initialize shared-memory stats
extern bool shmem_init (bool read);
// finalize the shared memory object
extern void shmem_fini ();
// emergency close
extern void shmem_emerg ();
// update shared memory stats from g_afrd_stats
extern void shmem_update ();
// update g_afrd_stats from shared memory (in read mode)
extern bool shmem_read ();

// Initialize the unix domain socket for afrd API
extern bool apisock_init ();
// Finalize the unix domain socket for afrd API
extern void apisock_fini ();
// Fill the poll structure with socket handles and return their amount
extern int apisock_prep_poll (struct pollfd *pfd, int pfd_count);
// Handle socket events
extern void apisock_handle (struct pollfd *pfd, int pfd_count);

// afrd API: next video starting in <1.0 sec will use this frame rate
extern void afrd_frame_rate_hint (int hz);
// afrd API: set display refresh rate
extern void afrd_refresh_rate (int hz);
// afrd API: reload configuration file
extern void afrd_reconf ();
// afrd API: override color space
extern void afrd_override_colorspace (char **cs);

#endif /* __AFRD_H__ */
