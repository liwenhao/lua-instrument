
require("instrument")

local i = instrument("?*::5::INSTR")

i:write("*CLS")
i:write("*IDN?")
print(i:read())

i:w("*CLS")
i:w("*IDN?")
print(i:r())

i:w("*CLS")
i:w("XXX %f", 4.0)

local s,m = i:q("SYST:%s", "*l", "ERR?")
print("status:"..s)
print("message:"..m)

