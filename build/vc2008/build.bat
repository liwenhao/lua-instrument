@echo off

setlocal
set platf=Win32
set conf=Release

:CheckOpts
if "%1"=="-d" (set conf=Debug) & shift & goto CheckOpts

set cmd=vcbuild /useenv lvisa.sln "%conf%|%platf%"
echo %cmd%
%cmd%
