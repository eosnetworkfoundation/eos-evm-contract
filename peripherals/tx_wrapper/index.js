const { Api, JsonRpc, RpcError } = require("eosjs");
const { JsSignatureProvider } = require("eosjs/dist/eosjs-jssig"); // development only
const fetch = require("node-fetch"); // node only; not needed in browsers
//const fetch = (...args) => import('node-fetch').then(({default: fetch}) => fetch(...args));
const { TextEncoder, TextDecoder } = require("util"); // node only; native TextEncoder/Decoder

const RpcServer = require("http-jsonrpc-server");
const dotenv = require("dotenv");
const isValidHostname = require('is-valid-hostname')

const { keccak256 } = require('ethereumjs-util');

// Local helpers
function validateNum(input, min, max) {
  var num = +input;
  return num >= min && num <= max && input === num.toString();
}

// Read and Validate Configs
dotenv.config();

if (!process.env.EOS_KEY) {
  console.log("Missing EOS_KEY in .env file!");
  process.exit();
}

if (!process.env.EOS_SENDER) {
  console.log("Missing EOS_SENDER in .env file!");
  process.exit();
}

if (!process.env.EOS_PERMISSION) {
  console.log("Missing EOS_PERMISSION in .env file!");
  process.exit();
}

if (!process.env.EOS_RPC) {
  console.log("Missing EOS_RPC in .env file!");
  process.exit();
}

if (!process.env.EOS_EVM_ACCOUNT) {
  console.log("Missing EOS_EVM_ACCOUNT in .env file!");
  process.exit();
}

// Note: the check is based on RFC-1123, some strange name such as 127.0.0.1abc can actually pass the test.
if (!process.env.HOST || !isValidHostname(process.env.HOST)) {
  console.log("Missing or invalid HOST in .env file!");
  process.exit();
}

if (!process.env.PORT || !validateNum(process.env.PORT, 1, 65535)) {
  console.log("Missing or invalid PORT in .env file!");
  process.exit();
}

expire_sec = 300;
if (!process.env.EXPIRE_SEC) {
  console.log("Missing EXPIRE_SEC, default to " + expire_sec);
} else {
  expire_sec = +process.env.EXPIRE_SEC;
}

// Setting up EOS
rpc_list = process.env.EOS_RPC.split("|");
console.log("number of RPC endpoints = " + rpc_list.length + ", using " + rpc_list[0]);
rpc_index = 0;

rpc = new JsonRpc(rpc_list[rpc_index], { fetch });
const defaultPrivateKey = process.env.EOS_KEY;
const signatureProvider = new JsSignatureProvider([defaultPrivateKey]);

api = new Api({
  rpc,
  signatureProvider,
  textDecoder: new TextDecoder(),
  textEncoder: new TextEncoder(),
});

function next_rpc_endpoint() {
  rpc_index = (rpc_index + 1) % rpc_list.length;
  console.log("changing RPC endpoint to " + rpc_list[rpc_index]);
  rpc = new JsonRpc(rpc_list[rpc_index], { fetch });
  api = new Api({
    rpc,
    signatureProvider,
    textDecoder: new TextDecoder(),
    textEncoder: new TextEncoder(),
  });
}

// EOS Helpers
var pushcount=0;
async function push_tx(strRlptx) {
  id=pushcount;
  pushcount = pushcount + 1;
  console.log("----rlptx(" + id + ")-----");
  console.log(strRlptx);
  t0 = Date.now();
  const result = await api.transact(
    {
      actions: [
        {
          account: process.env.EOS_EVM_ACCOUNT,
          name: "pushtx",
          authorization: [{
              actor      : process.env.EOS_SENDER,
              permission : process.env.EOS_PERMISSION,
            }
          ],
          data: {
            miner : process.env.EOS_SENDER,
            rlptx : strRlptx
          },
        },
      ],
    },
    {
      blocksBehind: 3,
      expireSeconds: +expire_sec,
    }
  );
  latency = Date.now() - t0;
  console.log("----response(" + id + ", " + latency + "ms) ----");
  console.log(result);
  return result;
}

// RPC Handlers
async function eth_sendRawTransaction(params) {
  const rlptx = params[0].substr(2);
  await push_tx(rlptx);
  return '0x'+keccak256(Buffer.from(rlptx, "hex")).toString("hex");
}

var lastGetTableCallTime = 0
var gasPrice = "0x1";
async function eth_gasPrice(params) {
  if ( (new Date() - lastGetTableCallTime) >= 1000 ) {
    try {
      const result = await rpc.get_table_rows({
        json: true,                // Get the response as json
        code: process.env.EOS_EVM_ACCOUNT,      // Contract that we target
        scope: process.env.EOS_EVM_ACCOUNT,     // Account that owns the data
        table: 'config',           // Table name
        limit: 1,                  // Maximum number of rows that we want to get
        reverse: false,            // Optional: Get reversed data
        show_payer: false          // Optional: Show ram payer
      });
      console.log("result:", result.rows[0].gas_price);
      gasPrice = "0x" + parseInt(result.rows[0].gas_price).toString(16);
      lastGetTableCallTime = new Date();
    } catch(e) {
      console.log("Error getting gas price from nodeos: " + e);
      next_rpc_endpoint();
    }
  }
  return gasPrice;
}

function zero_pad(hexstr) {
  if(!hexstr) return "";
  const res = hexstr.substr(2);
  return res.length % 2 ? '0'+res : res;
}

// Setting up the RPC server
const rpcServer = new RpcServer({
  path: "/",
  methods: {
    eth_sendRawTransaction,
  },
  onRequest: (request) => {
    console.debug(JSON.stringify(request));
  },
  onRequestError: (err, id) => {
    console.error("request " + id + " threw an error: " + err);
  },
  onServerError: (err) => {
    console.error("the server threw an error: " + err);
  },
});

rpcServer.setMethod("eth_sendRawTransaction", eth_sendRawTransaction);
rpcServer.setMethod("eth_gasPrice", eth_gasPrice);

process.on('SIGTERM', function onSigterm () {
  console.info('Got SIGTERM. Graceful shutdown start', new Date().toISOString())
  shutdown();
})

function shutdown() {
  rpcServer.close(function onServerClosed (err) {
    if (err) {
      console.error(err)
    }
    process.exit()
  })
}

// Main loop
rpcServer.listen(+process.env.PORT, process.env.HOST).then(() => {
  console.log(
    "server is listening at " + process.env.HOST + ":" + process.env.PORT
  );
});

