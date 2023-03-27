# evm_proxy_nginx

Sample config for location:
```
location / {
  set $jsonrpc_write_calls 'eth_sendRawTransaction';
  set $jsonrpc_blacklist 'eth_mining';
  access_by_lua_file 'eth-jsonrpc-access.lua';
  proxy_pass http://$proxy;
}
```

**The access log and error log are directed to stdout and sterr by default.**

To build
```
sudo docker build -t evm/tx_proxy .
```

To run
```
mkdir -p logs

sudo docker run -p 80:80 -v ${PWD}/nginx.conf:/usr/local/openresty/nginx/conf/nginx.conf evm/tx_proxy:latest > ./logs/access.log 2>./logs/error.log &
```

Or use -d instead of the & approach. Make sure logs are handled properly in that case.