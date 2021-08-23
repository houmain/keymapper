
keymapper
=========
<p>
<a href="https://ci.appveyor.com/project/houmain/keymapper-windows-x64">
<img alt="AppVeyor" src="https://img.shields.io/appveyor/build/houmain/keymapper-windows-x64?label=build%20Windows-x64"></a>
<a href="https://ci.appveyor.com/project/houmain/keymapper-ubuntu-x64">
<img alt="AppVeyor" src="https://img.shields.io/appveyor/build/houmain/keymapper-ubuntu-x64?label=build%20Linux-x64"></a>
<a href="https://github.com/houmain/keymapper/issues">
<img alt="Issues" src="https://img.shields.io/github/issues-raw/houmain/keymapper.svg"/></a>

<a href="#configuration">Configuration</a> |
<a href="#example-configuration">Example</a> |
<a href="#functional-principle">Functional principle</a> |
<a href="#installation">Installation</a> |
<a href="#building">Building</a> |
<a href="https://github.com/houmain/keymapper/blob/main/CHANGELOG.md">Changelog</a>
</p>

A cross-platform context-aware key remapper. It allows to:

* Redefine keyboard shortcuts system-wide or per application.
* Manage all your keyboard shortcuts in a single configuration file.
* Change shortcuts for similar actions in different applications at once.
* Share configuration files between multiple systems (GNU/Linux, Windows).
* Bind keyboard shortcuts to terminal commands.

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

Unless overridden, using the command line argument `-c`, the configuration is read from:
  * on Linux: &nbsp; &nbsp; &nbsp; `$HOME/.config/keymapper.conf`
  * on Windows: `keymapper.conf` in the working directory.

The command line argument `-u` causes the configuration to be automatically reloaded whenever the configuration file changes.

### Key names

The keys are named after their scan codes and are not affected by the present keyboard layout.
The names have been chosen to match on what the [web browsers](https://developer.mozilla.org/en-US/docs/Web/API/KeyboardEvent/code/code_values) have agreed upon, so this [handy website](http://keycode.info/) can be used to get a key's name.
For convenience the letter and digits keys are also named `A` to `Z` and `0` to `9`. The logical keys `Shift`, `Control` and `Meta` are also defined (each matches the left and right modifier keys). There are also [virtual keys](#virtual-keys) for state switching and an [Any](#any-key) key.

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
  * `(A B)` and `A{B}` mean that both keys are pressed simultaneously.
  * `!A` means that the (potentially pressed) key should be released before the rest of the expression is applied.
  * `^` splits the output in two parts, one which is applied when the input is pressed and one when it is released (see [further explanation](#Output-on-key-release)).
  * `$()` can be used for [terminal command binding](#terminal-command-binding).
  * An empty expression can be used to suppress any output.

### Context awareness

In order to map an input expression to different output expressions, depending on the focused window, it first needs to be mapped to an abstract command. The command name can be chosen arbitrarily but must not be a key name. The configuration is case sensitive and all key names start with a capital letter, so it is advisable to begin command names with a lowercase letter:

```bash
Control{B} >> build
```

Subsequently this command can be mapped to an output expression:

```bash
build >> F5
```

Additionally it can be mapped within a context block. These blocks define a context by system, window title or window class in which commands should be mapped to different output expressions. They are opened like:

```bash
[system="Windows" title="..." class="..."]
```

and continue until the next block header (respectively the end of the file). e.g.:

```bash
[title="Visual Studio"]
build            >> (Shift Control){B}
go_to_definition >> F12

[system="Linux" class="qtcreator"]
...
```

The title filter matches windows _containing_ the string in the title, the class filter only matches windows with the _exact_ class name. For finer control [regular expressions](https://en.wikipedia.org/wiki/Regular_expression) can be used. These have to be delimited with slashes. Optionally `i` can be appended to make the comparison case insensitive. e.g.:

```javascript
[title=/Visual Studio Code|Code OSS/i]
```

### Output on key release

When an output expression contains `^`, it is only applied up to this point, when the input key is pressed. The part after the `^` is not applied until the input is released. Both parts can be empty. e.g.:

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
In input expressions it matches any key and in output expressions it outputs the current stroke.

```bash
# keep Control-A but map A to B
Control{Any} >> Any
A >> B
```

### Key aliases

For convenience aliases for keys can be defined:

```bash
Win = Meta
Boss = Virtual1
```

### Terminal command binding

`$()` can be used in output expressions to embed terminal commands, which should be executed when the output is triggered. e.g.:

```bash
Meta{W} >> $(exo-open --launch WebBrowser) ^
```

Example configuration
---------------------

The [author's personal configuration](keymapper.conf) may serve as an inspiration (which itself took some inspiration from [DreymaR's Big Bag Of Keyboard Tricks](https://dreymar.colemak.org/layers-extend.html)).

Functional principle
--------------------

For advanced application it is good to know how the mapping is applied:

  * All key strokes are intercepted and appended to a key sequence.
  * On every key stroke the key sequence is matched with all input expressions in consecutive order, until an expression matches or might match (when more strokes follow).
  * When an input expression matches, the key sequence is cleared and the mapped expression is output.
  * As long as the key sequence can not match any input expression, its first stroke is removed and forwarded as output.
  * Keys which already matched but are still physically pressed participate in expression matching as an optional prefix to the key sequence.

Installation
------------
### Linux
On Linux the program is split into two parts:
* `keymapperd` is the daemon which needs to be run as root or some other user who is authorized to grab the keyboard and inject keys.
* `keymapper` loads the configuration, informs the daemon about it and the active context and also executes mapped terminal commands. It needs to be run as normal user within an X11 session. Wayland is not yet supported, but it is possible to build keymapper without context awareness and the X11 dependency.

**Arch Linux and derivatives:**

An up to date build can be installed from the [AUR](https://aur.archlinux.org/packages/?K=keymapper).

To try it out, simply create a [configuration](#configuration) file and start it using:
```
systemctl start keymapperd
keymapper
```

The package already adds `keymapper` to the desktop environment's auto-started applications. As long as the service is not running, it does nothing but wait for the service to start. So to install permanently, only the `keymapperd` service has to be enabled:
```
systemctl enable keymapperd
```

**Other Linux distributions:**

No packages are provided yet, please follow the instructions for [building manually](#Building) or download a portable build from the [latest release](https://github.com/houmain/keymapper/releases/latest) page.

To try it out, simply create a [configuration](#configuration) file and start it using:
```
sudo ./keymapperd &
./keymapper
```

### Windows
A portable build can be downloaded from the [latest release](https://github.com/houmain/keymapper/releases/latest) page.

`keymapper.exe` can simply be started without special permissions. To install it permanently, simply add it to the auto-started applications.

There are two modes of operation:

* By default a [Low level keyboard hook](https://docs.microsoft.com/en-us/windows/desktop/winmsg/about-hooks) is used, which generally works fine but has a few limitations. Foremost the Windows key cannot be mapped reliably and applications which are running as administrator (like the login screen, task manager, ...) resist any mapping.

* When the command line argument `-i` is passed, the [Interception](https://github.com/oblitum/Interception/) library is used. It does not have these limitations, but a special keyboard driver needs to be [installed](https://github.com/oblitum/Interception/#driver-installation) and the `interception.dll` needs to be placed in the working directory.

Building
--------

A C++17 conforming compiler is required. A script for the
[CMake](https://cmake.org) build system is provided.

**Installing dependencies on Debian Linux and derivatives:**
```
sudo apt install build-essential git cmake libudev-dev libusb-1.0-0-dev libx11-dev
```

**Checking out the source:**
```
git clone https://github.com/houmain/keymapper
```

**Building:**
```
cd keymapper
mkdir build
cd build
cmake ..
cmake --build . --config Release
```

License
-------

It is released under the GNU GPLv3. It comes with absolutely no warranty. Please see `LICENSE` for license details.
