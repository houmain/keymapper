
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

* Redefine keyboard shortcuts system-wide or per application.
* Manage all your keyboard shortcuts in a single configuration file.
* Change shortcuts for similar actions in different applications at once.
* Share configuration files between multiple systems (GNU/Linux, Windows).
* Bind keyboard shortcuts to terminal commands.
* Use mouse buttons in your mappings.

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
  * on Linux in `$HOME/.config/` and `/etc/`.
  * on Windows in the user's profile, `AppData\Local` and `AppData\Roaming` folders.
 
The command line argument `-u` causes the configuration to be automatically reloaded whenever the configuration file changes.

:warning: **In case of emergency:** You can always press the special key combination <kbd>Shift</kbd>+<kbd>Escape</kbd>+<kbd>K</kbd> to terminate `keymapperd`.

### Key names

The keys are named after their scan codes and are not affected by the present keyboard layout.
The names have been chosen to match on what the [web browsers](https://developer.mozilla.org/en-US/docs/Web/API/KeyboardEvent/code/code_values) have agreed upon, so this [handy website](http://keycode.info/) can be used to get a key's name.
For convenience the letter and digits keys are also named `A` to `Z` and `0` to `9`. The logical keys `Shift`, `Control` and `Meta` are also defined (each matches the left and right modifier keys). There are also [virtual keys](#virtual-keys) for state switching and an [Any](#any-key) key.

The mouse buttons are named: `ButtonLeft`, `ButtonRight`, `ButtonMiddle`, `ButtonBack` and `ButtonForward`.

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
  * `^` splits the output in two parts, one which is applied when the input is pressed and one when it is released (see [further explanation](#Output-on-key-release)).
  * `$()` can be used for [terminal command binding](#terminal-command-binding).
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

Context blocks allow to enable mappings only in specific contexts. A context can be defined by _system_, window _title_ or window _class_. They are opened like:

```bash
[system="Windows" title="..." class="..."]
```

A block continues until the next block (respectively the end of the file). The block which applies in all contexts can be reopened using `default`. e.g.:

```bash
[default]
CapsLock >> Backspace

[title="Visual Studio"]
Control{B} >> (Shift Control){B}

[system="Linux" class="qtcreator"]
...
```

The title filter matches windows _containing_ the string in the title, the class filter only matches windows with the _exact_ class name. For finer control [regular expressions](https://en.wikipedia.org/wiki/Regular_expression) can be used. These have to be delimited with slashes. Optionally `i` can be appended to make the comparison case insensitive:

```javascript
[title=/Visual Studio Code|Code OSS/i]
```

Additionally a common `modifier` for all the block's input expressions can be defined:

```bash
[modifier="CapsLock"]
K >> ArrowDown          # the same as "CapsLock{K} >> ArrowDown"
```

On Linux systems it is also possible to apply mappings when the input originates from a specific device:
```javascript
[device="Some Device Name"]
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

`Virtual0` to `Virtual9` are virtual keys, which can be used as state switches. They are toggled when used in output expressions and can be used as modifiers in input expressions:

```bash
# Virtual1 is toggled whenever ScrollLock is pressed
ScrollLock >> Virtual1

# map A to B when Virtual1 is down
Virtual1{A} >> B

# map E to F when Virtual1 is NOT down
!Virtual1 E >> F
```

### Any key

```Any``` can be used in input and output expressions.
In input expressions it matches any key and in output expressions it outputs the matched input.

```bash
# swap Control and Shift
Control{Any} >> Shift{Any}
Shift{Any} >> Control{Any}
```

This can for example be used to exclude an application from mapping:

```bash
[title="Remote Desktop"]
Any >> Any

[default]
...
```

### Key aliases

For convenience aliases for keys and even sequences can be defined. e.g.:

```bash
Win = Meta
Boss = Virtual1
Alt = AltLeft | AltRight
FindNext = Control{F3}
Greet = H E L L O
```

### Terminal command binding

`$()` can be used in output expressions to embed terminal commands, which should be executed when the output is triggered:

```bash
Meta{W} >> $(exo-open --launch WebBrowser) ^
```

:warning: You may want to append `^` to ensure that the command is not executed repeatedly as long as the input is kept hold.

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
* `keymapper` loads the configuration, informs the service about it and the active context and also executes mapped terminal commands.

For security and efficiency reasons, the communication between the two parts is kept as minimal as possible.

### Linux
`keymapperd` should be started as a service and `keymapper` as normal user within an X11 or Wayland session (currently the [GNOME Shell](https://en.wikipedia.org/wiki/GNOME_Shell) and [wlroots-based Wayland compositors](https://wiki.archlinux.org/title/Wayland#Compositors) are supported).

**Arch Linux and derivatives:**

An up to date build can be installed from the [AUR](https://aur.archlinux.org/packages/?K=keymapper).

To try it out, simply create a [configuration](#configuration) file and start it using:
```
systemctl start keymapperd
keymapper
```

To install permanently, enable the `keymapperd` service and add `keymapper` to the desktop environment's auto-started applications.

**Other Linux distributions:**

No packages are provided yet, please follow the instructions for [building manually](#Building) or download a portable build from the [latest release](https://github.com/houmain/keymapper/releases/latest) page.

To try it out, simply create a [configuration](#configuration) file and start it using:
```
sudo ./keymapperd &
./keymapper
```

### Windows
An installer and a portable build can be downloaded from the [latest release](https://github.com/houmain/keymapper/releases/latest) page.

The installer configures the Windows task scheduler to start `keymapper.exe` and `keymapperd.exe` at logon.

To use the portable build, simply create a [configuration](#configuration) file and start both `keymapper.exe` and `keymapperd.exe`. It is advisable to start `keymapperd.exe` with elevated privileges. Doing not so has a few limitations. Foremost the Windows key cannot be mapped reliably and applications which are running as administrator (like the task manager, ...) resist any mapping.

Building
--------

A C++17 conforming compiler is required. A script for the
[CMake](https://cmake.org) build system is provided.

**Installing dependencies on Debian Linux and derivatives:**
```
sudo apt install build-essential git cmake libudev-dev libusb-1.0-0-dev libx11-dev libdbus-1-dev libwayland-dev
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

License
-------

It is released under the GNU GPLv3. It comes with absolutely no warranty. Please see `LICENSE` for license details.
