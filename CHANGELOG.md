
# Changelog
All notable changes to this project will be documented in this file.
The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/).

## [Unreleased]

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

[Unreleased]: https://github.com/houmain/keymapper/compare/1.7.0...HEAD
[Version 1.7.0]: https://github.com/houmain/keymapper/compare/1.6.0...1.7.0
[Version 1.6.0]: https://github.com/houmain/keymapper/compare/1.5.0...1.6.0
[Version 1.5.0]: https://github.com/houmain/keymapper/compare/1.4.0...1.5.0
[Version 1.4.0]: https://github.com/houmain/keymapper/compare/1.3.0...1.4.0
[Version 1.3.0]: https://github.com/houmain/keymapper/compare/1.2.0...1.3.0
[Version 1.2.0]: https://github.com/houmain/keymapper/compare/1.1.5...1.2.0
[Version 1.1.5]: https://github.com/houmain/keymapper/releases/tag/1.1.5
