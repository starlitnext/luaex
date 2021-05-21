package.cpath = package.cpath..";./lualib/?.so"

local socket = require "socket.c"

for k, v in pairs(socket) do
  print(k, v)
end
