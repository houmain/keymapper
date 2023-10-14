[![Build Status](https://github.com/pqrs-org/Karabiner-DriverKit-VirtualHIDDevice/workflows/CI/badge.svg)](https://github.com/pqrs-org/Karabiner-DriverKit-VirtualHIDDevice/actions)
[![License](https://img.shields.io/badge/license-Public%20Domain-blue.svg)](https://github.com/pqrs-org/Karabiner-DriverKit-VirtualHIDDevice/blob/main/LICENSE.md)

# Karabiner-DriverKit-VirtualHIDDevice

Virtual devices (keyboard and mouse) implementation for macOS using DriverKit.

## Supported systems

-   macOS 14 Sonoma
    -   Both Intel-based Macs and Apple Silicon Macs
-   macOS 13 Ventura
    -   Both Intel-based Macs and Apple Silicon Macs
-   macOS 12 Monterey
    -   Both Intel-based Macs and Apple Silicon Macs
-   macOS 11 Big Sur
    -   Both Intel-based Macs and Apple Silicon Macs

## Status

-   Implemented:
    -   Extension manager
    -   Virtual HID keyboard
    -   Virtual HID pointing
    -   Virtual HID device client

## Documents

-   [How to be close to DriverKit](DEVELOPMENT.md)
-   [Extracts from xnu](XNU.md)

## Screenshots

-   System Preferences (macOS detects the virtual keyboard)<br/><br />
    <img src="docs/images/system-preferences@2x.png" width="668" alt="System Preferences" /><br /><br />

---

## Usage

1.  Open `dist/Karabiner-DriverKit-VirtualHIDDevice-x.x.x.pkg`.
2.  Install files via installer.
3.  Execute the following command in Terminal.

    ```shell
    /Applications/.Karabiner-VirtualHIDDevice-Manager.app/Contents/MacOS/Karabiner-VirtualHIDDevice-Manager activate
    ```

4.  Run a client program to test the driver extension.

    ```shell
    git clone --depth 1 https://github.com/pqrs-org/Karabiner-DriverKit-VirtualHIDDevice.git
    cd Karabiner-DriverKit-VirtualHIDDevice/examples/virtual-hid-device-service-client
    brew install xcodegen
    make
    make run
    ```

## Uninstallation

1.  Run uninstaller in Terminal.

    ```shell
    bash '/Library/Application Support/org.pqrs/Karabiner-DriverKit-VirtualHIDDevice/scripts/uninstall/deactivate_driver.sh'
    sudo bash '/Library/Application Support/org.pqrs/Karabiner-DriverKit-VirtualHIDDevice/scripts/uninstall/remove_files.sh'
    ```

### Installed files

-   `/Applications/.Karabiner-VirtualHIDDevice-Manager.app`
-   `/Library/Application Support/org.pqrs/Karabiner-DriverKit-VirtualHIDDevice`
-   `/Library/LaunchDaemons/org.pqrs.Karabiner-DriverKit-VirtualHIDDeviceClient.plist`
-   `/Library/Application Support/org.pqrs/tmp`
-   `/var/log/karabiner`

---

## For developers

### How to build

System requirements to build Karabiner-Elements:

-   macOS 11+
-   Xcode 13.0 (You need to hold Xcode version to 13.0 because Xcode 13.1 generate binary which does not work on macOS 11 Big Sur.)
-   Command Line Tools for Xcode
-   [XcodeGen](https://github.com/yonaskolb/XcodeGen)

### Note

A provisioning profile which supports `com.apple.developer.driverkit` is required to build a driver extension since Xcode 12.

If you want to start without a valid provisioning profile, use Xcode 11 and
[Karabiner-DriverKit-VirtualHIDDevice v0.11.0](https://github.com/pqrs-org/Karabiner-DriverKit-VirtualHIDDevice/releases/tag/v0.11.0).

### Steps

1.  Gain the DriverKit entitlements to be able to create a provisioning profile which supports `com.apple.developer.driverkit`.
    Specifically, follow the instructions on [Requesting Entitlements for DriverKit Development](https://developer.apple.com/documentation/driverkit/requesting_entitlements_for_driverkit_development)

    Note: This process may take some time to complete on Apple's end.

    If you want to start without the request, use Xcode 11 and Karabiner-DriverKit-VirtualHIDDevice v0.11.0. (See above note)

2.  Create a Developer ID distribution provisioning profile for `org.pqrs.Karabiner-DriverKit-VirtualHIDDevice` with `com.apple.developer.driverkit` entitlement.

    <img src="docs/images/generate-a-provisioning-profile@2x.png" width="921" alt="Generate a Provisioning Profile" />

3.  Replace `CODE_SIGN_IDENTITY` at `src/scripts/codesign.sh` with yours.

    Find your codesign identity by executing the following command in Terminal.

    ```shell
    security find-identity -p codesigning -v
    ```

    The result is as follows.

    ```text
    1) 8D660191481C98F5C56630847A6C39D95C166F22 "Developer ID Application: Fumihiko Takayama (G43BCU2T37)"
    2) 6B9AF0D3B3147A69C5E713773ADD9707CB3480D9 "Apple Development: Fumihiko Takayama (YVB3SM6ECS)"
    3) 637B86ED1C06AE99854E9F5A5DCE02DA58F2BBF4 "Mac Developer: Fumihiko Takayama (YVB3SM6ECS)"
    4) 987BC26C6474DF0C0AF8BEA797354873EC83DC96 "Apple Distribution: Fumihiko Takayama (G43BCU2T37)"
        4 valid identities found
    ```

    Choose one of them (e.g., `8D660191481C98F5C56630847A6C39D95C166F22`) and replace existing `CODE_SIGN_IDENTITY` with yours as follows.

    ```shell
    # Replace with your identity
    readonly CODE_SIGN_IDENTITY=8D660191481C98F5C56630847A6C39D95C166F22
    ```

4.  Replace team identifier, domain and embedded.provisionprofile.

    -   Search `G43BCU2T37` and replace them with your team identifier.

        ```shell
        git grep G43BCU2T37 src/
        ```

    -   Search `org.pqrs` and `org_pqrs`, then replace them with your domain.

        ```shell
        git grep org.pqrs src/
        git grep org_pqrs src/
        ```

    -   Replace `embedded.provisionprofile` file with yours.

        ```shell
        find * -name 'embedded.provisionprofile'
        ```

5.  Build by the following command in terminal.

    ```shell
    make package
    ```

    `dist/Karabiner-DriverKit-VirtualHIDDevice-X.X.X.pkg` will be generated.

### Components

Karabiner-DriverKit-VirtualHIDDevice consists the following components.

-   Extension Manager (including DriverKit driver)
    -   `/Applications/.Karabiner-VirtualHIDDevice-Manager.app`
    -   It provides a command line interface to activate or deactivate DriverKit driver.
-   VirtualHIDDeviceClient
    -   `/Library/Application Support/org.pqrs/Karabiner-DriverKit-VirtualHIDDevice/Applications/Karabiner-DriverKit-VirtualHIDDeviceClient.app`
    -   It mediates between the client app and the driver.
    -   It allows apps to communicate with the virtual device even if the app is not signed with pqrs.org's code signing identity.
        (The client app must be running with root privileges.)
-   Client apps
    -   Client apps are not included in the distributed package.
    -   For example, you can build the client app from `examples/virtual-hid-device-service-client` in this repository.
    -   Client apps can send input events by communicating with VirtualHIDDeviceClient via UNIX domain socket.
        (`/Library/Application Support/org.pqrs/tmp/rootonly/vhidd_server/*.sock`)

![components.svg](./docs/plantuml/output/components.svg)

### Version files

-   `version`:
    -   Karabiner-DriverKit-VirtualHIDDevice package version.
    -   Increment when any components are updated.
-   `driver-version`:
    -   DriverKit driver internal version.
    -   Increment when the driver source code is updated.
