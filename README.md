# PRISM Live Studio
**Learn more about PRISM Live Studio and get the latest updates on our [official website](http://prismlive.com/ko_kr/pcapp/).**

## Releases

### PRISM Live Studio 6.0.0 Beta

[Download PRISM Live Studio 6.0.0 Beta for Windows]()

### PRISM Live Studio 6.0.0 Beta Release Notes

PRISM Live Studio 6.0.0 Beta is based on OBS Studio 32.2.2 and is currently available for Windows only.

**Before installing:** Installing the Beta replaces the existing PRISM Live Studio installation on the same PC. The two versions cannot be installed together. The previous version remains available for download from the PRISM website. B2B features are not yet available; B2B customers should continue using the existing version.

#### Changes

- Changed the OBS base version to 32.2.2 and adapted PRISM features accordingly.
- Changed the application interface and settings to follow OBS, with support for installing and using OBS plugins.
- Changed account and subscription management to use Account Hub, bringing existing account information, subscription details, and account actions into one place.
- Added Onboarding Guide to introduce the PRISM Sidebar, PRISM Streaming, and key feature entry points.
- Changed the Help menu structure to provide separate OBS Help and PRISM Help & Support menus.
- Changed the PRISM Sidebar layout and feature entry points to integrate with the OBS main window.
- Changed how PRISM Sources are added and edited: sources are added through Add Source, managed in the OBS Sources list, and edited through the PRISM Sidebar or property windows. Adapted sources include Text Template, Chat Overlay, Clock Widget, Audio Visualizer, Background Template, Viewer Count Widget, Mobile Connect, and Sticker.
- Changed the Source Toolbar to retain only the common Properties and Filters actions, removing PRISM-specific action buttons.
- Changed the Scene Template workflow to import templates into the current Scene Collection and apply them immediately.
- Changed the multistreaming and channel management workflow to integrate existing platform features into the PRISM Streaming Dock. Platform Live Info is managed through channel tabs in Manage Broadcast.
- Changed the Resolution Guide entry point from the Sidebar to Manage Broadcast.
- Changed Output Setting Guide to display recommended resolutions, FPS, and official platform guides together, with an option to apply the recommended settings.
- Changed the Lens workflow to provide face retouch, background removal, Color Filter, Funny Face, Advanced Camera Settings, and other effects through Lens Filter on camera sources within the application. The existing PRISM Lens application is retained, but its previous capture-device workflow is no longer supported in the Beta. Lens effects must be configured again within the Beta.
- Changed Blur integration to use the OBS Filter system.
- Changed Spout2 integration to use the OBS plugin system, providing Source, Output, Filter, and Tools menu entries.
- Added more theme options by incorporating OBS Themes alongside the existing PRISM theme.
- Changed the Contact Us form and feedback flow, adding separate routing for PRISM and OBS issues.
- Updated the Terms of Use and Privacy Policy links used during registration.
- The installer continues to support PRISM's seven languages, using the system language by default and English when the system language is unsupported.
- Added migration support for existing scenes, settings, channel lists, and some sources' My / Recent data. Unsupported sources cannot be migrated and are removed. Some Docks, including Chat and Control Panel, may require users to log in again.
- Removed system file association for `.psc` files. Double-clicking these files no longer opens PRISM Live Studio.


## Overview
![PRISM Live Studio](https://resource-prismlive.pstatic.net/20231221002/pc/img/live_stream_img1.png)

This application currently only supports 64-bit Windows.

## Build on Windows
Before build, please prepare install Visual Studio 2022 and QT 6.3.1 version first and set the enviroment variables as:
```
QT631: QT install directory/6.3.1/msvc2019_64
```

1. Please enter in build/windows directory and config project:
```
> configure.cmd
```

2. Please enter in src/prism-live-studio/build, and use VS2022 to open the solution under:
```
> open prism-live-studio.sln
```

Then, you could find a bin/prism/windows/Debug or bin/prism/windows/Release directory generated under root.
And please find the file PRISMLiveStudio.exe in bin/64bit
which is the main program file of PRISM Live Studio

## Build on Mac OS
Before build, please prepare install XCode and QT 6.3.1 version first and set the enviroment variables as:
```
QTDIR: QT install directory/6.3.1/macos
```

1. Please enter in build/mac directory and config project:
```
> ./01_configure.sh
```

2. Please enter in src/prism-live-studio/build, and use XCode to open the solution under:
```
> open prism-live-studio.xcodeproj
```

Then, you could find a src/prism-live-studio/build/prism-live-studio/PRISMLiveStudio/Debug/PRISMLiveStudio.app or src/prism-live-studio/build/prism-live-studio/PRISMLiveStudio/Release/PRISMLiveStudio.app,
which is the bundle file of PRISM Live Studio

## Community

[Github issues](https://github.com/naver/prismlivestudio/issues)  
[Blog](https://blog.naver.com/prismlivestudio)  
If you have any question, please contact us by [mail:prismlive@navercorp.com](mailto://prismlive@navercorp.com)
