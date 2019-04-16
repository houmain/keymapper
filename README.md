[![Build status](https://ci.appveyor.com/api/projects/status/ykij7d5lrw7yc52d?svg=true)](https://ci.appveyor.com/project/houmaster/keymapper)

keymapper
=========

A cross-platform context-aware key remapper. It allows to:
* redefine keyboard shortcuts system-wide or per application.
* manage all your keyboard shortcuts in a single configuration file.
* change shortcuts for similar actions in different applications at once.
* share configuration files between multiple systems (GNU/Linux, Windows)

Configuration
-------------

Configuration files are easily written by hand and mostly consist of lines with [input expressions](#input-expressions) and corresponding [output expressions](#output-expressions) separated by ```>>```:

```bash
# comments start with # or ; and continue until the end of a line
CapsLock >> Backspace
Z >> Y
Y >> Z
Control{Q} >> Alt{F4}
```

Unless overridden, using the command line argument ```-c```, the configuration is read from:
  * on Linux: &nbsp; &nbsp; &nbsp; ```$HOME/.config/keymapper.conf```
  * on Windows: ```keymapper.conf``` in the working directory.

The command line argument ```-u``` causes the configuration to be automatically reloaded whenever the configuration file changes.

### Key names

The keys are named after their scancodes and not affected by the present keyboard layout.
The names have been chosen to match on what the [web browsers](https://developer.mozilla.org/en-US/docs/Web/API/KeyboardEvent/code) have agreed upon, so this [handy website](http://keycode.info/) can be used to get a key's name.

For convenience the letter and digits keys are also named ```A``` to ```Z``` and ```0``` to ```9```. The logical keys ```Shift```, ```Control```, ```Meta``` and ```Any``` are also defined (which match the keys the names suggest). There are also [virtual keys](#virtual-keys) for state switching.

### Input expressions

Input expressions consist of one or more key names separated by spaces or parenthesis, which give them different meaning:

  * ```A B``` means that keys have to be pressed successively (released in any order).
  * ```(A B)``` means that keys have to be pressed simultaneously in any order.
  * ```A{B}``` means that a key has to be hold while another is pressed.
  * ```!A``` means that a key must not be pressed.
  * Groups and modifiers can also be nested like ```A{B{C}}``` or ```(A B){C}```.

### Output expressions

The output expression format is analogous to the input expression format:

  * ```A B``` means that keys are pressed successively.
  * ```(A B)``` and ```A{B}``` mean that both keys are pressed simultaneously.
  * ```!A``` means that the (potentially pressed) key should be released before the rest of the expression is applied.
  * An empty expression can be used to suppress any output.

### Context awareness

In order to map an input expression to different output expressions, depending on the focused window, it first needs to be mapped to an abstract command. The command name can be chosen arbitrarily but must not be a key name:

```bash
Control{B} >> build
```

Subsequently this command can be mapped to an output expression:

```bash
build >> F5
```

Additionally it can be mapped within a context block. These blocks define a context by system, window title or window class in which commands should be mapped to different output expressions. They are opened like:

```bash
[Window system="Windows" title="..." class="..."]
```

and continue until the next block header (respectively the end of the file). e.g.:

```bash
[Window title="Visual Studio"]
build            >> (Shift Control){B}
go_to_definition >> F12

[Window system="Linux" class="terminator"]
...
```

### Virtual keys

```Virtual1``` to ```Virtual8``` are virtual keys, which can be used as state switches. They are toggled when used in output expressions and can be used as modifiers in input expressions:

```bash
# Virtual1 is toggled whenever ScrollLock is pressed
ScrollLock >> Virtual1

# map A to B when Virtual1 is down
Virtual1{A} >> B

# map E to F when Virtual1 is NOT down
!Virtual1 E >> F
```

### Key aliases

For convenience aliases for keys can be defined:

```bash
Boss = Virtual1
```

Functional principle
--------------------

For advanced application it is good to know how the mapping is applied:

  * Key strokes are retained and appended to a key sequence as long as no input expression matches the sequence, but as least one expression still might match (when more strokes follow).
  * The current key sequence is matched with all input expressions in consecutive order, until an expression matches or might match.
  * When an input expression matches, the key sequence is completely replaced with the mapped output expression.
  * When no input expression matches, the key sequence is forwarded unmodified.
  * Keys which already matched but are still physically pressed participate in expression matching as an optional prefix to the current key sequence.

Building
--------

Only a C++17 conforming compiler is required. A script for the
[CMake](https://cmake.org) build system is provided.

Installation
------------

### GNU / Linux / X11

On Linux the program is split into two parts:
* ```keymapperd``` is the daemon which needs to be run as root or some other user who is authorized to grab the keyboard and inject keys.
* ```keymapper``` needs to be run as normal user within a X11 session. It loads the configuration and informs the daemon about it and the active context.

**Arch Linux** users can install it from the [AUR](https://aur.archlinux.org/packages/keymapper-git).

### Windows

On Windows ```keymapper.exe``` can simply be started without special permissions.

There are two modes of operation:

* By default a [Low level keyboard hook](https://docs.microsoft.com/en-us/windows/desktop/winmsg/about-hooks) is used, which generally works fine but has a few limitations. Foremost the Windows key cannot be mapped reliably and applications which are running as administrator (like the login screen, task manager, ...) resist any mapping.

* When the command line argument ```-i``` is passed, the [Interception](https://github.com/oblitum/Interception/) library is used. It does not have these limitations, but a special keyboard driver needs to be installed and the ```interception.dll``` needs to be placed in the working directory.

License
-------

It is released under the GNU GPLv3. It comes with absolutely no warranty. Please see `LICENSE` for license details.
