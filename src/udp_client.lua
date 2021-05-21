package.cpath = package.cpath..";./lualib/?.so"

local socket = require "socket.c"

local rtt = {}

-- local function sleep(n)
--   local t0 = os.clock()
--   while os.clock() - t0 <= n do end
-- end

local function start()
  local fd = socket.socket(socket.AF_INET, socket.SOCK_DGRAM, 0)
  fd:setblocking(false)
  local err = fd:connect("30.210.220.83", 20001)
  print("connect err:", err)

  local total = 100   -- 发送package的个数
  local pkg_len = 500 -- 发送包的字节数
  local interval = 50 -- 每 50ms 发送一个包
  local pkg = 0
  local curack = 0
  local last_snd_ts
  local rcv_idx = 0
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
        fd:send(data)
      end
    end

    local package = fd:recv(1024)
    if package then
      local ack = string.sub(package, pkg_len + 1)
      if ack then
        ack = tonumber(ack)
      end
      if ack and rtt[ack] then
        rtt[ack].ack = now_ts
        rtt[ack].rcv_idx = rcv_idx
        rcv_idx = rcv_idx + 1
      end
      if ack then
        curack = ack
      end

    end

    -- print("recv curack: ", curack)

  until curack >= total or now_ts - last_snd_ts >= 1000

  for i = 1, total do
    if rtt[i] then
      if rtt[i].ack then
        print("pkg:", i, " rtt:", rtt[i].ack - rtt[i].start, " rcv_idx:", rtt[i].rcv_idx)
      else
        print("pkg:", i, " drop ack")
      end
    end
  end

end

start()
