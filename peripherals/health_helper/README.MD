## Health check helper

This tiny script will keep getting last block from the node and compare the timestamp with current time. If the time difference is larger than certain threshold, the node is considered stale.

The scipt will response to http GET calls and return 200 if the node is healthy and 500 if the node is stale.


### To run

The script is simple enough so no npm install is necessary.
```
RPC_ENDPOINT=http://xx.xx.xx.xx:xx node index.js 
```

### Environment variables
RPC_ENDPOINT: Endpoint of the node. No default value. Required.

LISTEN_PORT: Port to listen and report state. Default to 8080.

CHECK_INTERVAL: How often should the helper query last block (in ms). Default to 5000.

STALE_THRESHOLD: The time difference threshold for a node to be considered stale (in second). Default to 60.
