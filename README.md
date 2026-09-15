# PRISM Live Studio

**Visit [our official web site](http://prismlive.com) for more information and [Latest updates on PRISM Live Studio](http://prismlive.com/ko_kr/pcapp/)**.

## Releases

### PRISM Live Studio 5.1.3

[Download PRISM Live Studio 5.1.3 for Windows](https://resource-prismlive.pstatic.net/202607072241/PrismLiveStudio_Setup_x64_5.1.3.719.exe)

- 5.1.3 New Features
 
  - Added [feature description].
  - Added [feature description].
 
    
- Improvements
  - Improved streaming stability.
  - Improved application performance and usability.
  - Improved compatibility with supported streaming platforms.

- Bug Fixes

  - Fixed an issue where [problem description].
  - Fixed an issue where [problem description]. 
  - Fixed several crashes and stability issues.



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
