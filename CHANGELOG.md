
# Changelog
All notable changes to this project will be documented in this file.
The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/).

## [Unreleased]

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

[Unreleased]: https://github.com/houmain/keymapper/compare/1.5.0...HEAD
[Version 1.5.0]: https://github.com/houmain/keymapper/compare/1.4.0...1.5.0
[Version 1.4.0]: https://github.com/houmain/keymapper/compare/1.3.0...1.4.0
[Version 1.3.0]: https://github.com/houmain/keymapper/compare/1.2.0...1.3.0
[Version 1.2.0]: https://github.com/houmain/keymapper/compare/1.1.5...1.2.0
[Version 1.1.5]: https://github.com/houmain/keymapper/releases/tag/1.1.5
