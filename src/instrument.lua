--
-- Copyright (c) 2013 Wenhao Li. All Rights Reserved.
--

local v = require("lvisa")

-- scpi data convert
local function scpi_data(s, rf)
  -- string
  if not rf then return s end

  -- number
  if rf == "*n" then
    return tonumber(s)
  end

  s = string.gsub(s, "%+", "")
  -- array
  if rf == "*a" then
    return load("return {"..s.."}")()
  end

  -- list
  if rf == "*l" then
    return load("return "..s)()
  end
end

-- instrument
local mt = nil
function instrument(addr, timeout)
  local instr = v.open(addr, timeout)

  if not mt then
    mt = getmetatable(instr)
    mt.w = function(self, s, ...)
      self:write(string.format(s, ...))
    end
    mt.query = function(self, s, rf, ...)
      self:write(string.format(s, ...))
      return scpi_data(self:read(), rf)
    end
    mt.r = mt.read
    mt.q = mt.query
  end

  return instr
end

