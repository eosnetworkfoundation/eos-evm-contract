const { Api, JsonRpc, RpcError } = require("eosjs");
const { JsSignatureProvider } = require("eosjs/dist/eosjs-jssig"); // development only
const fetch = require("node-fetch"); // node only; not needed in browsers
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

// Setting up EOS
const rpc = new JsonRpc(process.env.EOS_RPC, { fetch });
const defaultPrivateKey = process.env.EOS_KEY;
const signatureProvider = new JsSignatureProvider([defaultPrivateKey]);

const api = new Api({
  rpc,
  signatureProvider,
  textDecoder: new TextDecoder(),
  textEncoder: new TextEncoder(),
});

// EOS Helpers
async function push_tx(strRlptx) {
  console.log('----rlptx-----');
  console.log(strRlptx);
  const result = await api.transact(
    {
      actions: [
        {
          account: process.env.EOS_EVM_ACCOUNT,
          name: "pushtx",
          authorization: [{
              actor      : process.env.EOS_SENDER,
              permission : "active",
            }
          ],
          data: {
            ram_payer : process.env.EOS_SENDER,
            rlptx     : strRlptx
          },
        },
      ],
    },
    {
      blocksBehind: 3,
      expireSeconds: 3000,
    }
  );
  console.log('----response----');
  console.log(result);
  return result;
}

// RPC Handlers
async function eth_sendRawTransaction(params) {
  const rlptx = params[0].substr(2);
  await push_tx(rlptx);
  return '0x'+keccak256(Buffer.from(rlptx, "hex")).toString("hex");
}

async function eth_gasPrice(params) {
  // TODO: get price from somewhere
  return "0x2540BE400";
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

// Main loop
rpcServer.listen(+process.env.PORT, process.env.HOST).then(() => {
  console.log(
    "server is listening at " + process.env.HOST + ":" + process.env.PORT
  );
});
