package.cpath = package.cpath..";./lualib/?.so"
package.path = package.path..";./src/?.lua"

local socket = require "socket.c"
local kcp = require "kcp"

local rtt = {}

-- local function sleep(n)
--   local t0 = os.clock()
--   while os.clock() - t0 <= n do end
-- end

local function start()
  local net = kcp.new("30.210.220.83", 20001)

  local total = 100   -- 发送package的个数
  local pkg_len = 500 -- 发送包的字节数
  local interval = 50 -- 每 50ms 发送一个包
  local pkg = 0
  local curack = 0
  local last_snd_ts
  repeat
    -- sleep(0.05)
    local now_ts = socket.get_time_ms()
    if pkg < total then
      if not last_snd_ts or now_ts - last_snd_ts >= interval then
        last_snd_ts = now_ts

        pkg = pkg + 1
        rtt[pkg] = {}
        rtt[pkg].start = now_ts
        local data = string.rep('a', pkg_len)
        data = data..pkg
        net:write(data)
      end
    end

    local package = net:read()
    if package then
      local ack = string.sub(package, pkg_len + 1)
      if ack then
        ack = tonumber(ack)
      end
      if ack and rtt[ack] then
        rtt[ack].ack = now_ts
      end
      if ack then
        curack = ack
      end

    end

    -- print("recv curack: ", curack)

    net:update(socket.get_time_ms())

  until curack >= total

  local maxrtt = 0
  local minrtt = 0xffffffff
  local totalrtt = 0
  for i = 1, total do
    if rtt[i] then
      local t = rtt[i].ack - rtt[i].start
      -- print("pkg:", i, ", cost:", rtt[i].ack - rtt[i].start)
      if t > maxrtt then
        maxrtt = t
      end
      if t < minrtt then
        minrtt = t
      end
      totalrtt = totalrtt + t
    end
  end
  local avgrtt = totalrtt / total
  print("min rtt:", minrtt)
  print("max rtt:", maxrtt)
  print("avg rtt:", avgrtt)

end

start()
