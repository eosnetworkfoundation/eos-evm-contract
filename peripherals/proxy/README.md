# evm_proxy_nginx

Sample config:
```
location / {
  set $jsonrpc_write_calls 'eth_sendRawTransaction';
  set $jsonrpc_blacklist 'eth_mining';
  access_by_lua_file 'eth-jsonrpc-access.lua';
  proxy_pass http://$proxy;
}
```
