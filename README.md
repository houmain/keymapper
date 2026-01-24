
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
<a href="#keymapperctl">keymapperctl</a> |
<a href="#installation">Installation</a> |
<a href="#building">Building</a> |
<a href="https://github.com/houmain/keymapper/releases">Changelog</a>
</p>

A cross-platform context-aware key remapper. It allows to:

* Redefine your keyboard layout and shortcuts systemwide or per application.
* Manage all your keyboard shortcuts in a single configuration file.
* Change shortcuts for similar actions in different applications at once.
* Share configuration files between multiple systems (GNU/Linux, Windows, MacOS).
* Specify input and output as [characters](#character-typing) instead of the keys required to type them.
* Bind keyboard shortcuts to [launch applications](#application-launching).
* Control the state from external applications using [keymapperctl](#keymapperctl).
* Use [mouse buttons and wheel](#key-names) in your mappings.

Configuration
-------------
Configuration files are easily written by hand and mostly consist of lines with [input expressions](#input-expressions) and corresponding [output expressions](#output-expressions) separated by `>>`:

```bash
# comments start with # and continue until the end of a line
CapsLock >> Backspace
Z >> Y
Y >> Z
Control{Q} >> Alt{F4}
```

Unless overridden using the command line argument `-c`, the configuration is read from `keymapper.conf`, which is looked for in the common places:
  * on all systems in `$XDG_CONFIG_HOME` and `$HOME/.config`,
  * on Linux and MacOS also in `/etc`,
  * on Windows also in the user's profile, `AppData\Local` and `AppData\Roaming` folders,

each with an optional `keymapper` subdirectory and finally in the working directory.

The command line argument `-u` causes the configuration to be automatically reloaded whenever the configuration file changes.

:warning: **In case of emergency:** You can always press the special key combination <kbd>Shift</kbd>+<kbd>Escape</kbd>+<kbd>K</kbd> to terminate `keymapperd`.

### Key names

The keys are named after their scan codes and are not affected by the present keyboard layout.
The names have been chosen to match on what the [web browsers](https://developer.mozilla.org/en-US/docs/Web/API/KeyboardEvent/code/code_values) have agreed upon, so this [handy website](http://keycode.info/) can be used to get a key's name.
For convenience the letter and digits keys are also named `A` to `Z` and `0` to `9`. The logical keys `Shift`, `Control`, `Alt` and `Meta` are also defined (each matches the left and right modifier keys). There are also [virtual keys](#virtual-keys) for state switching and an [Any key](#any-key).

The mouse buttons are named `ButtonLeft`, `ButtonRight`, `ButtonMiddle`, `ButtonBack` and `ButtonForward`, the wheel is named `WheelUp`, `WheelDown`, `WheelLeft` and `WheelRight`.

It is also possible to directly provide the scan code instead of the key name in decimal or hex notation (e.g. `159`, `0x9F`).

:warning: Beware that the configuration file is **case sensitive**.

### Input expressions

Input expressions consist of one or more key names separated by spaces or parenthesis, which give them different meaning:

  * `A B` means that keys have to be pressed successively (released in any order).
  * `(A B)` means that keys have to be pressed simultaneously in any order.
  * `A{B}` means that a key has to be held while another is pressed.
  * Groups and modifiers can also be nested like `A{B{C}}` or `(A B){C}`.
  * `!A` means that a key must not be pressed. When an expression ends with `!A`, it triggers when the key is released.
  * `"..."` string literals match when the enclosed [characters are typed](#character-typing).
  * With an initial `?` the mapping gets skipped as long as it only partially matches.

### Output expressions

The output expression format is analogous to the input expression format:

  * `A B` means that keys are pressed successively.
  * `(A B)` means that both keys are pressed simultaneously.
  * `A{B}` means that a key is held while another is pressed.
  * `!A` means that the (potentially pressed) key should be released before the rest of the expression is applied.
  * `^` splits the output in two parts, one which is applied when the input key is pressed and one when the [key is released](#output-on-key-release).
  * `"..."` string literals allow to specify [characters to type](#character-typing).
  * `$()` can be used for [launching applications](#application-launching).
  * An empty expression can be used to suppress any output.

### Order of mappings

Mappings are applied in consecutive order until a match is found, therefore their order is of importance. While the following outputs `A` as soon as `Meta` is pressed:

```bash
Meta    >> A
Meta{X} >> B
```

The other way round, nothing is output when `Meta` is pressed alone because depending on whether an `X` follows, either `B` or `A` is output:

```bash
Meta{X} >> B
Meta    >> A
```

:warning: You may want to add a `@forward-modifiers` [directive](#directives) to your configuration, which ensures that the common mouse-modifiers are never held back:

```bash
@forward-modifiers Shift Control Alt
```

For a detailed description of how the mapping is applied, see the [Functional principle](#functional-principle) section.

### Context awareness

Context blocks allow to enable mappings only in specific contexts. A context can be defined by `system`, the focused window `title`, window `class`, process `path` or the input `device`/`device-id` an event originates from.\
A block continues until the next block (respectively the end of the file). The block which applies in all contexts can be reopened using `default`. e.g.:

```ini
[default]

[title = "Visual Studio"]

[system = "Linux", class != "qtcreator"] # '!=' inverses a condition

[device = "Some Device Name"] # consecutive blocks share mappings
[device = "Some Other Device"]

[system = "Windows" path = "notepad.exe"] # comma separator is optional
```

:warning: The `device`/`device-id` filters on Windows require the installation of a [virtual device driver](#virtual-device-driver). The process `path` may not be available on Wayland and for processes with higher privileges. The window `title` is not available on MacOS.

The values of a context can be easily obtained using the _Next Key Info_ function of the tray icon or [keymapperctl](#keymapperctl).

Class and device filters match contexts with the _exact_ same string, others match contexts _containing_ the string.
For finer control [regular expressions](https://www.regexone.com) can be used. These have to be delimited with slashes. Optionally `i` can be appended to make the comparison case insensitive:

```ini
[title = /Visual Studio Code|Code OSS/i]
```

Additionally a `modifier` filter allows to activate blocks depending on the state of one or more keys:

```ini
# active when Virtual1 is down and Virtual2 is not
[modifier = "Virtual1 !Virtual2"]
```

### Abstract commands

To simplify mapping of one input expression to different output expressions, it can be mapped to an abstract command first. The command name can be chosen arbitrarily but must not be a key name. The configuration is case sensitive and all key names start with a capital letter, so it is advisable to begin command names with a lowercase letter:

```bash
Control{B} >> build
```

Subsequently this command can be mapped to one output expression per context. The last active mapping overrides the previous ones:

```ini
build >> Control{B}

[title="Visual Studio"]
build >> (Shift Control){B}
```

### Multiple stages

By inserting `[stage]` a configuration can be split into stages, which are evaluated separately. The output of a stage is the input of the following stage, where it can be mapped further:

```ini
# adjust keyboard layout
Z >> Y
Y >> Z

# map keys output by previous stage
[stage]
Control{Z} >> undo
```

### Output on key release

When an output expression contains `^`, it is only applied up to this point, when the input key is pressed. The part after the `^` is not applied until the input is released. Both parts can be empty:

```bash
# type "cmd" after the Windows run dialog appeared
Meta{C} >> Meta{R} ^ "cmd" Enter

# prevent key repeat
A >> B^

# output B when A is released
A >> ^B
```

### Virtual keys

`Virtual0` to `Virtual255` are virtual keys, which can be used as state switches. They are toggled when used in output expressions:

```bash
# toggle Virtual1 whenever ScrollLock is pressed
ScrollLock >> Virtual1

# release Virtual1 when Escape is pressed
Escape >> !Virtual1
```

They can be used as modifiers in input expressions:

```bash
# map A to B when Virtual1 is down
Virtual1{A} >> B

# map E to F when Virtual1 is NOT down
!Virtual1 E >> F

# keep G held as long as Virtual1 is down
Virtual1 >> G

# output H when Virtual1 is released
!Virtual1 >> H
```

Toggling virtual keys can also have immediate effects. Using them as modifiers is toggling them twice:

```bash
# toggle Virtual1 before and after pressing B
# this effectively maps D to A B C
Virtual1 >> A ^ C
D >> Virtual1{B}

# {} may also be empty. This maps C to A B
Virtual2 >> B
Virtual1 >> A Virtual2{}
C >> Virtual1{}
```

`ContextActive` exists separately for each context and is toggled when the context becomes active/inactive:

```ini
# toggle Virtual1 when entering and when leaving context
[title="Firefox"]
ContextActive >> Virtual1 ^ !Virtual1
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

```ini
[title="Remote Desktop"]
Any >> Any

[default]
```

On the output side it can also be used to release previously pressed modifiers first:

```bash
A >> !Any A
```

### Timeouts

Input expressions can contain timeouts in milliseconds e.g. `500ms` to specify the minimum duration a key needs to be held, or the minimum duration between two events. This can also be negated like `!500ms` to specify maximum durations:

```bash
# output Escape when CapsLock is held for a while
CapsLock{500ms} >> Escape

# output Escape when Control is pressed and released quickly
Control{!250ms} >> Escape

# output C when B quickly follows an A
A !250ms B >> C

# output D when A is tapped twice quickly
A{!200ms} !200ms A{!200ms} >> D
```

In output expressions it can be used to delay output or keep a key held for a while. e.g:

```bash
A >> B 500ms C{1000ms}
```

### Character typing

Output expressions can contain string literals with characters to type. The typeable characters depend on your keyboard layout. e.g:

```bash
AltRight{A} >> '@'

# long lines can be split using '\'
Meta{A} K >> \
  "Kind regards,\n" \
  "Douglas Quaid"
```

They can also be used in input expressions to match when the character are typed. e.g.:

```bash
? 'Abc' >> Backspace Backspace "Matched!"
```

:warning: The keyboard layout is evaluated when the configuration is loaded, switching is not yet supported.

### Key aliases / Macros

For convenience aliases for keys and even sequences can be defined. e.g.:

```bash
Win = Meta
Boss = Virtual            # assigns an unused virtual key
Alt = AltLeft | AltRight  # defines a logical key
proceed = Tab Tab Enter
```

In strings, regular expressions and terminal commands aliases can be inserted using `${Var}` or `$Var`. e.g.:

```bash
greet = "Hello"
F1 >> "${greet} World"
```

An alias can also be parameterized to create a macro. The arguments are provided in square brackets and referenced by `$0`, `$1`... e.g.:
```bash
print = $(echo $0 $1 >> ~/keymapper.txt)
F1 >> print["pressed the key", F1]

# multiple mappings on one line can be separated using ;
swap = $0 >> $1; \ 
       $1 >> $0
swap[Y, Z]
```

There are a few builtin macros `repeat[EXPR, N]`, `length[STR]`, `default[A, B]`, `apply[EXPR, ARGS...]`, `add/sub/mul/div/mod/min/max[A, B]` which allow to generate mappings and define some more advanced macros. e.g:

```bash
# when last character of string is typed, undo using backspace and output new string
substitute = ? "$0" >> repeat[Backspace, sub[length["$0"], 1]] "$1"
substitute["Cat", "Dog"]

# generate the string to output with an external program
type = $(keymapperctl --type "$0")
substitute[":whoami", type[$(whoami)]]

# add a `FN >> Meta{N}` mapping for each function key
apply[F$0 >> Meta{$0}, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12]
```

`$$` is substituted with the actual parameter count, which allows to add overloads:
```bash
log1 = $(echo "$0" >> $LOG_FILE)
log2 = $(echo "[$1]" "$0" >> $LOG_FILE)

# switch builds log1 or log2, which is then called with the second argument list
switch = $0$1
log = switch["log", $$][$0, $1]

F1 >> log["info message"]
F2 >> log["error message", "state"]
```

### Application launching

`$()` can be used to embed terminal commands in output expressions, which should be executed when triggered. e.g.:

```bash
Meta{C} >> $(C:\windows\system32\calc.exe) ^
Meta{W} >> $(exo-open --launch WebBrowser) ^

# on Windows console applications are revealed by prepending 'start'
Meta{C} >> $(start powershell) ^
```

:warning: You may want to append `^` to ensure that the command is not executed repeatedly as long as the input is kept held.

### Directives

The following directives, which are lines starting with an `@`, can be inserted in the configuration file:

- `forward-modifiers` allows to set a list of keys which should never be [held back](#order-of-mappings). e.g.:
  ```bash
  @forward-modifiers Shift Control Alt
  ```
  It effectively forwards these keys in each [stage](#multiple-stages) immediately, like:
  ```bash
  Shift   >> Shift
  Control >> Control
  Alt     >> Alt
  ```
  and automatically suppresses the forwarded keys in the output:
  ```bash
  # implicitly turned into 'Control{A} >> !Control Shift{B}'
  Control{A} >> Shift{B}
  ```

- `allow-unmapped-commands` and `enforce-lowercase-commands` change the way [commands](#abstract-commands) are validated. When used, then best together, so typing errors in key names are still detected. e.g.:
  ```bash
  @allow-unmapped-commands
  @enforce-lowercase-commands
  A >> command1   # OK, even though 'command1' has no output mapped
  command2 >> B   # OK, even though no input maps to 'command2'
  C >> Command3   # error: invalid key 'Command3'
  Command4 >> D   # error: invalid key 'Command4'
  ```

- `virtual-keys-toggle` allows to change the behavior of virtual keys in outputs. e.g.

    ```bash
    @virtual-keys-toggle true   # true is (still) the default

    # toggle Virtual1
    F1 >> Virtual1
    ```

    ```bash
    @virtual-keys-toggle false
    
    # press Virtual1
    F1 >> Virtual1

    # release Virtual1
    F2 >> !Virtual1

    # toggle Virtual1
    Virtual1{F3} >> !Virtual1
    F3 >> Virtual1
    ```

- `grab-device`, `skip-device`, `grab-device-id`, `skip-device-id` allow to explicitly specify the devices which `keymapperd` should grab. By default all keyboard devices are grabbed and mice only when mouse buttons or wheels were mapped.
The filters work like the [context filters](#context-awareness). e.g.:
  ```bash
  # do not grab anything but this one keyboard
  @skip-device /.*/
  @grab-device "Some Device Name"
  ```

- `include` and `include-optional` can be used to include a file in the configuration. e.g.:
  ```bash
  @include "filename.conf"

  # does not fail when the file is missing
  @include-optional "$HOME/.keymapper.conf"
  ```

- `done` stops parsing the configuration.

- `options` allows to set the command line options in the configuration. e.g.:

  ```bash
  @options update no-tray no-notify verbose no-update
  ```

- `toggle-active` allows to set a sequence which de-/activates keymapper. e.g.:

    ```bash
    @toggle-active ScrollLock
    ```

The following directives are for working around current limitations and can hopefully be removed in the future:

- `linux-compose-key` sets the key to use for composing special characters on Linux. e.g.:

  ```bash
  @linux-compose-key AltRight{Insert}
  ```

- `linux-highres-wheel-events` enables the handling of high-resolution wheel events on Linux.

- `macos-toggle-fn` allows to toggle the default state of the `FN` key on MacOS.

- `macos-iso-keyboard` should be added when the `IntlBackslash` and the `Backquote` keys are mixed up on MacOS.

Example configuration
---------------------
Please see and consider contributing to the ever growing [wiki](https://github.com/houmain/keymapper/wiki).

The [author's personal configuration](keymapper.conf) may also serve as an inspiration (which itself took some inspiration from [DreymaR's Big Bag Of Keyboard Tricks](https://dreymar.colemak.org/layers-extend.html)).

To get an impression of what a very advanced configuration can look like, check out a [power-user's configuration](https://github.com/ristomatti/keymapper-config/blob/main/keymapper.conf).

Functional principle
--------------------

For advanced application it is good to know how the mapping is applied:

  * All key strokes are intercepted and appended to a key sequence.
  * On every key stroke the key sequence is matched with all input expressions in consecutive order, until an expression matches or might match (when more strokes follow). Mappings in inactive contexts are skipped.
  * When the key sequence can no longer match any input expression (because more strokes followed), the longest exact match is looked for (by ignoring the last strokes). As long as still nothing can match, the first strokes are removed and forwarded as output.
  * When an input expression matches, the keys are removed from the sequence and the mapped expression is output.
  * Keys which already matched but are still physically pressed participate in expression matching as an optional prefix to the key sequence.

keymapperctl
------------

The application `keymapperctl` allows to control the running `keymapper` process from external applications.

It can be run arbitrarily often with one or more of the following arguments:
```
--input <sequence>    injects an input key sequence.
--output <sequence>   injects an output key sequence.
--type "string"       types a string of characters.
--notify "string"     shows a notification.
--next-key-info       outputs information about the next key press.
--set-config "file"   sets a new configuration.
--is-pressed <key>    sets the result code 0 when a virtual key is down.
--is-released <key>   sets the result code 0 when a virtual key is up.
--press <key>         presses a virtual key.
--release <key>       releases a virtual key.
--toggle <key>        toggles a virtual key.
--wait-pressed <key>  waits until a virtual key is pressed.
--wait-released <key> waits until a virtual key is released.
--wait-toggled <key>  waits until a virtual key is toggled (can also be Any).
--timeout <millisecs> sets a timeout for the following operation.
--wait <millisecs>    unconditionally waits a given amount of time.
--instance <id>       replaces another keymapperctl process with the same id.
--print "string"      outputs the string to the console.
--result              outputs the result code to the console (0 is success).
--restart             starts processing the first operation again.
```

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
```ini
sudo systemctl start keymapperd
keymapper -u
```

To install permanently, add `keymapper` to the desktop environment's auto-started applications and enable the `keymapperd` service:
```ini
sudo systemctl enable keymapperd
```

To make context awareness work under Wayland, the compositor has to inform `keymapper` about the focused window. For [wlroots-based](https://wiki.archlinux.org/title/Wayland#Compositors) compositors this works out of the box, other compositors need to send the information using the [D-Bus](https://freedesktop.org/wiki/Software/dbus/) interface. A [GNOME Shell extension](https://github.com/houmain/keymapper/tree/main/extra/share/gnome-shell/extensions/keymapper%40houmain.github.com) and a [KWin script](https://github.com/houmain/keymapper/tree/main/extra/share/kwin/scripts/keymapper) are provided doing this.

The values for the `device-id` context filters are obtained by looking for symlinks in `/dev/input/by-id`.

### MacOS

The MacOS build depends on [Karabiner-Element's](https://karabiner-elements.pqrs.org) virtual device driver.
One can install it either directly from [Karabiner-DriverKit-VirtualHIDDevice](https://github.com/pqrs-org/Karabiner-DriverKit-VirtualHIDDevice/releases) version 5.x.x (follow steps 1-4 of [install instructions](https://github.com/pqrs-org/Karabiner-DriverKit-VirtualHIDDevice?tab=readme-ov-file#usage)), or along with [Karabiner Elements](https://github.com/pqrs-org/Karabiner-Elements/releases) version 15.x.x.

A [Homebrew](https://brew.sh) formula is provided for building and installing keymapper:
```ini
brew tap houmain/tap
brew install --HEAD keymapper
```

Finally `keymapperd` and `keymapper` can be added to the `launchd` daemons/agents by calling:
```ini
sudo keymapper-launchd add
```

### Windows
An installer and a portable build can be downloaded from the [latest release](https://github.com/houmain/keymapper/releases/latest) page.

Most conveniently but possibly not always the very latest version can be installed using a package manager:
```ini
# install using winget
winget install keymapper

# install using Chocolatey
choco install keymapper
```

The installer configures the Windows task scheduler to start `keymapper.exe` and `keymapperd.exe` at logon.

After installation you can open the [configuration](#configuration) file by clicking the "Configuration" entry in the tray icon menu.

To use the portable build, simply start both `keymapper.exe` and `keymapperd.exe`. It is advisable to start `keymapperd.exe` with elevated privileges. Doing not so has a few limitations. Foremost the Windows key cannot be mapped reliably and applications which are running as administrator (like the task manager) resist any mapping.

#### Virtual device driver

The [device](#context-awareness) filter requires the installation of a virtual device driver. The only known freely available is [Interception](https://github.com/oblitum/Interception), which unfortunately has a [severe bug](https://github.com/oblitum/Interception/issues/25), that makes devices stop working after being dis-connected too often. Until this is fixed it not advisable to use it and it should only be installed when filtering by device is absolutely required. The `interception.dll` needs to be placed next to the `keymapperd.exe`.

Building
--------

A C++17 conforming compiler is required. A script for the
[CMake](https://cmake.org) build system is provided.

**Installing dependencies on Linux systems:**

<details>
<summary>On Arch Linux and derivatives</summary>

```ini
sudo pacman -S git base-devel
sudo pacman -S libusb dbus

# optional for Wayland support
sudo pacman -S wayland libxkbcommon

# optional for X11 support
sudo pacman -S libx11 libxkbcommon-x11

# optional for tray icon support
sudo pacman -S libappindicator-gtk3
```
</details>

<details>
<summary>On Debian Linux and derivatives</summary>

```ini
sudo apt install git cmake build-essential
sudo apt install libudev-dev libusb-1.0-0-dev libdbus-1-dev

# optional for Wayland support
sudo apt install libwayland-dev libxkbcommon-dev

# optional for X11 support
sudo apt install libx11-dev libx11-xcb-dev libxkbcommon-x11-dev

# optional for tray icon support
sudo apt install libayatana-appindicator3-dev
```
</details>

<details>
<summary>On Fedora Linux and derivatives</summary>

```ini
sudo dnf install git cmake make gcc-c++
sudo dnf install libudev-devel libusb1-devel dbus-devel

# optional for Wayland support
sudo dnf wayland-devel libxkbcommon-devel

# optional for X11 support
sudo dnf install libX11-devel

# optional for tray icon support
sudo dnf install libappindicator-gtk3-devel
```
</details>

**Checking out the source:**
```ini
git clone https://github.com/houmain/keymapper
```

**Building:**
```ini
cd keymapper
# to build with debug symbols append: -DCMAKE_BUILD_TYPE=Debug
cmake -B build
cmake --build build -j4
```

**Testing:**

To try it out, simply create a [configuration](#configuration) file and start it using:

```ini
sudo build/keymapperd -v
```

and

```ini
build/keymapper -v
```

License
-------
It is released under the GNU GPLv3. It comes with absolutely no warranty. Please see `LICENSE` for license details.
