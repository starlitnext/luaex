package.cpath = package.cpath..";./lualib/?.so"

local test01 = require "demo01"

local function test()
  print("1+2="..test01.add(1, 2))
  print("1-2"..test01.sub(1, 2))
end

test()