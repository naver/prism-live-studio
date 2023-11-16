@echo off

setlocal
cd %~dp0

DistributionTool.exe -b -i .\com.naver.prism.sdPlugin\ -o .\com.naver.prism.sdPlugin\
pause