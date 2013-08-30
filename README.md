lua-instrument
==============

lua-instrument is a lua module for instrument controlling through VISA interface.

## To build:

#### Tool:

    vc2008 or higher

#### Dependencies:

    * lua library
    * visa library

Put lua and visa library into external directory in a structure
described in [external README](external/README).

#### Build:

Run `build/vc2008/build.bat` or open `build/vc2008/lvisa.sln`

## To Run:

Make sure `lvisa.dll` and `instrument.lua` in lua's package search path.
Try same simple code:

```lua
require("instrument")

local i = instrument("?*::5::INSTR")

print(i:q("*IDN?"))

local s,m = i:q("SYST:ERR?", "*l")
if s ~= 0 then
  print("bad status: "..m)
end
```

## Downloads
[Demo for win32](http://liwenhao.github.io/lua-instrument/downloads/demo.zip)

## TODO:

    Add API document.
