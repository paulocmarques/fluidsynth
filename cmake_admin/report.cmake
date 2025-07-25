include( SCMRevision )

set ( AUDIO_MIDI_REPORT "\n" )

if ( ALSA_SUPPORT )
    set ( AUDIO_MIDI_REPORT "${AUDIO_MIDI_REPORT}  ALSA:                  yes\n" )
else ( ALSA_SUPPORT ) 
    set ( AUDIO_MIDI_REPORT "${AUDIO_MIDI_REPORT}  ALSA:                  no\n" )
endif ( ALSA_SUPPORT )

if ( COREAUDIO_SUPPORT )
    set ( AUDIO_MIDI_REPORT "${AUDIO_MIDI_REPORT}  CoreAudio:             yes\n" )
else ( COREAUDIO_SUPPORT )
    set ( AUDIO_MIDI_REPORT "${AUDIO_MIDI_REPORT}  CoreAudio:             no\n" )
endif ( COREAUDIO_SUPPORT )

if ( COREMIDI_SUPPORT )
    set ( AUDIO_MIDI_REPORT "${AUDIO_MIDI_REPORT}  CoreMIDI:              yes\n" )
else ( COREMIDI_SUPPORT )
    set ( AUDIO_MIDI_REPORT "${AUDIO_MIDI_REPORT}  CoreMIDI:              no\n" )
endif ( COREMIDI_SUPPORT )

if ( DSOUND_SUPPORT )
    set ( AUDIO_MIDI_REPORT "${AUDIO_MIDI_REPORT}  DSound:                yes\n" )
else ( DSOUND_SUPPORT )
    set ( AUDIO_MIDI_REPORT "${AUDIO_MIDI_REPORT}  DSound:                no\n" )
endif ( DSOUND_SUPPORT )

if ( JACK_SUPPORT )
    set ( AUDIO_MIDI_REPORT "${AUDIO_MIDI_REPORT}  JACK:                  yes\n" )
else ( JACK_SUPPORT )
    set ( AUDIO_MIDI_REPORT "${AUDIO_MIDI_REPORT}  JACK:                  no\n" )
endif ( JACK_SUPPORT )

if ( MIDISHARE_SUPPORT )
    set ( AUDIO_MIDI_REPORT "${AUDIO_MIDI_REPORT}  MidiShare:             yes\n" )
else ( MIDISHARE_SUPPORT )
    set ( AUDIO_MIDI_REPORT "${AUDIO_MIDI_REPORT}  MidiShare:             no\n" )
endif ( MIDISHARE_SUPPORT )

if ( OBOE_SUPPORT )
    set ( AUDIO_MIDI_REPORT "${AUDIO_MIDI_REPORT}  Oboe:                  yes\n" )
else ( OBOE_SUPPORT )
    set ( AUDIO_MIDI_REPORT "${AUDIO_MIDI_REPORT}  Oboe:                  no\n" )
endif ( OBOE_SUPPORT )

if ( OPENSLES_SUPPORT )
    set ( AUDIO_MIDI_REPORT "${AUDIO_MIDI_REPORT}  OpenSLES:              yes\n" )
else ( OPENSLES_SUPPORT )
    set ( AUDIO_MIDI_REPORT "${AUDIO_MIDI_REPORT}  OpenSLES:              no\n" )
endif ( OPENSLES_SUPPORT )

if ( DART_SUPPORT )
    set ( AUDIO_MIDI_REPORT "${AUDIO_MIDI_REPORT}  OS/2 DART:             yes\n" )
else ( DART_SUPPORT )
    set ( AUDIO_MIDI_REPORT "${AUDIO_MIDI_REPORT}  OS/2 DART:             no\n" )
endif ( DART_SUPPORT )

if ( KAI_SUPPORT )
    set ( AUDIO_MIDI_REPORT "${AUDIO_MIDI_REPORT}  OS/2 KAI:              yes\n" )
else ( KAI_SUPPORT )
    set ( AUDIO_MIDI_REPORT "${AUDIO_MIDI_REPORT}  OS/2 KAI:              no\n" )
endif ( KAI_SUPPORT )

if ( OSS_SUPPORT )
    set ( AUDIO_MIDI_REPORT "${AUDIO_MIDI_REPORT}  OSS:                   yes\n" )
else ( OSS_SUPPORT )
    set ( AUDIO_MIDI_REPORT "${AUDIO_MIDI_REPORT}  OSS:                   no\n" )
endif ( OSS_SUPPORT )

if ( PIPEWIRE_SUPPORT )
    set ( AUDIO_MIDI_REPORT "${AUDIO_MIDI_REPORT}  PipeWire:              yes\n" )
else ( PIPEWIRE_SUPPORT )
    set ( AUDIO_MIDI_REPORT "${AUDIO_MIDI_REPORT}  PipeWire:              no\n" )
endif ( PIPEWIRE_SUPPORT )

if ( PORTAUDIO_SUPPORT )
    set ( AUDIO_MIDI_REPORT "${AUDIO_MIDI_REPORT}  PortAudio:             yes\n" )
else ( PORTAUDIO_SUPPORT )
    set ( AUDIO_MIDI_REPORT "${AUDIO_MIDI_REPORT}  PortAudio:             no\n" )
endif ( PORTAUDIO_SUPPORT )

if ( PULSE_SUPPORT )
    set ( AUDIO_MIDI_REPORT "${AUDIO_MIDI_REPORT}  PulseAudio:            yes\n" )
else ( PULSE_SUPPORT ) 
    set ( AUDIO_MIDI_REPORT "${AUDIO_MIDI_REPORT}  PulseAudio:            no\n" )
endif ( PULSE_SUPPORT )

if ( SDL3_SUPPORT )
    set ( AUDIO_MIDI_REPORT "${AUDIO_MIDI_REPORT}  SDL3:                  yes\n" )
else ( SDL3_SUPPORT )
    set ( AUDIO_MIDI_REPORT "${AUDIO_MIDI_REPORT}  SDL3:                  no\n" )
endif ( SDL3_SUPPORT )

if ( WASAPI_SUPPORT )
    set ( AUDIO_MIDI_REPORT "${AUDIO_MIDI_REPORT}  WASAPI:                yes\n" )
else ( WASAPI_SUPPORT )
    set ( AUDIO_MIDI_REPORT "${AUDIO_MIDI_REPORT}  WASAPI:                no\n" )
endif ( WASAPI_SUPPORT )

if ( WAVEOUT_SUPPORT )
    set ( AUDIO_MIDI_REPORT "${AUDIO_MIDI_REPORT}  WaveOut:               yes\n" )
else ( WAVEOUT_SUPPORT )
    set ( AUDIO_MIDI_REPORT "${AUDIO_MIDI_REPORT}  WaveOut:               no\n" )
endif ( WAVEOUT_SUPPORT )

if ( WINMIDI_SUPPORT )
    set ( AUDIO_MIDI_REPORT "${AUDIO_MIDI_REPORT}  WinMidi:               yes\n" )
else ( WINMIDI_SUPPORT )
    set ( AUDIO_MIDI_REPORT "${AUDIO_MIDI_REPORT}  WinMidi:               no\n" )
endif ( WINMIDI_SUPPORT )


set ( INPUTS_REPORT "\n" )

set ( INPUTS_REPORT "${INPUTS_REPORT}Support for SF3 files:   " )
if ( LIBSNDFILE_HASVORBIS )
    set ( INPUTS_REPORT "${INPUTS_REPORT}yes\n" )
elseif ( NOT LIBSNDFILE_SUPPORT )
    set ( INPUTS_REPORT "${INPUTS_REPORT}no (libsndfile not found)\n" )
elseif ( NOT LIBSNDFILE_HASVORBIS )
    set ( INPUTS_REPORT "${INPUTS_REPORT}no (libsndfile has no ogg vorbis support)\n" )
endif ( LIBSNDFILE_HASVORBIS )


set ( INPUTS_REPORT "${INPUTS_REPORT}Support for DLS files:   " )
if ( LIBINSTPATCH_SUPPORT )
    set ( INPUTS_REPORT "${INPUTS_REPORT}yes\n" )
else ( LIBINSTPATCH_SUPPORT )
    set ( INPUTS_REPORT "${INPUTS_REPORT}no (libinstpatch not found)\n" )
endif ( LIBINSTPATCH_SUPPORT )


set ( RENDERING_REPORT "\n" )

if ( AUFILE_SUPPORT )
    set ( RENDERING_REPORT "${RENDERING_REPORT}Audio to file rendering: yes\n" )
else ( AUFILE_SUPPORT )
    set ( RENDERING_REPORT "${RENDERING_REPORT}Audio to file rendering: no\n" )
endif ( AUFILE_SUPPORT )

if ( LIBSNDFILE_SUPPORT )
    set ( RENDERING_REPORT "${RENDERING_REPORT}  libsndfile:            yes\n" )
else ( LIBSNDFILE_SUPPORT )
    set ( RENDERING_REPORT "${RENDERING_REPORT}  libsndfile:            no (RAW PCM rendering only)\n" )
endif ( LIBSNDFILE_SUPPORT )


set ( MISC_REPORT "\nMiscellaneous support:\n" )

if ( DBUS_SUPPORT )
  set ( MISC_REPORT "${MISC_REPORT}  D-Bus:                 yes\n" )
else ( DBUS_SUPPORT )
  set ( MISC_REPORT "${MISC_REPORT}  D-Bus:                 no\n" )
endif ( DBUS_SUPPORT )

if ( LADSPA_SUPPORT )
  set ( MISC_REPORT "${MISC_REPORT}  LADSPA support:        yes\n" )
else ( LADSPA_SUPPORT )
  set ( MISC_REPORT "${MISC_REPORT}  LADSPA support:        no\n" )
endif ( LADSPA_SUPPORT )

if ( NETWORK_SUPPORT )
  set ( MISC_REPORT "${MISC_REPORT}  NETWORK Support:       yes\n" )
else ( NETWORK_SUPPORT )
  set ( MISC_REPORT "${MISC_REPORT}  NETWORK Support:       no\n" )
endif ( NETWORK_SUPPORT )

if ( IPV6_SUPPORT )
  set ( MISC_REPORT "${MISC_REPORT}    IPV6 Support:        yes\n" )
else ( IPV6_SUPPORT )
  set ( MISC_REPORT "${MISC_REPORT}    IPV6 Support:        no\n" )
endif ( IPV6_SUPPORT )

if ( READLINE_SUPPORT )
  set ( MISC_REPORT "${MISC_REPORT}  Readline:              yes (NOTE: GPL library)\n" )
else ( READLINE_SUPPORT )
  set ( MISC_REPORT "${MISC_REPORT}  Readline:              no\n" )
endif ( READLINE_SUPPORT )

if ( SYSTEMD_SUPPORT )
  set ( MISC_REPORT "${MISC_REPORT}  systemd:               yes\n" )
else ( SYSTEMD_SUPPORT )
  set ( MISC_REPORT "${MISC_REPORT}  systemd:               no\n" )
endif ( SYSTEMD_SUPPORT )

if ( HAVE_GETOPT_H )
  set ( MISC_REPORT "${MISC_REPORT}  getopt:                yes\n" )
else ( HAVE_GETOPT_H )
  set ( MISC_REPORT "${MISC_REPORT}  getopt:                no\n" )
endif ( HAVE_GETOPT_H )

if ( WIN32 OR CYGWIN )
    set ( WINDOWS_REPORT "\nWindows specific info:\n" )
    if ( windows-version )
        set ( WINDOWS_REPORT "${WINDOWS_REPORT}  target version:        ${windows-version}\n" )
    endif ( windows-version )
    if ( enable-unicode )
      set ( WINDOWS_REPORT "${WINDOWS_REPORT}  unicode support:       yes\n" )
    else ( enable-unicode )
      set ( WINDOWS_REPORT "${WINDOWS_REPORT}  unicode support:       no\n" )
    endif ( enable-unicode )
else ( WIN32 OR CYGWIN )
    set ( WINDOWS_REPORT "")
endif ( WIN32 OR CYGWIN )

set ( DEVEL_REPORT "\nDeveloper nerds info:\n" )

set ( DEVEL_REPORT "${DEVEL_REPORT}  OS abstraction:        ${osal}\n" )

if ( WITH_FLOAT )
  set ( DEVEL_REPORT "${DEVEL_REPORT}  Samples type:          float\n" )
else ( WITH_FLOAT )
  set ( DEVEL_REPORT "${DEVEL_REPORT}  Samples type:          double\n" )
endif ( WITH_FLOAT )

if ( ENABLE_MIXER_THREADS )
  set ( DEVEL_REPORT "${DEVEL_REPORT}  Multithread rendering: yes\n" )
else ( ENABLE_MIXER_THREADS )
  set ( DEVEL_REPORT "${DEVEL_REPORT}  Multithread rendering: no\n" )
endif ( ENABLE_MIXER_THREADS )

if ( HAVE_OPENMP )
  set ( DEVEL_REPORT "${DEVEL_REPORT}  OpenMP 4.0:            yes\n" )
else ( HAVE_OPENMP )
  set ( DEVEL_REPORT "${DEVEL_REPORT}  OpenMP 4.0:            no\n" )
endif ( HAVE_OPENMP )

if ( WITH_PROFILING )
  set ( DEVEL_REPORT "${DEVEL_REPORT}  Profiling:             yes\n" )
else ( WITH_PROFILING )
  set ( DEVEL_REPORT "${DEVEL_REPORT}  Profiling:             no\n" )
endif ( WITH_PROFILING )

if ( ENABLE_DEBUG )
  set ( DEVEL_REPORT "${DEVEL_REPORT}  Debug Build:           yes\n" )
else ( ENABLE_DEBUG )
  set ( DEVEL_REPORT "${DEVEL_REPORT}  Debug Build:           no\n" )
endif ( ENABLE_DEBUG )

if ( ENABLE_TRAPONFPE )
  set ( DEVEL_REPORT "${DEVEL_REPORT}  Trap on FPE (debug):   yes\n" )
else ( ENABLE_TRAPONFPE )
  set ( DEVEL_REPORT "${DEVEL_REPORT}  Trap on FPE (debug):   no\n" )
endif ( ENABLE_TRAPONFPE )

if ( ENABLE_FPECHECK )
  set ( DEVEL_REPORT "${DEVEL_REPORT}  Check FPE (debug):     yes\n" )
else ( ENABLE_FPECHECK )
  set ( DEVEL_REPORT "${DEVEL_REPORT}  Check FPE (debug):     no\n" )
endif ( ENABLE_FPECHECK )

if ( ENABLE_UBSAN )
  set ( DEVEL_REPORT "${DEVEL_REPORT}  UBSan (debug):         yes\n" )
else ( ENABLE_UBSAN )
  set ( DEVEL_REPORT "${DEVEL_REPORT}  UBSan (debug):         no\n" )
endif ( ENABLE_UBSAN )

if ( ENABLE_COVERAGE )
  set ( DEVEL_REPORT "${DEVEL_REPORT}  Coverage:              yes\n" )
else ( ENABLE_COVERAGE )
  set ( DEVEL_REPORT "${DEVEL_REPORT}  Coverage:              no\n" )
endif ( ENABLE_COVERAGE )

message( STATUS 
        "\n**************************************************************\n"
        "Build Summary:\n"
        "FluidSynth Version:    " ${FLUIDSYNTH_VERSION} "\n"
        "Library version:       " ${LIB_VERSION_INFO} "\n"
        "Git revision:          " ${FluidSynth_WC_REVISION} "\n"
        "Build type:            " ${CMAKE_BUILD_TYPE} "\n"
        "Install Prefix:        " ${CMAKE_INSTALL_PREFIX} "\n"
        "\n"
        "Audio / MIDI driver support:"
        ${AUDIO_MIDI_REPORT}
        ${INPUTS_REPORT}
        ${RENDERING_REPORT}
        ${MISC_REPORT}
        ${WINDOWS_REPORT}
        ${DEVEL_REPORT}
         )

message ( "**************************************************************\n\n" )
