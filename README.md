Auto Frame Rate Daemon (AFRD)
-----------------------------

This is a Linux daemon for AMLogic SoC based boxes to switch screen
framerate to most suitable during video play. The daemon uses kernel
uevent-based notifications, available in AMLogic 3.14 kernels from
summer 2017. Earlier kernels won't emit notifications at the start
and end of playing video, so the daemon won't work.

Unfortunately, this uevent is gone from kernel 4.9 (used for
Android 7 and 8), so afrd can't use frame rate notifications
on newer kernels. Instead, afrd will use video decoder notifications
that are emitted at the start and end of playback.

The daemon can be linked either with BioniC (for Android) or with
glibc (for plain Linux OSes).

Versions up to 0.1.* use kernel uevent notifications emmited by
AMLogic kernels starting from summer 2017, used in some Android 6.0
and 7.0 firmwares. Unfortunately, somewhere around start of 2018
AMLogic broke its own code and since then these uevents are not
generated (altough part of the respective code is still in kernel).
Here are a example FRAME_RATE_HINT uevent generated when a 29.976
fps movie is started:
```
change@/devices/virtual/tv/tv
    ACTION=change
    DEVPATH=/devices/virtual/tv/tv
    SUBSYSTEM=tv
    FRAME_RATE_HINT=3203
    MAJOR=254
    MINOR=0
    DEVNAME=tv
    SEQNUM=2787
```
and when the movie stops:
```
change@/devices/virtual/tv/tv
    ACTION=change
    DEVPATH=/devices/virtual/tv/tv
    SUBSYSTEM=tv
    FRAME_RATE_END_HINT
    MAJOR=254
    MINOR=0
    DEVNAME=tv
    SEQNUM=2788
```
Since version 0.2.0 afrd supports detecting movie framerate by
catching uevent's generated when a hardware decoder is started
or stopped. These events are present on both older and newer
kernels, thus afrd can work with almost every AMLogic kernel
out there:
```
add@/devices/vdec.25/amvdec_h264.0
    ACTION=add
    DEVPATH=/devices/vdec.25/amvdec_h264.0
    SUBSYSTEM=platform
    MODALIAS=platform:amvdec_h264
    SEQNUM=2786

remove@/devices/vdec.25/amvdec_h264.0
    ACTION=remove
    DEVPATH=/devices/vdec.25/amvdec_h264.0
    SUBSYSTEM=platform
    MODALIAS=platform:amvdec_h264
    SEQNUM=2789
```
These events do not contain the movie frame rate, thus in addition
to these events afrd has to query the video decoder driver, its status
is available from /sys/class/vdec/vdec_status:
```
vdec channel 0 statistics:
  device name : amvdec_h264
  frame width : 1920
 frame height : 1080
   frame rate : 24 fps
     bit rate : 856 kbps
       status : 63
    frame dur : 4000
   frame data : 19 KB
  frame count : 230
   drop count : 0
fra err count : 0
 hw err count : 0
   total data : 1197 KB
```
The "frame dur" field is used if not 0 (the actual frame rate is
96000/frame_dur), and "frame rate" field is used otherwise
(with 23 fps being 23.976, 29 being 29.970 and 59 being 59.94 fps).

Configuration file
------------------

AFRD uses a simple configuration file that can be used to customize
its behavior. The file contains either comment lines, starting with
the '#' character, or key=value pairs.

If a key contains several values, list elements are always separated
by white spaces.

Every 5 seconds afrd will check the last-modified timestamp on the
loaded config file. If config file is changed, afrd reloads it.

The following parameters are recognized by AFRD:

* *hdmi.dev*
    This keyword defines the sysfs directory with HDMI driver attributes.
    Usually has the value /sys/class/amhdmitx/amhdmitx0.

* *hdmi.state*
    This defines the sysfs file used to check if HDMI is enabled or not.
    Usually has the value /sys/class/switch/hdmi/state.

* *switch.delay.on*
* *switch.delay.off*
    The time to delay a mode switch when video starts (on) or stops (off).
    The 'on' delay is usually small, while 'off' should be relatively big
    to avoid unneeded mode switches at position change or when you're
    watching a series of videos with same framerate.

    Setting a parameter to zero will disable the respective operation.
    For example, if you want to completely disable restoration of the
    original refresh rate, set switch.delay.off to 0.

    The time is given in milliseconds.

* *switch.delay.retry*
    Sometimes video decoder doesn't report movie frame rate correctly.
    In these cases afrd will wait some time and try the query again.
    This keyword defines the time interval between these tries.

    The time is given in milliseconds.

* *switch.timeout*
    If movie frame rate cannot be safely determined for this period
    of time, afrd will give up trying.

* *switch.blackout*
    When movie starts after this period of time the screen will be
    disabled to minimize flicker. Don't set it too low as in some
    circumstances screen may be blackened when it shouldn't be.

* *switch.ignore*
    If time interval between a STOP/PLAY operation is less than given
    amount of milliseconds, afrd will not re-evaluate movie fps. This
    is useful to avoid suddent refresh rate switches due to wrong fps
    evaluation during movie rewind operations.

* *switch.hdmi*
    Number of milliseconds to delay HDMI hot-plug event handling.
    When HDMI is plugged out, afrd will clear the list of supported
    video modes, and when it is plugged in, afrd will query the list
    of supported modes (which is retrieved from TV EDID data), but it
    has to wait a little so that things settle down (till kernel driver
    does its job).

    Some strange TVs will simulate a HDMI off/on event pair on every
    refresh rate switch, this can be seen in afrd log. In this case
    this parameter should be either 0 (which means to ignore HDMI
    hotplug events), or larger than the "HDMI off" period.

* *mode.path*
    Points to sysfs file used to switch current video mode.
    This is usually /sys/class/display/mode.

* *mode.prefer.exact*
    If set is 1, AFRD will prefer exact match of video mode refresh rate
    to movie frame rate. If 0 (default setting), AFRD will prefer the
    video mode with highest refresh rate which is a exact multiple of
    movie frame rate.

    Default value is 0.

* *mode.use.fract*
    Choose how AFRD handles fractional frame rates (23.976, 29.97, 59.94 fps):

    * 0 - Use both fractional and integer refresh rates as appropiate
    * 1 - Prefer fractional refresh rates, even if movie says it uses integer
    * 2 - Prefer integer refresh rates, even if movie says it uses fractional

    This can be used to limit the number of mode switches, e.g. when your OS
    is set up to use 59.94Hz refresh rate (the default on AndroidTV boxes with
    Android 8+) to avoid a mode switch when watching 30 or 60Hz videos
    you can set this to 1.

* *mode.blacklist.rates*
    This option allows you to blacklist some refresh rates. For example,
    AMLogic kernels have a bug in the HDMI driver that sets the 29.976 mode
    wrong; using this refresh mode causes severe jittering on 23.976 movies.
    Thus, this refresh rate is blacklisted by default.

    This is a list of numbers separated by spaces.

* *mode.extra*
    Normally afrd takes the list of supported video modes from EDID info blob
    fetched via HDMI from your TV. Sometimes this info doesn't contain all the
    supported modes. You can put a list of additional supported video modes
    that aren't listed

* *vdec.sysfs*
    This points to sysfs directory containing the status of the video decoder.
    afrd will try to use attributes vdec_status, dump_vdec_blocks, dump_vdec_chunks.

* *uevent.filter.frhint*
* *uevent.filter.vdec*
* *uevent.filter.hdmi*
    These sets the filters for kernel uevents we handle:
    * *frhint* for the FRAMERATE_HINT uevent generated on some older and patched
        kernels when video decoder detects a change of framerate in played video
    * *vdec* for uevents generated by the video decoder when a hardware decoder
        is activated or deactivated
    * *hdmi* for uevents generated when we plug in or out the HDMI connector.

    Event filters use the following format:

    uevent.filter.XXX=&lt;keyword&gt;=&lt;regex&gt;[ &lt;keyword&gt;=&lt;regex&gt;...]

    The regular expressions are "extended regular expressions" and are matched
    against the whole attribute value. E.g. regular expression "plat" will not
    match the string "platform", while "platform" or "plat.*" will.

    Regular expressions cannot contain space characters, which are used to
    delimit attribute filters from each other. If you need a space, use the
    [[:space:]] regular expression.

* *frhint.vdec.blacklist*
    A list of video decoders to ignore FRAME_RATE_HINT events from. For example,
    the amvdec_h265 codec will always report 26 fps on every video in a
    FRAME_RATE_HINT event. By ignoring this FRAME_RATE_HINT event you give
    a chance to afrd to use other sources to determine the correct frame rate.


AFRd API
--------

AFRd has an API which can be used to cooperate with video player software in order
to get a better user experience. In particular, software may tell afrd in advance
what is the frame rate of the video, so that afrd doesn't have to guess it.

The API is available by sending UDP datagrams to address 127.0.0.1, port 50505.
The protocol is purely textual, each datagram may contain multiple commands,
each command on a separate line.

The format of each line is:

	<command> [<arguments> ...]

The following commands are available:

* *help*
    display a list of supported commands and their short description

* *frame_rate_hint* <fr>
    tell afrd the video starting in less than one second will use
    <fr>/1000 frames per second (e.g. 23976 = 23.976 fps).

* *refresh_rate* <rr>
    tell afrd to set display refresh rate as close to <rr>/1000 Hz
    as possible immediately, no arg to restore original rate.

* *color_space* <cs>
    override colorspace, empty arg to restore default behavior.
    The <cs> specificator may contain, separated by comma:
    * The color space itself (one of rgb, 444, 422, 420)
    * The color depth (8bit, 10bit, 12bit, 16bit)
    * The color range (limit or full)
    Color space takes effect on next refresh rate switch.

* *status*
    get current afrd status

* *reconf*
    tell afrd to reload configuration file as soon as possible

To test the API you may use the 'nc' tool that is part of busybox, which can be
easily installed if you didn't already. Example dialog with afrd, lines starting
with '>' are outgoing, others are incoming:

```
1|u211:/ # nc -u 127.0.0.1 50505
>help
frame_rate_hint <fr>
	tell afrd the video starting in <1.0 seconds will use <fr>/1000 frames per second (e.g. 23976 = 23.976 fps)
refresh_rate <rr>
	tell afrd to set display refresh rate as close to <rr>/1000 Hz as possible, no arg to restore original rate
color_space <cs>
	override colorspace, empty arg tor restore default behavior
status
	get current afrd status
reconf
	tell afrd to reload configuration file as soon as possible

>status
enabled:1
active:0
blackened:0
version:0.2.5
build:2019-03-31 21:17:38
current hz:50000
original hz:50000

>refresh_rate 59940

>status
enabled:1
active:1
blackened:0
version:0.2.5
build:2019-03-31 21:17:38
current hz:59941
original hz:50000

>refresh_rate

>status
enabled:1
active:0
blackened:0
version:0.2.5
build:2019-03-31 21:17:38
current hz:50000
original hz:50000

>color_space rgb,8bit,full
>refresh_rate 50000
```

Note how afrd remembers original refresh rate when you switch it for the first time,
and how it restores refresh rate when the command 'refresh_rate' without parameters
is issued.
