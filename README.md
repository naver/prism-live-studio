# PRISM Live Studio
![LOGO](https://resource-prismlive.pstatic.net/202112161144/1585712652.png)

**Visit [our official web site](http://prismlive.com) for more information and [Latest updates on PRISM Live Studio](http://prismlive.com/ko_kr/pcapp/)**.

## About PRISM Live Studio
PRISM Live studio PC version helps beginners stream like professionals. It is a desktop application for live broadcasting.
With easy operation, anyone can easily make broadcasts and send it stably to various platforms.

PRISM Live Studio used the OBS engine as the core module. We would thank all the developers with their wonderful work of OBS project.

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

## Build on Macos
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
