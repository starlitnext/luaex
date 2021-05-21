package.cpath = package.cpath..";./lualib/?.so"

local socket = require "socket.c"

-- function sleep(n)
--   local t0 = os.clock()
--   while os.clock() - t0 <= n do end
-- end

local function start()
  local fd = socket.socket(socket.AF_INET, socket.SOCK_DGRAM, 0)
  fd:setblocking(false)
  local result, err = fd:bind("0.0.0.0", 20001)
  if not result then
    print("bind failed, err: ", err)
    return
  end
  while true do
    -- sleep(0.05)
    local data, ip, port = fd:recvfrom(4096)
    if data then
      fd:sendto(ip, port, data)
    end
  end
end

start()

