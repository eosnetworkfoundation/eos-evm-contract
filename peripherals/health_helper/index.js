var http = require('http');

const url = process.env.RPC_ENDPOINT || 'http://localhost:80'
const listen_port = process.env.LISTEN_PORT || 8000
const check_interval = process.env.CHECK_INTERVAL || 5000
const stale_threshold = process.env.STALE_THRESHOLD || 60

var last_block = 0;
var stale = 1;

function intervalFunc() {
    let post_data = '{"jsonrpc":"2.0","method":"eth_getBlockByNumber","params":["latest", false],"id":1}'
    const options = {
        hostname: url.hostname,
        port: url.port,
        path: '/',
        method: 'POST',
        headers: {
            'Content-Type': 'application/json',
            'Content-Length': post_data.length
        }
    }

    let post_req = http
        .request(options, response => {
            var data = "";

            response.on('data', function (chunk) {
                data += chunk;
            });

            response.on('end', function () {
                try {
                    const obj = JSON.parse(data);

                    last_block = obj.result.timestamp
                    let timestamp = Number(last_block)
                    let now = Math.floor(Date.now() / 1000)
                    if (now - timestamp > stale_threshold) {
                        stale = 1;
                    }
                    else {
                        stale = 0
                    }

                }
                catch (_) {
                    stale = 1;
                }
            })
        })
        .on("error", err => {
            console.log("Error: " + err.message);
            stale = 1;
        });

    post_req.write(post_data);
    post_req.end();
}

setInterval(intervalFunc, check_interval);

http.createServer(function (req, res) {
    res.writeHead(stale > 0 ? 500 : 200, { 'Content-Type': 'text/html' });
    res.write(last_block.toString());
    res.end();
}).listen(listen_port);

