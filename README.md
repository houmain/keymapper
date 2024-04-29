
keymapper
=========
<p>
<a href="https://github.com/houmain/keymapper/actions/workflows/build.yml">
<img alt="Build" src="https://github.com/houmain/keymapper/actions/workflows/build.yml/badge.svg"/></a>
<a href="https://github.com/houmain/keymapper/issues">
<img alt="Issues" src="https://img.shields.io/github/issues-raw/houmain/keymapper.svg"/></a>

<a href="#configuration">Configuration</a> |
<a href="#example-configuration">Example</a> |
<a href="#functional-principle">Functional principle</a> |
<a href="#installation">Installation</a> |
<a href="#building">Building</a> |
<a href="https://github.com/houmain/keymapper/releases">Changelog</a>
</p>

A cross-platform context-aware key remapper. It allows to:

* Redefine your keyboard layout and shortcuts systemwide or per application.
* Manage all your keyboard shortcuts in a single configuration file.
* Change shortcuts for similar actions in different applications at once.
* Share configuration files between multiple systems (GNU/Linux, Windows, MacOS).
* Specify the output as [characters](#character-typing) instead of the keys required to type them.
* Bind keyboard shortcuts to [launch applications](#application-launching).
* Control the state from external applications using [keymapperctl](#keymapperctl).
* Use [mouse buttons](#key-names) in your mappings.

Configuration
-------------

Configuration files are easily written by hand and mostly consist of lines with [input expressions](#input-expressions) and corresponding [output expressions](#output-expressions) separated by `>>`:

```bash
# comments start with # or ; and continue until the end of a line
CapsLock >> Backspace
Z >> Y
Y >> Z
Control{Q} >> Alt{F4}
```

Unless overridden using the command line argument `-c`, the configuration is read from `keymapper.conf`, which is looked for in the common places and in the working directory:
  * on Linux and MacOS in `$HOME/.config/` and `/etc/`.
  * on Windows in the user's profile, `AppData\Local` and `AppData\Roaming` folders.

The command line argument `-u` causes the configuration to be automatically reloaded whenever the configuration file changes.

:warning: **In case of emergency:** You can always press the special key combination <kbd>Shift</kbd>+<kbd>Escape</kbd>+<kbd>K</kbd> to terminate `keymapperd`.

### Key names

The keys are named after their scan codes and are not affected by the present keyboard layout.
The names have been chosen to match on what the [web browsers](https://developer.mozilla.org/en-US/docs/Web/API/KeyboardEvent/code/code_values) have agreed upon, so this [handy website](http://keycode.info/) can be used to get a key's name.
For convenience the letter and digits keys are also named `A` to `Z` and `0` to `9`. The logical keys `Shift`, `Control` and `Meta` are also defined (each matches the left and right modifier keys). There are also [virtual keys](#virtual-keys) for state switching, an [Any key](#any-key) and a [No key](#no-key).

The mouse buttons are named: `ButtonLeft`, `ButtonRight`, `ButtonMiddle`, `ButtonBack` and `ButtonForward`.

It is also possible to directly provide the scan code instead of the key name in decimal or hex notation (e.g. `159`, `0x9F`).

:warning: Beware that the configuration file is **case sensitive**.

### Input expressions

Input expressions consist of one or more key names separated by spaces or parenthesis, which give them different meaning:

  * `A B` means that keys have to be pressed successively (released in any order).
  * `(A B)` means that keys have to be pressed simultaneously in any order.
  * `A{B}` means that a key has to be hold while another is pressed.
  * `!A` means that a key must not be pressed.
  * Groups and modifiers can also be nested like `A{B{C}}` or `(A B){C}`.

### Output expressions

The output expression format is analogous to the input expression format:

  * `A B` means that keys are pressed successively.
  * `(A B)` means that both keys are pressed simultaneously.
  * `A{B}` means that a key is hold while another is pressed.
  * `!A` means that the (potentially pressed) key should be released before the rest of the expression is applied.
  * `^` splits the output in two parts, one which is applied when the input key is pressed and one when the [key is released](#output-on-key-release).
  * Strings enclosed in single or double quotes specify [characters to type](#character-typing).
  * `$()` can be used for [launching applications](#application-launching).
  * An empty expression can be used to suppress any output.

### Order of mappings

Mappings are always applied in consecutive order, therefore their order is of importance. While the following outputs `A` as soon as `Meta` is pressed:

```bash
Meta    >> A
Meta{X} >> B
```

The other way round, nothing is output when `Meta` is pressed alone. Depending on whether an `X` follows, either `B` or `A` is output:

```bash
Meta{X} >> B
Meta    >> A
```

:warning: You may want to start your configuration with mappings, which ensure that the common mouse-modifiers are never hold back:

```bash
Shift   >> Shift
Control >> Control
AltLeft >> AltLeft
```

For a detailed description of how the mapping is applied, see the [Functional principle](#functional-principle) section.

### Context awareness

Context blocks allow to enable mappings only in specific contexts. A context can be defined by `system`, the focused window `title`, window `class`, process `path` or the input `device` an event originates from.\
A block continues until the next block (respectively the end of the file). The block which applies in all contexts can be reopened using `default`. e.g.:

```bash
[default]

[title = "Visual Studio"]

[system = "Linux" class != "qtcreator"] # '!=' inverses a condition

[system = "Windows", path = "notepad.exe"] # comma is optional

[device = "Some Device Name"] # consecutive blocks share mappings
[device = "Other Device"]
```

:warning: The `device` filter on Windows requires the installation of a [virtual device driver](#virtual-device-driver). The process `path` may not be available on Wayland and for processes with higher privileges. The window `title` is not available on MacOS.

Class and device filters match contexts with the _exact_ same string, others match contexts _containing_ the string.
For finer control [regular expressions](https://en.wikipedia.org/wiki/Regular_expression) can be used. These have to be delimited with slashes. Optionally `i` can be appended to make the comparison case insensitive:

```javascript
[title = /Visual Studio Code|Code OSS/i]
```

Additionally a `modifier` filter allows to activate blocks depending on the state of one or more keys:

```bash
# active when Virtual1 is down and Virtual2 is not
[modifier = "Virtual1 !Virtual2"]
```

### Abstract commands

To simplify mapping of one input expression to different output expressions, it can be mapped to an abstract command first. The command name can be chosen arbitrarily but must not be a key name. The configuration is case sensitive and all key names start with a capital letter, so it is advisable to begin command names with a lowercase letter:

```bash
Control{B} >> build
```

Subsequently this command can be mapped to one output expression per context. The last active mapping overrides the previous ones:

```bash
build >> Control{B}

[title="Visual Studio"]
build >> (Shift Control){B}
```

### Output on key release

When an output expression contains `^`, it is only applied up to this point, when the input key is pressed. The part after the `^` is not applied until the input is released. Both parts can be empty:

```bash
# send "cmd" after the Windows run dialog appeared
Meta{C} >> Meta{R} ^ C M D Enter

# prevent key repeat
A >> B^

# output B when A is released
A >> ^B
```

### Virtual keys

`Virtual0` to `Virtual255` are virtual keys, which can be used as state switches. They are toggled when used in output expressions and can be used as modifiers in input expressions:

```bash
# Virtual1 is toggled whenever ScrollLock is pressed
ScrollLock >> Virtual1

# map A to B when Virtual1 is down
Virtual1{A} >> B

# map E to F when Virtual1 is NOT down
!Virtual1 E >> F

# keep G hold as long as Virtual1 is down
Virtual1 >> G
```

`ContextActive` exists separately for each context and is toggled when the context becomes active/inactive:

```bash
# toggle Virtual1 when entering and when leaving context
[title="Firefox"]
ContextActive >> Virtual1 ^ Virtual1
```

### Any key

```Any``` can be used in input and output expressions.
In input expressions it matches any key and in output expressions it outputs the matched input.

```bash
# swap Control and Shift
Control{Any} >> Shift{Any}
Shift{Any} >> Control{Any}
```

To exclude an application from any mapping this can be added to the top of the configuration:

```bash
[title="Remote Desktop"]
Any >> Any

[default]
```

### No key

Input expressions can contain timeouts in milliseconds e.g. `500ms`, to specify a time in which no key is pressed:

```bash
# output Escape when CapsLock is hold for a while
CapsLock{500ms} >> Escape

# output Escape when Control is pressed and released quickly
Control{!250ms} >> Escape

# output C when B quickly follows an A
A !250ms B >> C
```

In output expressions it can be used to delay output or keep a key hold for a while. e.g:

```bash
A >> B 500ms C{1000ms}
```

### Character typing

Output expressions can contain string literals with characters to type. The typeable characters depend on your keyboard layout. e.g:

```bash
AltRight{A} >> '@'
Meta{A} K >> "Kind regards,\nDouglas Quaid"
```

:warning: The keyboard layout is evaluated when the configuration is loaded, switching is not yet supported.

### Key aliases

For convenience aliases for keys and even sequences can be defined. e.g.:

```bash
Win = Meta
Boss = Virtual1
Alt = AltLeft | AltRight
FindNext = Control{F3}
Proceed = Tab Tab Enter
Greet = "Hello"
```

Aliases can also be parameterized. The arguments are provided in square brackets and can be referenced by `$0`, `$1`... e.g.:
```
print = $(echo $0 $1 >> ~/keymapper.txt)
F1 >> print["pressed the key", F1]
```

### Application launching

`$()` can be used in output expressions to embed commands, which should be executed when it is triggered. e.g.:

```bash
Meta{C} >> $(C:\windows\system32\calc.exe) ^
Meta{W} >> $(exo-open --launch WebBrowser) ^

# on Windows console applications are revealed by prepending 'start'
Meta{C} >> $(start powershell) ^
```

:warning: You may want to append `^` to ensure that the command is not executed repeatedly as long as the input is kept hold.

### keymapperctl

The application `keymapperctl` allows to communicate with the running `keymapper` process.
It can be run arbitrarily often with one or more of the following arguments:
```
  --is-pressed <key>    sets the result code 0 when a virtual key is down.
  --is-released <key>   sets the result code 0 when a virtual key is up.
  --press <key>         presses a virtual key.
  --release <key>       releases a virtual key.
  --toggle <key>        toggles a virtual key.
  --wait-pressed <key>  waits until a virtual key is pressed.
  --wait-released <key> waits until a virtual key is released.
  --wait-toggled <key>  waits until a virtual key is toggled.
  --timeout <millisecs> sets a timeout for the following operation.
  --wait <millisecs>    unconditionally waits a given amount of time.
  --instance <id>       replaces another keymapperctl process with the same id.
  --restart             starts processing the first operation again.
  --stdout              writes the result code to stdout.
```

Example configuration
---------------------

The [author's personal configuration](keymapper.conf) may serve as an inspiration (which itself took some inspiration from [DreymaR's Big Bag Of Keyboard Tricks](https://dreymar.colemak.org/layers-extend.html)).

Functional principle
--------------------

For advanced application it is good to know how the mapping is applied:

  * All key strokes are intercepted and appended to a key sequence.
  * On every key stroke the key sequence is matched with all input expressions in consecutive order, until an expression matches or might match (when more strokes follow). Mappings in inactive contexts are skipped.
  * When the key sequence can no longer match any input expression (because more strokes followed), the longest exact match is looked for (by ignoring the last strokes). As long as still nothing can match, the first strokes are removed and forwarded as output.
  * When an input expression matches, the key sequence is cleared and the mapped expression is output.
  * Keys which already matched but are still physically pressed participate in expression matching as an optional prefix to the key sequence.

Installation
------------
The program is split into two parts:
* `keymapperd` is the service which needs to be given the permissions to grab the keyboard devices and inject keys.
* `keymapper` should be run as normal user in a graphical environment. It loads the configuration, informs the service about it and the active context and also executes mapped terminal commands.

For security and efficiency reasons, the communication between the two parts is kept as minimal as possible.

The command line argument `-v` can be passed to both processes to output verbose logging information to the console.

### Linux

Pre-built packages can be downloaded from the [latest release](https://github.com/houmain/keymapper/releases/latest) page. Arch Linux users can install an up to date build from the [AUR](https://aur.archlinux.org/packages/?K=keymapper).

After installation you can try it out by creating a [configuration](#configuration) file and starting it using:
```
sudo systemctl start keymapperd
keymapper
```

To install permanently, add `keymapper` to the desktop environment's auto-started applications and enable the `keymapperd` service:
```
sudo systemctl enable keymapperd
```

To make context awareness work under Wayland, the compositor has to inform `keymapper` about the focused window. For [wlroots-based](https://wiki.archlinux.org/title/Wayland#Compositors) compositors this works out of the box, other compositors need to send the information using the [D-Bus](https://freedesktop.org/wiki/Software/dbus/) interface. A [GNOME Shell extension](https://github.com/houmain/keymapper/tree/main/extra/share/gnome-shell/extensions/keymapper%40houmain.github.com) and a [KWin script](https://github.com/houmain/keymapper/tree/main/extra/share/kwin/scripts/keymapper) are provided doing this.

### MacOS

The MacOS build depends on version 3.1.0 of [Karabiner-Element's](https://karabiner-elements.pqrs.org) virtual device driver.
One can install it either directly from [Karabiner-DriverKit-VirtualHIDDevice](https://github.com/pqrs-org/Karabiner-DriverKit-VirtualHIDDevice/releases) or along with [Karabiner Elements](https://github.com/pqrs-org/Karabiner-Elements/releases) 14.13.0.

A [Homebrew](https://brew.sh) formula is provided for building and installing keymapper:
```
brew tap houmain/tap
brew install --HEAD keymapper
```

Finally `keymapperd` and `keymapper` can be added to the `launchd` daemons/agents by calling:
```
sudo keymapper-launchd add
```

### Windows
An installer and a portable build can be downloaded from the [latest release](https://github.com/houmain/keymapper/releases/latest) page.

The installer configures the Windows task scheduler to start `keymapper.exe` and `keymapperd.exe` at logon.

To use the portable build, simply create a [configuration](#configuration) file and start both `keymapper.exe` and `keymapperd.exe`. It is advisable to start `keymapperd.exe` with elevated privileges. Doing not so has a few limitations. Foremost the Windows key cannot be mapped reliably and applications which are running as administrator (like the task manager) resist any mapping.

#### Virtual device driver

The [device](#context-awareness) filter requires the installation of a virtual device driver. The only known freely available is [Interception](https://github.com/oblitum/Interception), which unfortunately has a [severe bug](https://github.com/oblitum/Interception/issues/25), that makes devices stop working after being dis-connected too often. Until this is fixed it not advisable to use it and it should only be installed when filtering by device is absolutely required. The `interception.dll` needs to be placed next to the `keymapperd.exe`.

Building
--------

A C++17 conforming compiler is required. A script for the
[CMake](https://cmake.org) build system is provided.

**Installing dependencies on Debian Linux and derivatives:**
```
sudo apt install build-essential git cmake libudev-dev libusb-1.0-0-dev libx11-dev libdbus-1-dev libwayland-dev libxkbcommon-dev libgtk-3-dev libappindicator3-dev
```

**Checking out the source:**
```
git clone https://github.com/houmain/keymapper
```

**Building:**
```
cd keymapper
cmake -B build
cmake --build build
```

**Testing:**

To try it out, simply create a [configuration](#configuration) file and start it using:

```
sudo build/keymapperd -v
```

and

```
build/keymapper -v
```


License
-------
It is released under the GNU GPLv3. It comes with absolutely no warranty. Please see `LICENSE` for license details.
