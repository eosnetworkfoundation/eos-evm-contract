#!/usr/bin/env -S npx tsx@4.7.1

import * as Web3 from 'web3';
import * as Web3Accounts from 'web3-eth-accounts';
import {signTransaction, Transaction} from 'web3-eth-accounts';

async function main() {
    const privateKey = '0x4bbbf85ce3377467afe5d46f804f221813b2bb87f24d81f60f1fcdbf7cbf4356';
    let account = Web3Accounts.privateKeyToAccount(privateKey);
    console.log(account.address);
    let trx = new Transaction({
        to: '0x3787b98fc4e731d0456b3941f0b3fe2e01439961',
        value: 1,
        gasLimit: `0x${(1000000000).toString(16)}`,
        gasPrice: `0x${(150000000000).toString(16)}`,
        nonce: 0,
    }, {common: Web3Accounts.Common.custom({chainId: 15555})});
    let signature = await signTransaction(trx, privateKey);
    console.log(signature);
}

main();