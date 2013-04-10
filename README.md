hxpad
=====

GamePad support for Haxe

This is a small Windows program than will use DirectX to capture gamepad buttons and movements. It will also open a local socket on which you can connect to get the events from your application.

The Pad class is written in Haxe and uses the Flash API (and a bit of AIR to automatically start pad.exe)

Feel free to reuse or modify as you wish.

About Dependencies
------------------

Requires dinput8.dll installed on user computer.

If you distribute dinput8.dll with it:
  - using Win7 dinput8.dll might not work on XP unless you also add Win7 msvcrt.dll
  - it seems that the native Win8 dinput8.dll will crash with X360 controllers

The best is then to either ship a XP version of dinput8.dll or a Win7 version together with msvcrt.dll

The 'pad.exe' file can be recompiled using VC++ Express 2010, I have included dxguid.lib which does not seem to be distributed together with it (saves 600MB of DirectX SDK download).
