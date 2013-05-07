-- GSM Test
--
-- Copyright (c) 2013 Wenhao Li. All Rights Reserved.
--
--
require("instrument")

local mock = false
local bs = nil
if not mock then
  bs = instrument("?*::14::INSTR")
else
  local mt = {}
  mt.q = function(self, fmt, t, ...)
    print(string.format(fmt,...))
    if t == "*l" then
      return 0, 0, 0, 0
    end
    if t == "*n" then
      return 1
    end
    if t == nil then
      return "CONN"
    end
  end

  mt.w = function(self, fmt, ...)
    print(string.format(fmt,...))
  end
  mt.__index = mt
  bs = {}
  setmetatable(bs, mt)
end

local dloss = 0.5
local uloss = 0.5
local level = -82
local ber_level = -102.0

local function range(s, e, o)
  local t = o or {}
  for i=s,e do
    table.insert(t, i)
  end
  return t
end

local function verdict(pass, result, ...)
  local v
  if pass then
    v = "PASS"
  else
    v = "FAIL"
  end
  print(string.format(result.." "..v, ...))
end

local EGSM = {
  band = "EGSM",
  bch = 20,
  tch = 37,
  channels = {975,37,124}, -- range(975, 1023, range(0, 124)),
  levels = {5,11,19}, -- range(5, 19)
}

local GSM850 =  {
  band = "GSM850",
  bch = 160,
  tch = 190,
  channels = {128,190,251}, -- range(128, 251),
  levels = {5,11,19}, -- range(5, 19)
}

local DCS =  {
  band = "DCS",
  bch = 600,
  tch = 698,
  channels = {512,698,885}, -- range(512, 885),
  levels = {0,8,15}, -- range(0, 15)
}

local PCS = {
  band = "PCS",
  bch = 600,
  tch = 660,
  channels = {512,660,810}, -- range(512, 810),
  levels = {0,8,15}, -- range(0, 15)
}

local bands = {EGSM, DCS, PCS, GSM850}

-- init bs
local function bs_init()
  bs:w("*RST")
  bs:w("*CLS")
  -- GSM mode
  bs:w("SYST:APPL:FORM 'GSM/GPRS'")
  bs:w("CALL:OPER:MODE OFF")
  bs:w("CALL:BCH:SCELl GSM")
  bs:w("CALL:OPER:MODE CALL")
  bs:w("RFG:OUTP IO")
  bs:w("CALL:TCH:TSL 4")
  bs:w("CALL:TCH:CMOD FRSP")
  bs:w("CALL:TCH:DOWN:SPE PRBS15")
  -- set cable loss
  bs:w("SYST:CORR:FREQ 880.2MHz, 934.8MHz")
  bs:w("SYST:CORR:GAIN %.1f, %.1f", -uloss, -uloss)
  -- set power
  bs:w("RFAN:CONT:POW:AUTO ON")
  bs:w("CALL:BURS:TYPE TSC5")
  bs:w("CALL:POW:STAT ON")
  bs:w("CALL:POW:AMPL %d", level)

  bs:w("CALL:PAG:REP OFF")
  bs:w("CALL:PAG:IMSI '001012345678901'")
  -- measurement setup
  -- PFER
  bs:w("SET:PFER:CONT OFF")
  bs:w("SET:PFER:COUN:SNUM %d", 20)
  bs:w("SET:PFER:TIM:STIM 10")
  bs:w("SET:PFER:TRIG:SOUR AUTO")
  bs:w("SET:PFER:SYNC MID")
  -- PVT
  bs:w("SET:PVT:CONT OFF")
  bs:w("SET:PVT:COUN:SNUM %d", 5)
  bs:w("SET:PVT:TRIG:SOUR AUTO")
  bs:w("SET:PVT:SYNC MID")
  bs:w("SET:PVT:LIM:ETSI:PCS REL")
  -- FBER
  bs:w("SET:FBER:TIM:TIME 10")
  bs:w("SET:FBER:CONT OFF")
  bs:w("SET:FBER:COUN %d", 30000)
  bs:w("SET:FBER:CLSD:STIM 500 MS")
  bs:w("SET:FBER:LDC:AUTO ON")
  -- complete?
  bs:q("*OPC?")
end

local function bs_connect()
  while bs:q("CALL:CONN:STAT?", "*n") ~= 1 do
	print("MAKE A MS ORGINATED CALL.")
	print("PRESS ANY KEY TO CONTINUE.")
	io.read()
  end
  print(bs:q("CALL:MS:REP:IMEI?"))
end

local function bs_end()
  bs:w("CALL:END")
  local s = bs:q("CALL:CONN:STAT?")
  print(s)
end

local function test_modulation()
  bs:w("INIT:PFER")
  -- fetch
  local s,rms,peak,freq = bs:q("FETC:PFER:ALL?", "*l")
  -- check
  verdict(s==0, "FPER FreqError=%.1fkHz, PeakPhaseError=%.2f, RMSPhaseError=%.2f", freq, peak, rms)
end

local function test_power()
  bs:w("INIT:PVT")
  -- fetch
  local power, mask
  power = bs:q("FETCH:PVT:TXP:AVER?", "*n")
  mask = bs:q("FETCH:PVT:MASK?", "*n")
  -- check
  verdict(mask==0, "PVT AvgBurstPower=%.2fdB", power)
end

local function test_ber()
  local spec = 2
  bs:w("CALL:POW:AMPL %d", ber_level)
  bs:w("INIT:FBER")
  -- fetch
  local count = bs:q("FETCh:FBER:BITS?", "*n")
  local ber = bs:q("FETCH:FBER:RAT?", "*n")
  -- check
  verdict(ber<2, "BER %.2f%%", ber)
end

local tests = {test_modulation,	test_power, test_ber}

local function test_band(b)
  bs:w("CALL:BAND %s", b.band)
  bs:w("CALL:BCH:%s %d", b.band, b.bch)
  for _,tch in ipairs(b.channels) do
    if tch ~= b.bch then
      print(string.format("ARFCN=%d", tch))
      bs:w("CALL:TCH:%s %d", b.band, tch)
      bs:w("CALL:POW:AMPL %d", level)
      bs:w("CALL:HAND")
	  while not string.match(bs:q("CALL:STAT:STAT?"), "^CONN.*") do
		print("Connection is lost")
		bs_connect()
      end
      for _,pcl in ipairs(b.levels) do
	print(string.format("PCL=%d", pcl))
	bs:w("CALL:MS:TXL:%s %d", b.band, pcl)
	for _,test in ipairs(tests) do
	  test()
	end
      end
    end
  end
end

------------------------------------------------------------
-- main
------------------------------------------------------------
bs_init()
bs_connect()
for _,b in ipairs(bands) do
  print(string.format("BAND=%s", b.band))
  test_band(b)
end
bs_end()
