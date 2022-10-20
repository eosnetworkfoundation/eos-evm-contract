import Web3 from 'web3';
import { args } from './cli.mjs';
import assert from 'node:assert';
import { spawn, spawnSync } from 'node:child_process';
import os from 'node:os';
import fs from 'node:fs';
import process from 'node:process';
import dns from 'node:dns';

import proxy from './TrustEVMStaking.json' assert {type: 'json'};

const minimumStakeUnit = Web3.utils.unitMap['szabo'];
const zeroAddress = "0x0000000000000000000000000000000000000000";
const pollMS = 1000;

const argv = args();
assert(Web3.utils.isAddress(argv.address), "contract address is not a valid address");

const web3 = new Web3(argv.rpc);
const proxyContract = new web3.eth.Contract(proxy, argv.address);

//an array of objects, {address: string, url: string, weight: string}
let currentStakers = [];
let nginxProcess = null;
const configFilePath = `${os.tmpdir}/evmnginx-${process.pid}.conf`;

function createConfig(stakers) {
   let configTemplate = fs.readFileSync(argv.config, { encoding: 'utf8' });
   let serversString = "";
   if (stakers.length === 0)
      serversString = `server ${argv.fallback};   # (fallback)\n`;
   else
      stakers.forEach(staker => serversString += `server ${staker.url} weight=${staker.weight};   # ${staker.address}\n`);
   configTemplate = configTemplate.replaceAll('$STAKERS', serversString);
   configTemplate += '\ndaemon off;\n';
   fs.writeFileSync(configFilePath, configTemplate);
}

async function getStakers() {
   const bnum = await web3.eth.getBlockNumber();
   const STAKER_ROLE = await proxyContract.methods.STAKER_ROLE().call({}, bnum);
   const allowCount = await proxyContract.methods.getRoleMemberCount(STAKER_ROLE).call({}, bnum);
   let newConfig = [];

   let queryPromises = [];
   for (let i = 0; i < allowCount; i++) {
      const addr = await proxyContract.methods.getRoleMember(STAKER_ROLE, i).call({}, bnum);
      if (addr === zeroAddress)
         continue;
      queryPromises.push(Promise.all([addr,
                                      proxyContract.methods.getUpstreamUrl(addr).call({}, bnum),
                                      proxyContract.methods.getAmount(addr).call({}, bnum)]));
   }

   for (const queryResult of await Promise.allSettled(queryPromises)) {
      let entryAddr = "unknown";
      try {
         assert(queryResult.status === 'fulfilled', "Query promise failed");
         let [entryAddr, entryUrl, entryStaked] = queryResult.value;

         const parsedUrl = entryUrl.match(/https:\/\/(?<url>[0-9a-zA-Z.-]+):?(?<port>[0-9]+)?\//);
         assert(parsedUrl, `unable to parse URL`);
         const resolvedAddress = await dns.promises.lookup(parsedUrl.groups.url, { family: 4 });

         const stakedBN = web3.utils.toBN(entryStaked);
         const stakedUnitsBN = stakedBN.div(web3.utils.toBN(minimumStakeUnit));
         assert(!stakedUnitsBN.isZero(), "less than minimum stake amount staked");

         newConfig.push({ "address": entryAddr, "url": `${resolvedAddress.address}:${parsedUrl.groups.port ?? '443'}`, "weight": stakedUnitsBN.toString() });
      }
      catch (e) {
         console.error(`skipping ${entryAddr} due to error ${e}`);
      }
   }

   return newConfig;
}

function populateNginxConfig(stakers) {
   createConfig(stakers);
   //test config first...
   assert(spawnSync(argv.nginx, ['-t', '-c', configFilePath], { stdio: "inherit" }).status === 0, "testing new config failed");
   if (nginxProcess?.pid)
      process.kill(nginxProcess.pid, 'SIGHUP');
}

async function tick() {
   try {
      const newStakers = await getStakers();
      if (JSON.stringify(currentStakers) !== JSON.stringify(newStakers)) {
         populateNginxConfig(newStakers);
         currentStakers = newStakers;
      }
   }
   catch (e) {
      console.error(`Something failed during tick update ${e}`);
   }
   setTimeout(tick, pollMS);
}

currentStakers = await getStakers();
populateNginxConfig(currentStakers);

let nginxProcessPromise = new Promise(resolve => {
   let nginxArgs = ['-c', configFilePath];
   if (argv.e)
      nginxArgs.push(['-e', argv.e]);
   if (argv.g)
      nginxArgs.push(['-g', argv.g]);
   if (argv.p)
      nginxArgs.push(['-p', argv.p]);
   nginxProcess = spawn(argv.nginx, nginxArgs, { stdio: "inherit" }).on('error', (err) => { throw err; }).on('close', code => resolve());
});

process.on('SIGTERM', signal => process.kill(nginxProcess.pid, signal));
process.on('SIGHUP', () => {
   try {
      populateNginxConfig(currentStakers);
   } catch (e) {
      console.error(`Failed to reload config`);
   }
});
setTimeout(tick, pollMS);

await nginxProcessPromise;
fs.unlinkSync(configFilePath);
process.exit(nginxProcess.exitCode ?? 100);
