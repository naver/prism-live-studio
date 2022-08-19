# PRISM Live Studio
![LOGO](https://resource-prismlive.pstatic.net/202112161144/1585712652.png)

**Visit [our official web site](http://prismlive.com) for more information and [Latest updates on PRISM Live Studio](http://prismlive.com/ko_kr/pcapp/)**.

## About PRISM Live Studio
PRISM Live studio PC version helps beginners stream like professionals. It is a desktop application for live broadcasting.
With easy operation, anyone can easily make broadcasts and send it stably to various platforms.

PRISM Live Studio used the OBS engine as the core module. We would thank all the developers with their wonderful work of OBS project.

## Overview
![PRISM Live Studio](https://resource-prismlive.pstatic.net/202112161142/1639622512.png)

This application currently only supports 64-bit Windows.

## Build on Windows
Before build, please prepare install QT 5.12 version first and set the environment variables as:
```
QTDIR32: QT install directory/msvc2017
QTDIR64: QT install directory/msvc2017_64
```

1. Please enter in build/windows directory and implement:
```
>configure.cmd
```

2. Implement
```
>build.cmd
```

Or you could implement the following batch processing (contains step1 and step2)：
```
>autobuild_win.bat
```

Then, you could find a bin/Release/x64 directory generated under root.
And please find the file PRISMLiveStudio.exe
which is the main program file of PRISM Live Studio

And, if you wish to use Visual studio to open our project, please use VS2019 to open the solution under:
```
\src\prism\build\PRISMLiveStudio.sln
```
after the configure.cmd is implemented.
 
## Community

[Github issues](https://github.com/naver/prismlivestudio/issues)  
[Blog](https://blog.naver.com/prismlivestudio)  
If you have any question, please contact us by [mail:prismlive@navercorp.com](mailto://prismlive@navercorp.com)

## License
PRISM Live Stduio is licensed under the GNU GENERAL PUBLIC LICENSE, Version 2.0.
See [LICENSE](COPYING) for full license text.
