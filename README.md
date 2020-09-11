# midictl

A terminal-based MIDI control panel meant to be simple and fast in use.

<img src=ss/ss.png margin=auto></img>

## Using midictl
Basic use is very straightforward: `midictl <config file> -d <client ID> -p <port> -c <MIDI channel>`.
You can determine client ID and port number of the device you want by executing `aconnect -o`.
Configuration file format is described in the next section.

_Please keep in mind that `midictl` is in very early stage of its life. I literally just wrote it in the two past days. You may stumble upon weird bugs (if you do, please [open an issue](https://github.com/Jacajack/midictl/issues/new)). Some things may change in future releases, including key bindings and config file format._

Key bindings:
 - <kbd>Q</kbd> - Quit
 - <kbd>K</kbd>, <kbd>&#8593;</kbd> - Move up
 - <kbd>J</kbd>, <kbd>&#8595;</kbd> - Move down
 - <kbd>PgUp</kbd> - Move one page up
 - <kbd>PgDn</kbd> - Move one page down
 - <kbd>Home</kbd> - Jump to the beginning
 - <kbd>End</kbd> - Jump to the end
 - <kbd>H</kbd>, <kbd>&#8592;</kbd> - Value -= 1 
 - <kbd>L</kbd>, <kbd>&#8594;</kbd> - Value += 1
 - <kbd>Shift</kbd> + <kbd>H</kbd>, <kbd>Shift</kbd> + <kbd>Z</kbd> - Value -= 10 
 - <kbd>Shift</kbd> + <kbd>L</kbd>, <kbd>Shift</kbd> + <kbd>C</kbd> - Value += 10 
 - <kbd>I</kbd>, <kbd>Enter</kbd> - Enter value for the controller
 - <kbd>Z</kbd> - Min value
 - <kbd>X</kbd> - Center value
 - <kbd>C</kbd> - Max value
 - <kbd>T</kbd> - Transmit value to the MIDI device
 - <kbd>Shift</kbd> + <kbd>T</kbd> - Transmit all current values to the MIDI device
 - <kbd>R</kbd> - Default value
 - <kbd>Shift</kbd> + <kbd>R</kbd> - Reset all controllers to their defaults
 - <kbd>/</kbd> - Search for controller by name (leave empty to repeat search)
 - <kbd>[</kbd> - Move split to the left
 - <kbd>]</kbd> - Move split to the right
 - <kbd>=</kbd> - Hide/show left column

## Config file format
The config file format is meant to be as simple and friendly as possible. Each line in the file represents one MIDI controller, a heading or a horizontal rule.
 - Empty lines and those starting with `#` are ignored.
 - Lines starting with `---` represent horizontal rules. If the `---` is followed by text, it is placed over the horizontal rule and serves as heading.
 - Lines starting with numbers or square brackets are MIDI controllers. The number is the MIDI controller number. The number can be followed by optional square bracket pair containing metadata. Any text following the number or metadata block is treated as name of the controller.

It sounds much more complicated than it is:
```
--- Oscillator 1
10 [slider = 0] Waveform
11 [min = 32, max = 96] Detune
12 [def = 127] Volume

--- LFO
[cc = 13] Rate
14 Fade
```

Valid metadata parameters are:
 - `cc` - MIDI CC. Overrides the value specified outside the metadata block. As you can see, the controller number can be omitted as long as this parameter is set instead. In my opinion this syntax looks better actually.
 - `min` - Minimum value for the controller
 - `max` - Maximum value for the controller
 - `def` - Default value. 
 - `chan` - MIDI channel (overrides default MIDI channel)
 - `slider` - The slider is not displayed if set to 0
 - `update` - If non-zero, controller's default value is automatically transmitted when `midictl` starts.

#### Some wild development ideas
 - [ ] More colors!
 - [ ] Device access
 - [ ] `hide` metadata parameter
 - [ ] Loading config files on the fly
 - [ ] Multiple tabs
 - [ ] Configurable key bindings
 - [ ] Transmit program change messages etc.
 - [ ] 14-bit controller support (a big one...)

Pull requests are welcome!