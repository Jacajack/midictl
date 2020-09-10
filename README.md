# midictl

A terminal-based MIDI control panel meant to be simple and fast in use.

<img src=ss/ss.png width=80% height=auto></img>

### Controller list file description
The controller file format accepted by `midictl` is very simple:
 - Empty lines and those starting with `#` are ignored
 - Lines starting with `---` represent horizontal rules. If the `---` is followed by text, it is placed over the horizontal rule and serves as heading.
 - Lines starting with numbers represet MIDI controllers. The number must be in range [0; 127]. The text following the number is used as controller name.

 An example configuration:
 ```
--- Oscillators
110 Detune 1
111 Waveform 1
112 Volume 1
---
120 Detune 2
121 Waveform 2
122 Volume 2
```

### Using midictl
`midictl` uses ALSA sequencer to send MIDI CC messages. It requires the user to specify destination device ID and port number. To list all output MIDI devices you can use `aconnect -o`:
```
client 14: 'Midi Through' [type=kernel]
    0 'Midi Through Port-0'
client 130: 'VMPK Input' [type=user,pid=497443]
    0 'in  
```

For example, to connect to `VMPK Input` (device 130, port 0) you would use `midictl -d 130 -p 0 ...`.

TODO
