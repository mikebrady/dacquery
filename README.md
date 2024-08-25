#### NAME AND DESCRIPTION
Dacquery is a command-line utility that scans a Linux system's [ALSA](https://www.alsa-project.org/) audio cards for Digital to Analog Converters -- "DACs" -- and lists the combinations of rates, sample formats and channels
(with channel maps if available) they accept. (Interleaved audio is assumed.)

Dacquery only probes interfaces that begin with `hw:`, `hdmi:` or `iec958:`.

Dacquery also lists the mixers  attached to the cards, listing their ranges, decibel-denominated ranges if provided and, if so, whether they accept a special decibel "volume" that causes them to mute.

The name on the command line is `dacquery`.

**Note:** Dacquery is available as a Docker image on the [Docker Hub](https://hub.docker.com/r/mikebrady/dacquery).

#### EXAMPLE

```
$ dacquery
  --- Alsa Version: 1.2.12.
  --- Sound Cards: 1.
  --- Card 0:
        --- Name: "HD-Audio Generic".
        --- Mixers:
               --------------------------------------------------------------------------------------------------------
              |  Name                              |  Index  |     Min  |     Max  |  Mute dB  |   Min dB  |   Max dB  |
               --------------------------------------------------------------------------------------------------------
              |  Master                            |      0  |       0  |      64  |       No  |   -64.00  |     0.00  |
              |  PCM                               |      0  |       0  |     255  |       No  |   -51.00  |     0.00  |
              |  Front                             |      0  |       0  |      64  |       No  |   -64.00  |     0.00  |
              |  Surround                          |      0  |       0  |      64  |       No  |   -64.00  |     0.00  |
              |  Center                            |      0  |       0  |      64  |       No  |   -64.00  |     0.00  |
              |  LFE                               |      0  |       0  |      64  |       No  |   -64.00  |     0.00  |
              |  Side                              |      0  |       0  |      64  |       No  |   -64.00  |     0.00  |
              |  Mic Boost                         |      0  |       0  |       3  |       No  |     0.00  |    30.00  |
               --------------------------------------------------------------------------------------------------------
        --- Interfaces and Supported Formats:
              >>> Interface "hw:Generic":
                  The interface listed above supports rate, format and channel combinations from the following table:
                       -------------------------------------------------------------------------------------------------------------
                      |    Rate |              Format |  Channels | Channel Map                                                     |
                       -------------------------------------------------------------------------------------------------------------
                      |   44100 |              S16_LE |         2 | FL FR                                                           |
                      |   48000 |              S32_LE |         4 | FL FR RL RR                                                     |
                      |   96000 |                     |         8 | FL FR RL RR FC LFE SL SR                                        |
                      |  192000 |                     |           |                                                                 |
                       -------------------------------------------------------------------------------------------------------------
```
#### OPTIONS
`-e`Display some extra information, including information about devices, sub-devices and interfaces. The start of the example above would become:
```
  --- Card 0:
        --- CTL name: "hw:CARD=Generic".
        --- Name: "HD-Audio Generic".
        --- Long name: "HD-Audio Generic at 0x3d400000 irq 72".
        --- Devices: 1.
              --- Device 0:
                    --- Name: "Generic Analog".
                    --- ID: "Generic Analog".
                    --- Subdevices: 1.
                          --- Subdevice: 0:
                                --- Name: "subdevice #0".
                                --- Interfaces:
                                      >>> "hw:Generic"
        --- Mixers:
            ...
```

`-h`Display help information and quit.

`-V`Display version information and quit.

#### NOTES
Dacquery must have permission to access the ALSA sound system. It should complain if it does not.

For Dacquery to fully investigate a device, the device must be idle. If you can't free up a device, perhaps it is being used by a sound server such as PulseAudio or PipeWire.

To  test  a  HDMI  interface, it is usually necessary to have a HDMI device connected to it and enabled at the time your system boots up. Otherwise, the HDMI interface may be listed as uninitialised.

#### LIMITATION
Dacquery lists devices and mixers as they appear to programs and utilities running on the computer. Sometimes, however, these devices may not be useful or functional. For instance, they may not be physically hooked up to, say, output connections.

Another thing to note is that the same audio hardware can appear in two or more different interfaces.

#### SEE ALSO
`aplay(1)`, `amixer(1)`, `alsamixer(1)`, `speaker-test(1)`.

#### CREDITS
Mike Brady (https://github.com/mikebrady) wrote Dacquery to help with Shairport Sync.
