# Universal Hotkeyer

A cross-platform context-aware hot keyer. It allows to:
- Redefine keyboard shortcuts system-wide or per application.
- Manage all your keyboard shortcuts in a single configuration file.
- Change shortcuts for similar actions in different applications at once.
- Share configuration files between multiple systems (GNU/Linux, Windows)
- Execute shell commands on any shortcut

This is a fork of [houmain's keymapper](https://github.com/houmaster/keymapper). It extends the functionality of a keyboard remapper to allow binding shell commands on your keys as well as fixes some bugs of the original project.

## Configuration

Unless overridden, using the command line argument ```-c```, the configuration is read from:
  * on Linux: ```$HOME/.config/hotkeyer.conf```
  * on Windows: ```hotkeyer.conf``` in the working directory.

Keynames: https://keycode.info/

*Simple hotkeyer.conf:*
```bash
O >>                  # Supress key
O >> /firefox         # Bind key to command
A >> B                # Remap a single key
C D >> D C            # Remap sequence (Pressed successively)
(X Y) >> (Y X)        # Remap sequence (Pressed simultaneously)
Control{Q} >> Alt{F4} # Remap sequence (Hold, press)
```

*Exculisive key hotkeyer.conf:*
```bash
# Triggers when the key is released and no other keys were pressed

# To achive Windows-like start menu behaviour:
~Meta >> /~/.scripts/launcher.sh
# While other combinations do not trigger the script above
Meta{Q} >> AltLeft{F4}
```


*Context hotkeyer.conf:*
```bash
# Context syntax: [system="..." title="..." class="..."]
(Shift Control){T} >> terminal

[system="Linux"]
terminal >> /cd ~; st

[system="Windows"]
terminal >> /powershell -NoExit -Command "cd ~"
```

*Any-key hotkeyer.conf:*
```bash
# Special 'Any' key to match all the keys or current stroke

# Keep Control-A but map A to B
Control{Any} >> Any
A >> B
```

*Virtual keys hotkeyer.conf:*
```bash
# Optional alias for a virtual key
Boss = Virtual1

# Virtual1/Boss is toggled whenever ScrollLock is pressed
ScrollLock >> Boss

# Map A to B when Virtual1/Boss is down
Boss{A} >> B

# Map E to F when Virtual1/Boss is NOT down
!Boss E >> F
```

*Output on key release hotkeyer.conf:*
```bash
# Send "cmd" after the Windows run dialog appeared
Win{C} >> Win{R} ^ C M D Enter

# Prevent key repeat
A >> B^

# Output B when A is released
A >> ^B
```

For more detailed instructions check out: https://github.com/houmaster/keymapper#configuration

## Installation

### Linux
- Install [AUR package](https://aur.archlinux.org/packages/hotkeyer-git) (for arch based) or download the [latest binaries](https://github.com/Azarattum/UniversalHotkeyer/releases/latest)
- Create [configuration](#configuration)
- Enable & start the service
```sh
systemctl enable hotkeyer
systemctl start hotkeyer
```
- If your environment supports [XDG Autostart](https://wiki.archlinux.org/index.php/XDG_Autostart), just relogin. Otherwise, manually start:
```sh
./hotkeyer
```

### Windows
- Download the [latest release](https://github.com/Azarattum/UniversalHotkeyer/releases/latest) for Windows
- Put [configuration](#configuration) in the binary's directory
- Run the executable
- Optional: run as administrator/use [*interception.dll*](https://github.com/oblitum/Interception/) for complete compatibility
- Optional: add the program to autostart

## Arguments
| Long            | Short     | Description                                                        |
| --------------- | --------- | ------------------------------------------------------------------ |
| --config <path> | -c <path> | Specify configuration file                                         |
| --update        | -u        | Reload configuration file when it changes                          |
| --verbose       | -v        | Enable verbose output                                              |
| --interception  | -i        | Use [*interception.dll*](https://github.com/oblitum/Interception/) |

## Building

Only a C++17 conforming compiler is required. A script for the
[CMake](https://cmake.org) build system is provided.
