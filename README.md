# UniversalHotkeyer

A cross-platform context-aware hot keyer. It allows to:
- Redefine keyboard shortcuts system-wide or per application.
- Manage all your keyboard shortcuts in a single configuration file.
- Change shortcuts for similar actions in different applications at once.
- Share configuration files between multiple systems (GNU/Linux, Windows)
- Execute shell commands on any shortcut

This is a fork of [houmaster's keymapper](https://github.com/houmaster/keymapper). It extends the functionality of a keyboard remapper to allow binding shell commands on your keys as well as fixes some bugs of the original project.

## Configuration

Unless overridden, using the command line argument ```-c```, the configuration is read from:
  * on Linux: ```$HOME/.config/keymapper.conf```
  * on Windows: ```keymapper.conf``` in the working directory.

Keynames: http://keycode.info/

*Simple keymapper.conf:*
```bash
O >>                  # Supress key
O >> /firefox         # Bind key to command
A >> B                # Remap a single key
C D >> D C            # Remap sequence (Pressed successively)
(X Y) >> (Y X)        # Remap sequence (Pressed simultaneously)
Control{Q} >> Alt{F4} # Remap sequence (Hold, press)
```

*Context keymapper.conf:*
```bash
#Context syntax: [Window system="..." title="..." class="..."]
(Shift Control){T} >> terminal

[Window system="Linux"]
terminal >> /cd ~; st

[Window system="Windows"]
terminal >> /powershell -NoExit -Command "cd ~"
```


*Virtual keys keymapper.conf:*
```bash
# Optional alias for a virtual key
Boss = Virtual1

# Virtual1/Boss is toggled whenever ScrollLock is pressed
ScrollLock >> Boss

# map A to B when Virtual1/Boss is down
Boss{A} >> B

# map E to F when Virtual1/Boss is NOT down
!Boss E >> F
```

For more detailed instructions check out: https://github.com/houmaster/keymapper#configuration

## Building

Only a C++17 conforming compiler is required. A script for the
[CMake](https://cmake.org) build system is provided.