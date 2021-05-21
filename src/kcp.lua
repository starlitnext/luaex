package.cpath = package.cpath..";./lualib/?.so"

socket = require "socket.c"
lkcp = require "lkcp"

local sndwnd = 2048
local rcvwnd = 2048
local nodelay = 1       -- 是否启用 nodelay模式，0不启用；1启用
local interval = 10     -- 协议内部工作的 interval，单位毫秒
local resend = 2        -- 快速重传模式，0关闭，可以设置2（2次ACK跨越将会直接重传）
local nc = 1            -- 是否关闭流控，0代表不关闭，1代表关闭

local mt = {}
mt.__index = mt

local M = {}

function M.new(ip, port)
  local fd = socket.socket(socket.AF_INET, socket.SOCK_DGRAM, 0)
  fd:setblocking(false)
  local errcode = fd:connect(ip, port)
  local conv = math.random(0xffffffff)
  local kcp = lkcp.lkcp_create(conv, function(buf)
    fd:send(buf)
  end)
  kcp:lkcp_nodelay(nodelay, interval, resend, nc)
  kcp:lkcp_wndsize(sndwnd, rcvwnd)

  local obj = {
    v_fd = fd,
    kcp = kcp,
    conv = conv,
  }
  return setmetatable(obj, mt), errcode
end

function M.bind(fd, ip, port)
  local errcode = fd:connect(ip, port)
  local conv = math.random(0xffffffff)
  local kcp = lkcp.lkcp_create(conv, function(buf)
    fd:send(buf)
  end)
  kcp:lkcp_nodelay(nodelay, interval, resend, nc)
  kcp:lkcp_wndsize(sndwnd, rcvwnd)

  local obj = {
    v_fd = fd,
    kcp = kcp,
    conv = conv,
  }
  return setmetatable(obj, mt), errcode
end

function mt:flush_recv()
  local fd = self.v_fd
::CONTINUE::
  local data, err = fd:recv(4096)
  if not data then
    if err == socket.EAGAIN or err == 0 then
      return true
    elseif err == socket.EINTR then
      goto CONTINUE
    else
      return false, ""
    end
  elseif #data == 0 then
    return false, "connect_break"
  else
    self.kcp:lkcp_input(data)
  end
end

function mt:write(data)
  self.kcp:lkcp_send(data)
end

function mt:read()
  local sz, buf = self.kcp:lkcp_recv()
  return buf
end

function mt:update(msnow)
  self:flush_recv()
  self.kcp:lkcp_update(msnow)
end

return M
