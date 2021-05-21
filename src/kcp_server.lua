package.path = package.path..";./src/?.lua"

kcp = require "kcp"

function sleep(n)
  local t0 = os.clock()
  while os.clock() - t0 <= n do end
end

function start()
  local fd, err = socket.socket(socket.AF_INET, socket.SOCK_DGRAM, 0)
  fd:setblocking(false)
  local result, err = fd:bind("0.0.0.0", 20001)
  if not result then
    print("bind failed, err: ", err)
    return
  end
  local net
  while true do
    -- sleep(0.05)
    local data, ip, port = fd:recvfrom(4096)
    if not net and ip and port then
      net = kcp.bind(fd, ip, port)
      print("new conn:", ip, port)
    end

    if net then
      if data then
        net.kcp:lkcp_input(data)
      end

      package = net:read()
      if package then
        net:write(package)
      end

      net:update(socket.get_time_ms())
    end
  end
end

start()
