import { assert, assertAsync, assertAsyncThrow, HTTP_API, sleep } from '@ultraos/ultratest/apis/testApi';
import { SignerLibTransactResponse, UltraTest, UltraTestAPI } from '@ultraos/ultratest/interfaces/test';
import { UltraAPIv2, ultraStartup } from 'ultratest-ultra-startup-plugin';
import { ultraContracts } from 'ultratest-ultra-contracts-plugin';
import { genesis } from 'ultratest-genesis-plugin/genesis';
import { system, SystemAPI } from 'ultratest-system-plugin/system';
import { RequiredContract } from 'ultratest-ultra-startup-plugin/interfaces';
import { AccountConfiguration } from 'ultratest-ultra-startup-plugin/ultraStartupPermissions';
import * as Web3 from 'web3';
import * as Web3Accounts from 'web3-eth-accounts';
import {signTransaction, Transaction} from 'web3-eth-accounts';
import evmSetup from './evm_setup';

export default class Test extends UltraTest {
    constructor() {
        super();
    }

    async onChainStart(ultra: UltraTestAPI) {
        ultra.addPlugins([genesis(ultra), system(ultra), ultraContracts(ultra), await ultraStartup(ultra), evmSetup()]);
    }

    async tests(ultra: UltraTestAPI) {
        const systemAPI = new SystemAPI(ultra);
        const ultraAPI = new UltraAPIv2(ultra);

        return {
            'Initialize evm contract': async () => {
                await assertAsync(ultraAPI.evm.init(15555, {gas_price: 150000000000, miner_cut: 10000, ingress_bridge_fee: "0.01000000 UOS"}), "failed to initialize evm");
                const config = await ultraAPI.evm.getConfig();
                assert(config.chainid === 15555);
                assert(config.miner_cut === 10000);
                assert(config.gas_price == 150000000000);
            },
            'Perform a transfer and check balances': async () => {
                let accounts = await ultraAPI.evm.getAllAccounts();
                let balances = await ultraAPI.evm.getAllBalances();
                let balanceWithDust = await ultraAPI.evm.getBalanceWithDust();
                assert(accounts.length == 0, "wrong accounts length");
                assert(balances.length == 1, "wrong balances length");
                assert(balances[0].owner == 'eosio.evm', "wrong balance[0] owner");
                assert(balances[0].balance.balance == '0.00000000 UOS', "wrong balance[0] balance");
                assert(balanceWithDust?.balance == '0.00000000 UOS', "wrong balance with dust");
                
                await ultraAPI.transferTokens("ultra.eosio", 'eosio.evm', 1, 'eosio.evm');
                balances = await ultraAPI.evm.getAllBalances();
                assert(balances.length == 1, "wrong balances length after transfer");
                assert(balances[0].owner == 'eosio.evm', "wrong balance[0] owner after transfer");
                assert(balances[0].balance.balance == '1.00000000 UOS', "wrong balance[0] balance after transfer");
                
                await ultraAPI.transferTokens("ultra.eosio", 'eosio.evm', 1000, "0x14dC79964da2C08b23698B3D3cc7Ca32193d9955");
                accounts = await ultraAPI.evm.getAllAccounts();
                balances = await ultraAPI.evm.getAllBalances();
                balanceWithDust = await ultraAPI.evm.getBalanceWithDust();
                assert(accounts.length == 1, "wrong accounts length after evm address transfer");
                assert(accounts[0].eth_address == '14dc79964da2c08b23698b3d3cc7ca32193d9955', "wrong ethereum account address");
                assert(Number('0x' + accounts[0].balance) == 999990000000000000000, "wrong ethereum account balance");
                assert(balances.length == 1, "wrong balances length after evm address transfer");
                assert(balances[0].owner == 'eosio.evm', "wrong balance[0] owner after evm address transfer");
                assert(balances[0].balance.balance == '1.01000000 UOS', "wrong balance[0] balance after evm address transfer");
                assert(balanceWithDust?.balance == '999.99000000 UOS', "wrong balance with dust after evm address transfer");
                
                await ultraAPI.transferTokens("ultra.eosio", 'eosio.evm', 2, "0x3787b98fc4e731d0456b3941f0b3fe2e01439961");
                accounts = await ultraAPI.evm.getAllAccounts();
                balances = await ultraAPI.evm.getAllBalances();
                balanceWithDust = await ultraAPI.evm.getBalanceWithDust();
                assert(accounts.length == 2, "wrong accounts length after evm address transfer 2");
                assert(accounts[1].eth_address == '3787b98fc4e731d0456b3941f0b3fe2e01439961', "wrong ethereum account address 2");
                assert(Number('0x' + accounts[1].balance) == 1990000000000000000, "wrong ethereum account balance 2");
                assert(balances.length == 1, "wrong balances length after evm address transfer 2");
                assert(balances[0].owner == 'eosio.evm', "wrong balance[0] owner after evm address transfer 2");
                assert(balances[0].balance.balance == '1.02000000 UOS', "wrong balance[0] balance after evm address transfer 2");
                assert(balanceWithDust?.balance == '1001.98000000 UOS', "wrong balance with dust after evm address transfer 2");
            },
            'Perform a transfer in EVM': async () => {
                const privateKey = '0x4bbbf85ce3377467afe5d46f804f221813b2bb87f24d81f60f1fcdbf7cbf4356';
                let account = Web3Accounts.privateKeyToAccount(privateKey);
                let trx = new Transaction({
                    to: '0x3787b98fc4e731d0456b3941f0b3fe2e01439961',
                    value: 1,
                    gasLimit: `0x${(1000000000).toString(16)}`,
                    gasPrice: `0x${(150000000000).toString(16)}`,
                    nonce: 0,
                }, {common: Web3Accounts.Common.custom({chainId: 15555})});
                let signature = await signTransaction(trx, privateKey);

                await assertAsync(ultraAPI.evm.pushTransaction('eosio.evm', signature.rawTransaction.substring(2)), 'Failed to send an EVM transfer');
                
                let accounts = await ultraAPI.evm.getAllAccounts();
                let balances = await ultraAPI.evm.getAllBalances();
                let balanceWithDust = await ultraAPI.evm.getBalanceWithDust();
                assert(accounts.length == 2, "wrong accounts length after evm transaction");
                assert(accounts[0].eth_address == '14dc79964da2c08b23698b3d3cc7ca32193d9955', "wrong ethereum account address 1");
                assert(Number('0x' + accounts[0].balance) == 999986850000000000000, "wrong ethereum account balance 1");
                assert(accounts[1].eth_address == '3787b98fc4e731d0456b3941f0b3fe2e01439961', "wrong ethereum account address 2");
                assert(Number('0x' + accounts[1].balance) == 1990000000000000001, "wrong ethereum account balance 2");
                assert(balances.length == 1, "wrong balances length after evm transaction");
                assert(balances[0].owner == 'eosio.evm', "wrong balance[0] owner after evm transaction");
                assert(balances[0].balance.balance == '1.02315000 UOS', "wrong balance[0] balance after evm transaction");
                assert(balanceWithDust?.balance == '1001.97685000 UOS', "wrong balance with dust after transaction");
            }
        };
    }
}
