local cjson = require('cjson')

local function empty(s)
  return s == nil or s == ''
end

local function split(s)
  local res = {}
  local i = 1
  for v in string.gmatch(s, "([^,]+)") do
    res[i] = v
    i = i + 1
  end
  return res
end

local function contains(arr, val)
  for i, v in ipairs (arr) do
    if v == val then
      return true
    end
  end
  return false
end

-- parse conf
local write_calls = nil
if not empty(ngx.var.jsonrpc_write_calls) then
  write_calls = split(ngx.var.jsonrpc_write_calls)
end

-- parse conf
local blacklist = nil
if not empty(ngx.var.jsonrpc_blacklist) then
  blacklist = split(ngx.var.jsonrpc_blacklist)
end

-- get request content
ngx.req.read_body()

local body_data = ngx.req.get_body_data();
if empty(body_data) then
  local file, err = io.open(ngx.req.get_body_file(), 'r')
  if not file then
    ngx.log(ngx.ERR, 'Unable to read body data')
    ngx.exit(ngx.HTTP_BAD_REQUEST)
    return
  end
  body_data = file:read()
  io.close(file)
end

-- try to parse the body as JSON
local success, body = pcall(cjson.decode, body_data);
if not success then
  ngx.log(ngx.ERR, 'invalid JSON request')
  ngx.exit(ngx.HTTP_BAD_REQUEST)
  return
end

local method = body['method']
local version = body['jsonrpc']

-- check we have a method and a version
if empty(method) or empty(version) then
  ngx.log(ngx.ERR, 'no method and/or jsonrpc attribute')
  ngx.exit(ngx.HTTP_BAD_REQUEST)
  return
end

-- check the version is supported
if version ~= "2.0" then
  ngx.log(ngx.ERR, 'jsonrpc version not supported: ' .. version)
  ngx.exit(ngx.HTTP_INTERNAL_SERVER_ERROR)
  return
end

-- if blacklist is configured, check that the method is not blacklisted
if blacklist ~= nil then
  if contains(blacklist, method) then
    ngx.exit(ngx.HTTP_FORBIDDEN)
    return
  end
end

-- Proxy certain calls to "write" and rest to "read"
ngx.var.proxy = 'read'
if write_calls ~= nil then
  if contains(write_calls, method) then
    ngx.var.proxy = 'write'
    return
  end
end

return
