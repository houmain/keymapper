# Changelog

All notable changes to this project will be documented in this file.
The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/).

## [Version 5.2.0] - 2025-10-25

### Added

- Added directive `@virtual-keys-toggle`, which allows to change the behavior of virtual keys in outputs (#309). e.g.

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

### Fixed

- Improved performance of sending mouse events on Windows (#300).
- Fixed output on release with together groups (#306).

## [Version 5.1.0] - 2025-10-12

### Added

- Verbose keymapper logging logs virtual key state changes (#297).
- Allowing next-key-info to reply multiple keys (#212).

### Fixed

- Fixed obtaining current keyboard layout on X11/XKB (#296).
- Ignoring key repeat also when mouse was clicked (#294).
- Fixed `keymapperctl --toggle` when invalid key is passed.


## [Version 5.0.0] - 2025-09-02

The reason for the major version number increment is mainly because of the changed behavior of `;`. Up until 4.9 it was documented to start comments like `#`. Otherwise all configurations should work like before.

### Added

- Allow to separate multiple mappings on one line with `;`. This is useful for macros which generate multiple mappings. e.g.:
  ```bash
  swap = $0 >> $1 ; $1 >> $0
  swap[Y, Z]
  ```

- Allow to add mappings for inputs which are released. e.g.:
    ```bash
    # trigger when A is released
    !A >> B

    # trigger when B is released while A is held
    A{!B} >> C

    # trigger when B is released while A is NOT held
    !A !B >> C
    ```

- Toggling virtual keys can also have immediate effects. Using them as modifiers is toggling them twice:
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

- Automatically replace `Virtual` with an unused virtual key. e.g.:
    ```bash
    # assign different virtual keys to VimMode and CapsWord
    VimMode = Virtual
    CapsWord = Virtual
    ```

- Added `keymapperctl` operation `--notify` for showing notifications (#282). e.g.:
    ```bash
    notify = $(keymapperctl --notify "$0")
    F1 >> notify["Test"]
    ```

- Added `toggle-active` directive, which allows to set a sequence that de-/activates keymapper (#283). e.g.:

    ```python
    @toggle-active ScrollLock
    ```

- Added `include-optional` directive, which includes a file but does not fail when file does not exist.

### Fixed

- Fixed suppressed modifiers getting reapplied (#291).
- Fixed loading config from `"%HOME%\.config\keymapper"` on Windows (#292).
- Properly releasing keys when UAC prompt appeared on Windows.

## [Version 4.12.3] - 2025-07-13

### Fixed

- Fixed problem with virtual keys (introduced in 4.12.1 for #275).
- Fixed checking for a specific number of mouse wheel events (#280).

## [Version 4.12.2] - 2025-06-28

### Added

- Showing a hint in "Next Key Info" that Gnome/KDE script is not enabled.

### Changed

- Handling only lowres wheel events by default on Linux (#268).

### Fixed

- Not blocking when opening configuration from tray on Linux (#220).
- Fixed focus update when xwayland app is minimized to tray (#220).
- Fixed device context fallthrough (#277).
- Optimized SVG icon.

## [Version 4.12.1] - 2025-05-20

### Changed

- First try to find a match also including already matched events (#275).

### Fixed

- Made automatically reloading configuration file more robust (#276).

## [Version 4.12.0] - 2025-05-18

### Added

- Added dead key support to string typing on MacOS.
- Added dependencies of other Linux systems to build instructions.

### Changed

- Single character literals only set state of shift modifier, to make shortcuts like `Ctrl-<` work, when mapping to `'<'`.
- First try to match last matching mapping again (#275).

### Fixed

- Fixed some problems with FN key on MacOS (#263).
- Fixed unexpected input matching (#267).
- Allowing string literals in more places (#260 and #261).
- Ensure virtual device name is completely appended to forward devices on Linux.

## [Version 4.11.4] - 2025-03-31

### Fixed

- Fixed character typing on X11 systems with xkbcommon versions < 1.0 (#258).

## [Version 4.11.3] - 2025-03-30

### Added

- Added Unicode character typing on Linux using hex code input.

### Fixed

- Allow to clear compose key on config update.
- Prevent setting compose key to AltRight (#257).

## [Version 4.11.2] - 2025-03-29

### Added

- Allowed setting modifiers of typed characters (#213, #257). e.g.:

    ```
    Control{ '+' } >> Shift{ 'c' }
    ```

### Changed

- Releasing output after forwarding not matching keys (#244).

### Fixed

- Fixed a bug in unicode character typing on Windows (#257).
- Added concurrent key repeat prevention to all systems (#255).

## [Version 4.11.1] - 2025-03-20

### Added

- Added general unicode character typing on Windows (#240).

### Fixed

- Fixed some problems with FN key on MacOS (#239, #249).
- Added installation path in keymapper's PATH variable on MacOS (#251).
- Added redundancy check to wlroots context update.

## [Version 4.11.0] - 2025-03-01

### Added

- Added dead key support to string typing on Linux.
- Added `@compose-key` directive.
- Added `@options` directive (#223).
- Forwarding switch events on Linux (#248).
- Ignoring UTF-8 BOM in configuration file (#240).

### Changed

- Typing '?' when character to type is not present in layout (#231).
- Not grabbing devices with switches by default on Linux (#248).
- Removed automatic update of copyright year to allow reproducible builds (#245).

### Fixed

- Fixed build on 32bit Linux (#245).
- Fixed invalid number parameter handling in `keymapperctl`.
- Fixed missing autostart file on Linux (#246).

## [Version 4.10.2] - 2025-02-14

### Changed

- Unconditionally grabbing mouse on Windows again (#236).

### Fixed

- Sending mouse events with virtual device which forwards movement on Linux (#188).
- Sending less low-resolution mouse wheel events on Linux.
- Fixed mouse button-repeat prevention on Windows (#236).

## [Version 4.10.1] - 2025-02-07

### Added

- Providing arm64 binaries for Linux and Windows and universal binaries for MacOS.

### Changed

- Parsing macros in inactive systems' contexts.
- Hooking mouse only when it has mappings on Windows.

### Fixed

- Fixed keyrepeat after timeout (#216).
- Ignoring surplus timeout events in sequence (#113 and #217).
- Fixed missing help output (#233).
- Fixed build on Raspberry Pi (#224).

## [Version 4.10.0] - 2025-01-28

### Added

- Creating virtual forwarding devices on Linux (#188).
- Added `@macos-toggle-fn` directive (#215).
- Added tray icon for MacOS (icon is WIP).

### Changed

- Made virtual device name and install directory lowercase.
- Made `@macos-iso-keyboard` directive only affect builtin keyboard.

### Fixed

- Fixed forwarding of media key events on MacOS (#172).

## [Version 4.9.2] - 2025-01-04

### Added

- Automatically reloading configuration when `@include` file is modified (#211).
- Added support for mouse remapping with Interception driver (#184).

### Fixed

- Prevent concurrent keyrepeat events on Linux (#207).
- Fixed build when no CMake build type is set (#208).
- Corrected scancodes of keys Paste, Cut, Copy, Eject, Help, Sleep, WakeUp on Windows (#194).
- Checking that parameter of `@include` is enclosed in quotes.

## [Version 4.9.1] - 2024-11-24

### Added

- Added `@done` directive to stop parsing configuration.

### Fixed

- Fixed immediately releasing (e.g. `A >> B !B`).
- `!Any` checks previously matched (e.g. `A{Any !Any} >> Meta{Any}`) (#187).
- Improved swapping mouse wheel directions (delta of input is used) (#184).
- Fixed Next key info when key name is unknown.
- Next key info also shows wheel events.

## [Version 4.9.0] - 2024-11-15

### Added

- Added builtin macro `default` (#176).
- Added macro actual parameter `$$` (#176).
- Added builtin macro `apply` (#177).
- Added `@macos-iso-keyboard` directive.
- `keymapperctl --type` expands `$(commands)` under Windows.

### Changed

- Updated Karabiner VirtualHIDDevice to version 5.0.0 (#178).
- Looking for config in `$XDG_CONFIG_HOME` first (#180).
- Deprecated `;` as start of comment.

### Fixed

- Fixed build on MacOS (#179).
- Improved coexistence with Karabiner-Elements (#172).
- Fixed invalid index lookup on null object in KWin script.

## [Version 4.8.2] - 2024-09-02

### Added
- Added logical key `Alt`.

### Changed
- Allow to override keys with aliases. e.g.
   ```
   Alt = AltLeft
   AltGr = AltRight
   ```

## [Version 4.8.1] - 2024-09-05

### Added

- Added [@forward-modifiers](https://github.com/houmain/keymapper?tab=readme-ov-file#directives) directive (#174).
- Implemented `grab-device` directive support on MacOS (#166).

## [Version 4.8.0] - 2024-09-01

### Added

- Added `keymapperctl` operations `--input` and `--output` for injecting key presses. e.g.:

    ```
    keymapperctl --input Shift{A} B C
    ```
    with `--input` the mappings are applied as if the sequence came from an input device,
    with `--output` the sequence is output as if it was generated by keymapper (#170).

## [Version 4.7.3] - 2024-08-31

### Fixed

- Fixed mouse wheel breaking `?` input expressions.

## [Version 4.7.2] - 2024-08-24

### Added

- Added `WheelLeft` and `WheelRight` for horizontal mouse wheels.

### Changed

- Swapped `WheelUp` and `WheelDown` on Windows.

### Fixed

- Fixed leaking of unmapped mouse input on Windows.

## [Version 4.7.1] - 2024-08-19

### Added

- Added string interpolation, which works in strings, terminal commands and regular expressions. e.g.:
    ```bash
    VAR = "World"
    A >> "Hello ${VAR}"  # $VAR also works
    ```

- Allow to split a line with a `\` at the end. e.g.:
    ```bash
    Shift{A} >> \
       Control{B}
    ```

### Changed

- Aliases are no longer automatically substituted in terminal commands (use string interpolation).

### Fixed

- Fixed too eager macro evaluation.

## [Version 4.7.0] - 2024-08-17

### Added

- Allow to define top-level macros:
    ```bash
    macro = $0 >> $1
    macro[A, B]
    ```

- Added builtin macros `repeat[EXPR, N]`, `length[STR]`, `add/sub/mul/div/mod/min/max[A, B]`.

- Apply further argument list to result of macro. e.g.:

    ```bash
    case1 = $0
    case2 = $0 $0
    switch = case$0
    A >> switch[2][B]  # switch generates case2, which is called with second argument list
    ```

- Implemented `@grab-device` directive support on Windows.

### Fixed
- `@allow-unmapped-commands` also ignore mappings of undefined commands. e.g.:

    ```python
    @allow-unmapped-commands
    command >> A
    ```

- Added KDE6 support to keymapper KWin script.

## [Version 4.6.0] - 2024-08-07

### Added
- Allow string literals in input expressions. e.g. `'Abc' >> B`.
- Added `@allow-unmapped-commands` and `@enforce-lowercase-commands` directives.

### Changed
- Keep key held when pressed immediately after `!Any`.

### Fixed
- Fixed `#` and `]` in terminal commands and macros.
- Allow to undo ! in input. e.g. `!Shift A Shift{B}`
- Preserving order of hold back output.
- Fixed MacOS build.

## [Version 4.5.2] - 2024-07-28

### Added
- Looking for `keymapper.conf` in an optional `keymapper` subdirectory.

### Fixed
- `@include` looking up relative paths next to configuration file.

## [Version 4.5.1] - 2024-07-28

### Added
- Expanding ~ and variables in `@include` directive.

### Fixed
- Fixed `@grab-device` directives.

## [Version 4.5.0] - 2024-07-27

### Added
- Added `@grab-device`... directives.
- Added `@include` directive.
- Showing system in "Next Key Info".
- Allowing hyphens in identifiers.

### Changed
- Grabbing keyboards with mouse axes on Linux.
- `Any` no longer matches mouse buttons/wheel.

## [Version 4.4.5] - 2024-07-18

### Fixed

- Not reevaluating `?` inputs when context becomes active (#161).
- Prevent infinite loop when two `ContextActive` toggle each other.

### Changed
- No longer setting description of all executables to "Keymapper" on Windows (they were indistinguishable in task manager #161).

## [Version 4.4.4] - 2024-07-14

### Fixed

- Prevent key state validation from resetting virtual key state on Windows.

## [Version 4.4.3] - 2024-07-14

### Fixed

- Defined behavior of `!Virtual` in output to always release (#156).
- Fixed toggling virtual key set by `ContextActive` (#156).
- Fixed string typing occasionally releasing virtual keys (#156).

## [Version 4.4.2] - 2024-07-09

### Fixed

- Fixed potentially hanging key (#153).
- Improved not-timeout with modifier. e.g `A{B{!500ms}} >> C` (#153).
- Improved nested modifiers e.g. `A{B{C} D{E}}`.
- Prevent modifier in group e.g. `(A B{C})`.

## [Version 4.4.1] - 2024-07-26

### Changed

- Not always grabbing mice with keyboard keys on Linux (#152).

## [Version 4.4.0] - 2024-06-10

### Added

- Added multiple stages.
- Added `keymapperctl --type`.

### Changed

- Shutdown keymapperd on version mismatch (#149).
- Prevent not supported virtual keys in ? input expressions.

### Fixed

- Revert swapping mixed up `IntlBackslash` and `Backquote` keys on MacOS workaround (#150).
- Improved `!Any` in output.

## [Version 4.3.1] - 2024-05-18

### Changed

- Building Linux packages with `libayatana-appindicator3`.

### Fixed

- Fixed shutdown when no devices could be grabbed on Linux.

## [Version 4.3.0] - 2024-05-16

### Added

- Allow input expressions to begin with `?` to prevent might match.
- `!Any` in output releases all pressed keys.
- Using `libayatana-appindicator` instead of `libappindicator` when available.

### Changed

- Not reconnecting in keymapperctl when `--instance` is set.

### Fixed

- Improved mouse wheel handling.

## [Version 4.2.0] - 2024-05-08

### Added

- Added "Next Key Info" tray menu item.
- Added `keymapperctl` action `--next-key-info`.
- Added `device_id` context filter.
- Added mouse wheel keys `WheelUp` and `WheelDown`.

## [Version 4.1.3] - 2024-05-01

### Added

- Added keymapperctl --set-config.
- Added --no-notify keymapper argument to disable notifications.
- Creating config file when opened using tray icon on Linux.

### Fixed

- Fixed forwarding on cancelled sequence/group.

## [Version 4.1.2] - 2024-04-24

### Fixed

- Fixed device filter on MacOS.
- Fixed forwarding of FN keys on MacOS.
- Fixed slowly appearing tray icon menu on Linux.

## [Version 4.1.1] - 2024-04-19

### Added

- Added "Devices" entry to tray icon menu.

### Changed

- Always using Interception when available on Windows.

### Fixed

- Fixed tray icon Linux.

## [Version 4.1.0] - 2024-04-17

### Added

- Added keymapper tray icon for Linux (#126).

### Fixed

- Further improved selection of key releasing a triggered output (#122).
- Fixed logical keys in context modifiers (#128).
- Fixed ContextActive with fallthrough contexts.

## [Version 4.0.2] - 2024-04-11

### Fixed

- Improved selection of key releasing a triggered output (#122).
- Improved forwarding of input when a potential match fails.

## [Version 4.0.1] - 2024-04-09

### Fixed

- Fixed input timeouts on Linux (#91).
- Fixed ContextActive with output on release (#91).
- Restored substition of aliases in terminal commands (#91).
- Fixed error notifications on Linux.

## [Version 4.0.0] - 2024-03-28

### Added

- Added virtual key `ContextActive` (#91).
- Added aliases with parameters (#91).
- Added `keymapperctl` application (#105).
- Added device filter support on Windows using `Interception` (#107).
- Allow to invert context filters with !=.
- Consecutive blocks share mappings (#103).

### Changed

- Ignore aliases defined in contexts of other systems.
- Completely resetting state of virtual keys when updating the configuration.
- Setting `keymapper` process priority to high on Windows
- Exiting when config is invalid and not reloaded on Windows (#114).
- Allow to separate context filters with comma.

### Fixed

- Fixed groups with not timeout (#121).
- Not grabbing gamedevices on Linux (#119).
- Fixed starting multiple terminal commands at once.

## [Version 3.5.2] - 2024-01-24

### Fixed
- Fixed device filters when devices are changing (#41).
- Fixed process starting on Windows (#102).
- Interpreting escape sequence only in character typing string literals (\n, \r, \t).

## [Version 3.5.1] - 2024-01-19

### Added
- Added keys `Again`, `Props`, `Undo`, `Select`, `Copy`, `Open`, `Paste`, `Find`, `Cut`, `Help`, `Sleep`,
`WakeUp`, `Eject`, `Fn` (currently only on Linux #85).

### Changed
- Applying context updates even when a key is hold (#99).

### Fixed
- Improved keymapperd shutdown signal handing (#101)
- Hold Virtual keys could prevent context updates (#41, #99).

## [Version 3.5.0] - 2024-01-16

### Added
- Added diacritic support to string typing on Windows.
- Allow context filters to contain aliases.

### Changed
- Improved bringing spawned applications to front on Windows.

### Fixed
- Prevent second keymapper process from partially connecting on Linux.
- Prevent alias substitution in strings.

## [Version 3.4.0] - 2023-12-31

### Added

- _Not_-key following key in input expression matches when key is released. e.g. `A !A >> B`

### Changed

- No longer implicitly waiting for key release before timeout. e.g. `A !250ms B`
- Prevent input sequences without key down. e.g. `!A 500ms`

### Fixed
- Restored unintendedly reverted support for Gnome 45.

## [Version 3.3.0] - 2023-12-18

### Added
- Supporting devices with event IDs higher than 31 on Linux (#89).
- Added keymapper KWin script.
- Improved coexistence with Karabiner Elements on MacOS.

### Changed
- Updated Karabiner VirtualHIDDevice to version 3.1.0.

### Fixed
- Swapping mixed up IntlBackslash and Backquote keys on MacOS.
- Fixed context filter on MacOS.
- Fixed static build on Windows.

## [Version 3.2.0] - 2023-12-01

### Added
- Handling keys without scancode on Windows.
- Added keys LaunchApp2, BrowserHome, LaunchMail, LaunchMediaPlayer.

### Changed
- Grabbing all devices with keys on Linux.

## [Version 3.1.0] - 2023-11-18

### Added

- Added character output typing.
- Allow to override Not in output with Down.

### Changed

- Always hiding spawned console applications on Windows (use "start XY" to see it).

### Fixed

- Improved starting of terminal commands on Windows.
- Fixed executing terminal commands on wlroots based Wayland compositor.

## [Version 3.0.0] - 2023-10-26

### Added

- Added initial MacOS support.
- Toggling virtual keys can trigger output.

### Changed

- Simultaneous output on release.

## [Version 2.7.2] - 2023-10-14

### Changed

- Made Gnome extension compatible with Gnome 45.

## [Version 2.7.1] - 2023-08-17

### Fixed

- Properly handling inaccessible process path on Linux.

## [Version 2.7.0] - 2023-08-10

### Added

- Added process path context filter.

### Fixed

- Immediately applying context update on Linux.

## [Version 2.6.1] - 2023-05-07

### Changed

- Preventing mouse button repeat on Windows.
- Made mouse button debouncing optional (keymapperd parameter).

## [Version 2.6.0] - 2023-05-05

### Added

- Allow timeouts in output expressions.
- Allow scan codes in configuration.
- Added MetaLeft/MetaRight aliases OSLeft/OSRight.

## [Version 2.5.0] - 2023-03-09

### Added

- Showing notifications on Linux.

### Fixed

- Fixed hanging keys when using Windows remote desktop.
- Removed limit of 127 keys per sequence.
- Enabled visual styles for Windows about dialog.

## [Version 2.4.1] - 2022-11-27

### Fixed

- Keys triggered by timeout no longer released immediately.

## [Version 2.4.0] - 2022-11-26

### Added

- Allow to add timeouts to input sequences.
- Added about dialog on Windows.

### Fixed

- Cancel output-on-release when focus changed on Windows.
- Fixed releasing focused window detection on Linux.
- Fixed releasing virtual device on Linux.
- Fixed displaying version.

## [Version 2.3.0] - 2022-10-21

### Added

- Allow the device context filter to match multiple devices.

### Fixed

- Grab devices regardless of bus type.
- Linking filesystem library when building with older gcc/clang

## [Version 2.2.0] - 2022-09-17

### Added

- Handling wheel/slider functionality of some keyboards (Linux).

### Changed

- Grabbing keyboard/mouse devices despite unhandled axes (Linux).

## [Version 2.1.5] - 2022-07-29

### Added

- Added command line parameter to hide tray icon (Windows).

### Fixed

- Further improved Pause/NumLock key handling (Windows).

## [Version 2.1.4] - 2022-06-24

### Fixed

- Improved Pause/NumLock key handling (Windows).

### Changed

- Showing notification when config was updated (Windows).
- Renamed Break key to Cancel.

## [Version 2.1.3] - 2022-06-16

### Fixed

- Fixed loading configuration from '\$HOME/.config/' (Linux).

## [Version 2.1.2] - 2022-06-10

### Added

- Added Windows installer.

## [Version 2.1.1] - 2022-06-06

### Added

- Added tray icon for Windows client.
- Showing errors in notifications on Windows.

### Fixed

- Fail when config file cannot be read.
- Fixed output-on-release for terminal commands.
- Starting terminal commands in foreground.

## [Version 2.1.0] - 2022-05-23

### Added

- Added mouse button support.
- Added device context filter (only supported on Linux).

### Changed

- Split Windows version in client/server.
- Removed Interception mode on Windows.
- Removed colored error messages.

## [Version 1.10.0] - 2022-05-03

### Added

- Exiting on special key sequence Shift-Escape-K.
- Any in output expressions outputs the matched input.
- Added Break key.

### Fixed

- Improved sending of Ctrl-key sequences on Windows.
- Restored order of logical key substitution.

## [Version 1.9.0] - 2022-03-13

### Added

- Added D-Bus context update support.
- Added gnome-shell extension for updating focused window on Wayland.
- Added wlroots context update support on Wayland.

## [Version 1.8.3] - 2022-03-07

### Fixed

- Better handling of unknown version by build script.

## [Version 1.8.2] - 2022-02-20

### Changed

- Improved Not in output behavior.
- Statically linking runtime under Windows.

### Fixed

- Fixed right-modifier / mouse drag under Windows.
- Setting initial context on Windows.

## [Version 1.8.1] - 2022-01-23

### Changed

- No longer grabbing combined keyboard/mouse devices.
- CMake defaults to build type "Release".

## [Version 1.8.0] - 2021-12-20

### Added

- Multiple contexts can be active at once.
- [default] can be used to return to the default context.
- Logical keys can be defined.
- Allow to define common modifiers for context blocks.

### Fixed

- Updating active context on configuration update.
- AltRight is no longer implicitly forwarded.

## [Version 1.7.0] - 2021-12-13

### Changed

- Removed implicitly mapping of modifier keys.
- Matching begin of sequence when might-match failed.
- Completely releasing sequences and modifier groups.

### Added

- Documented importance of mapping order.

### Fixed

- Made Linux keyboard initialization more robust.

## [Version 1.6.0] - 2021-08-29

### Added

- Added terminal command output.
- Allow mapping of sequences in context blocks.
- Add option to check the validity of the config file, then exit.
- Added icon and metadata to Windows executable.

### Fixed

- Fixed bug with more than one virtual key in output.
- Fixed regular expressions containing [ ].

## [Version 1.5.0] - 2021-05-10

### Added

- Allow to define output on key release.

### Fixed

- Stricter validation of configuration to reduce undefined behavior.

## [Version 1.4.0] - 2021-03-29

### Changed

- Releasing triggered input in reverse order.

### Fixed

- Prevent hanging key.
- Validating state on Windows after session change.

## [Version 1.3.0] - 2021-01-26

### Added

- Optional verbose output.
- Regular expression support for context definition.

### Changed

- Updating context when window title changes.
- Applying system context independent of window context.

## [Version 1.2.0] - 2021-01-22

### Added

- Support of multiple keyboards under Linux.
- Improved device hot-plugging.
- Added some missing keys.

### Changed

- Simplified context block definition.
- Made X11 dependency optional.

### Fixed

- Proper output of Any key.
- Might match when key is hold.

## [Version 1.1.5] - 2020-05-09

[version 5.2.0]: https://github.com/houmain/keymapper/compare/5.1.0...5.2.0
[version 5.1.0]: https://github.com/houmain/keymapper/compare/5.0.0...5.1.0
[version 5.0.0]: https://github.com/houmain/keymapper/compare/4.12.3...5.0.0
[version 4.12.3]: https://github.com/houmain/keymapper/compare/4.12.2...4.12.3
[version 4.12.2]: https://github.com/houmain/keymapper/compare/4.12.1...4.12.2
[version 4.12.1]: https://github.com/houmain/keymapper/compare/4.12.0...4.12.1
[version 4.12.0]: https://github.com/houmain/keymapper/compare/4.11.4...4.12.0
[version 4.11.4]: https://github.com/houmain/keymapper/compare/4.11.3...4.11.4
[version 4.11.3]: https://github.com/houmain/keymapper/compare/4.11.2...4.11.3
[version 4.11.2]: https://github.com/houmain/keymapper/compare/4.11.1...4.11.2
[version 4.11.1]: https://github.com/houmain/keymapper/compare/4.11.0...4.11.1
[version 4.11.0]: https://github.com/houmain/keymapper/compare/4.10.2...4.11.0
[version 4.10.2]: https://github.com/houmain/keymapper/compare/4.10.1...4.10.2
[version 4.10.1]: https://github.com/houmain/keymapper/compare/4.10.0...4.10.1
[version 4.10.0]: https://github.com/houmain/keymapper/compare/4.9.2...4.10.0
[version 4.9.2]: https://github.com/houmain/keymapper/compare/4.9.1...4.9.2
[version 4.9.1]: https://github.com/houmain/keymapper/compare/4.9.0...4.9.1
[version 4.9.0]: https://github.com/houmain/keymapper/compare/4.8.2...4.9.0
[version 4.8.2]: https://github.com/houmain/keymapper/compare/4.8.1...4.8.2
[version 4.8.1]: https://github.com/houmain/keymapper/compare/4.8.0...4.8.1
[version 4.8.0]: https://github.com/houmain/keymapper/compare/4.7.3...4.8.0
[version 4.7.3]: https://github.com/houmain/keymapper/compare/4.7.2...4.7.3
[version 4.7.2]: https://github.com/houmain/keymapper/compare/4.7.1...4.7.2
[version 4.7.1]: https://github.com/houmain/keymapper/compare/4.7.0...4.7.1
[version 4.7.0]: https://github.com/houmain/keymapper/compare/4.6.0...4.7.0
[version 4.6.0]: https://github.com/houmain/keymapper/compare/4.5.2...4.6.0
[version 4.5.2]: https://github.com/houmain/keymapper/compare/4.5.1...4.5.2
[version 4.5.1]: https://github.com/houmain/keymapper/compare/4.5.0...4.5.1
[version 4.5.0]: https://github.com/houmain/keymapper/compare/4.4.5...4.5.0
[version 4.4.5]: https://github.com/houmain/keymapper/compare/4.4.4...4.4.5
[version 4.4.4]: https://github.com/houmain/keymapper/compare/4.4.3...4.4.4
[version 4.4.3]: https://github.com/houmain/keymapper/compare/4.4.2...4.4.3
[version 4.4.2]: https://github.com/houmain/keymapper/compare/4.4.1...4.4.2
[version 4.4.1]: https://github.com/houmain/keymapper/compare/4.4.0...4.4.1
[version 4.4.0]: https://github.com/houmain/keymapper/compare/4.3.1...4.4.0
[version 4.3.1]: https://github.com/houmain/keymapper/compare/4.3.0...4.3.1
[version 4.3.0]: https://github.com/houmain/keymapper/compare/4.2.0...4.3.0
[version 4.2.0]: https://github.com/houmain/keymapper/compare/4.1.3...4.2.0
[version 4.1.3]: https://github.com/houmain/keymapper/compare/4.1.2...4.1.3
[version 4.1.2]: https://github.com/houmain/keymapper/compare/4.1.1...4.1.2
[version 4.1.1]: https://github.com/houmain/keymapper/compare/4.1.0...4.1.1
[version 4.1.0]: https://github.com/houmain/keymapper/compare/4.0.2...4.1.0
[version 4.0.2]: https://github.com/houmain/keymapper/compare/4.0.1...4.0.2
[version 4.0.1]: https://github.com/houmain/keymapper/compare/4.0.0...4.0.1
[version 4.0.0]: https://github.com/houmain/keymapper/compare/3.5.2...4.0.0
[version 3.5.2]: https://github.com/houmain/keymapper/compare/3.5.1...3.5.2
[version 3.5.1]: https://github.com/houmain/keymapper/compare/3.5.0...3.5.1
[version 3.5.0]: https://github.com/houmain/keymapper/compare/3.4.0...3.5.0
[version 3.4.0]: https://github.com/houmain/keymapper/compare/3.3.0...3.4.0
[version 3.3.0]: https://github.com/houmain/keymapper/compare/3.2.0...3.3.0
[version 3.2.0]: https://github.com/houmain/keymapper/compare/3.1.0...3.2.0
[version 3.1.0]: https://github.com/houmain/keymapper/compare/3.0.0...3.1.0
[version 3.0.0]: https://github.com/houmain/keymapper/compare/2.7.2...3.0.0
[version 2.7.2]: https://github.com/houmain/keymapper/compare/2.7.1...2.7.2
[version 2.7.1]: https://github.com/houmain/keymapper/compare/2.7.0...2.7.1
[version 2.7.0]: https://github.com/houmain/keymapper/compare/2.6.1...2.7.0
[version 2.6.1]: https://github.com/houmain/keymapper/compare/2.6.0...2.6.1
[version 2.6.0]: https://github.com/houmain/keymapper/compare/2.5.0...2.6.0
[version 2.5.0]: https://github.com/houmain/keymapper/compare/2.4.1...2.5.0
[version 2.4.1]: https://github.com/houmain/keymapper/compare/2.4.0...2.4.1
[version 2.4.0]: https://github.com/houmain/keymapper/compare/2.3.0...2.4.0
[version 2.3.0]: https://github.com/houmain/keymapper/compare/2.2.0...2.3.0
[version 2.2.0]: https://github.com/houmain/keymapper/compare/2.1.5...2.2.0
[version 2.1.5]: https://github.com/houmain/keymapper/compare/2.1.4...2.1.5
[version 2.1.4]: https://github.com/houmain/keymapper/compare/2.1.3...2.1.4
[version 2.1.3]: https://github.com/houmain/keymapper/compare/2.1.2...2.1.3
[version 2.1.2]: https://github.com/houmain/keymapper/compare/2.1.1...2.1.2
[version 2.1.1]: https://github.com/houmain/keymapper/compare/2.1.0...2.1.1
[version 2.1.0]: https://github.com/houmain/keymapper/compare/1.10.0...2.1.0
[version 1.10.0]: https://github.com/houmain/keymapper/compare/1.9.0...1.10.0
[version 1.9.0]: https://github.com/houmain/keymapper/compare/1.8.2...1.9.0
[version 1.8.3]: https://github.com/houmain/keymapper/compare/1.8.2...1.8.3
[version 1.8.2]: https://github.com/houmain/keymapper/compare/1.8.1...1.8.2
[version 1.8.1]: https://github.com/houmain/keymapper/compare/1.8.0...1.8.1
[version 1.8.0]: https://github.com/houmain/keymapper/compare/1.7.0...1.8.0
[version 1.7.0]: https://github.com/houmain/keymapper/compare/1.6.0...1.7.0
[version 1.6.0]: https://github.com/houmain/keymapper/compare/1.5.0...1.6.0
[version 1.5.0]: https://github.com/houmain/keymapper/compare/1.4.0...1.5.0
[version 1.4.0]: https://github.com/houmain/keymapper/compare/1.3.0...1.4.0
[version 1.3.0]: https://github.com/houmain/keymapper/compare/1.2.0...1.3.0
[version 1.2.0]: https://github.com/houmain/keymapper/compare/1.1.5...1.2.0
[version 1.1.5]: https://github.com/houmain/keymapper/releases/tag/1.1.5
