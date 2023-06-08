# evm_proxy_nginx

Sample config for location:
```
location / {
  set $jsonrpc_write_calls 'eth_sendRawTransaction,eth_gasPrice';
  set $jsonrpc_read_calls 'xxx,xxx';
  set $jsonrpc_test_calls 'xxx';
  access_by_lua_file 'eth-jsonrpc-access.lua';
  proxy_pass http://$proxy;
}
```

To build (You can change the endpoints through build-arg):
```
sudo docker build -t evm/tx_proxy --build-arg WRITE_ENDPOINT=host.docker.internal:18888 --build-arg READ_ENDPOINT=host.docker.internal:8881 --build-arg TEST_ENDPOINT=host.docker.internal:8882 .
```

To run:
```
mkdir -p logs

sudo docker run --add-host=host.docker.internal:host-gateway -p 80:80 -v ${PWD}/logs:/var/log/nginx -d --restart=always --name=tx_proxy evm/tx_proxy

```

