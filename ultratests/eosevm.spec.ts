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

const EVM_CONTRACT = "evm"

interface FeeParameters {
    gas_price?: number;
    miner_cut?: number;
    ingress_bridge_fee?: string;
}

interface EvmAccount {
    id: number;
    eth_address: string;
    nonce: number;
    balance: string;
    code_id?: number;
    flags: number;
}

interface EvmAccountCode {
    id: number;
    ref_count: number;
    code: string;
    code_hahs: string;
}

interface EvmBalance {
    owner: string;
    balance: EvmBalanceWithDust;
}

interface EvmConsensusParameter {
    gas_parameter: {
        gas_txnewaccount: number;
        gas_newaccount: number;
        gas_txcreate: number;
        gas_codedeposit: number;
        gas_sset: number;   
    }
}

interface EvmConfig {
    version: number;
    chainid: number;
    genesis_time: string;
    ingress_bridge_fee: string;
    gas_price: number;
    miner_cut: number;
    status: number;
    evm_version: {
        pending_value?: number;
        cached_value: number
    },
    consensus_parameter: {
        cached_value: [string, EvmConsensusParameter],
        pending_value?: [string, EvmConsensusParameter]
    },
    token_contract: string;
    queue_front_block: number;
    gas_prices: {
        overhead_price: number;
        storage_price: number;
    }
}

interface EvmConfig2 {
    next_account_id: number;
}

interface EvmBalanceWithDust {
    balance: string;
    dust: number;
}

interface EvmMessageReceiver {
    account: string;
    handler: string;
    min_fee: string;
    flags: number;
}

interface EvmNextNonce {
    owner: string;
    next_nonce: number;
}

class EvmHelper {
    api: HTTP_API;
    ultra: UltraTestAPI;
    ultraAPI: UltraAPIv2

    constructor(ultra: UltraTestAPI, ultraAPI: UltraAPIv2) {
        this.api = ultra.api;
        this.ultra = ultra;
        this.ultraAPI = ultraAPI;
    }

    async init(chainid: number, fee_params: FeeParameters, token_contract: string | undefined = undefined) {
        return this.ultraAPI.transactOrThrow([
            {
                account: EVM_CONTRACT,
                name: 'init',
                authorization: [{actor: EVM_CONTRACT, permission: "active"}],
                data: {
                    chainid: chainid,
                    fee_params: fee_params,
                    token_contract: token_contract
                },
            },
        ]);
    }

    async getAccount(eth_address: string): Promise<EvmAccount | undefined> {
        const rows = await this.api.api.contract(EVM_CONTRACT).getTable<EvmAccount>('account', EVM_CONTRACT);
        return rows.find(v => { return v.eth_address === eth_address; })
    }

    async getAllAccounts(): Promise<EvmAccount[]> {
        return await this.api.api.contract(EVM_CONTRACT).getTable<EvmAccount>('account', EVM_CONTRACT);
    }

    async getAccountCode(id: number): Promise<EvmAccountCode | undefined> {
        const rows = await this.api.api.contract(EVM_CONTRACT).getTable<EvmAccountCode>('account', EVM_CONTRACT);
        return rows.find(v => { return v.id === id; })
    }

    async getBalance(owner: string): Promise<EvmBalance | undefined> {
        const rows = await this.api.api.contract(EVM_CONTRACT).getTable<EvmBalance>('balances', EVM_CONTRACT);
        return rows.find(v => { return v.owner === owner; })
    }

    async getAllBalances(): Promise<EvmBalance[]> {
        return await this.api.api.contract(EVM_CONTRACT).getTable<EvmBalance>('balances', EVM_CONTRACT);
    }

    async getConfig(): Promise<EvmConfig> {
        return (await this.api.api.contract(EVM_CONTRACT).getTable<EvmConfig>('config', EVM_CONTRACT))[0];
    }

    async getConfig2(): Promise<EvmConfig2> {
        return (await this.api.api.contract(EVM_CONTRACT).getTable<EvmConfig2>('config2', EVM_CONTRACT))[0];
    }

    async getBalanceWithDust(): Promise<EvmBalanceWithDust | undefined> {
        return (await this.api.api.contract(EVM_CONTRACT).getTable<EvmBalanceWithDust>('inevm', EVM_CONTRACT))[0];
    }

    async getMessageReceiver(account: string): Promise<EvmMessageReceiver | undefined> {
        const rows = await this.api.api.contract(EVM_CONTRACT).getTable<EvmMessageReceiver>('msgreceiver', EVM_CONTRACT);
        return rows.find(v => { return v.account === account; })
    }

    async getNextNonce(owner: string): Promise<EvmNextNonce | undefined> {
        const rows = await this.api.api.contract(EVM_CONTRACT).getTable<EvmNextNonce>('nextnonces', EVM_CONTRACT);
        return rows.find(v => { return v.owner === owner; })
    }

    async pushTransaction(miner: string, rlptx: string): Promise<SignerLibTransactResponse> {
        return await this.ultraAPI.transactOrThrow([
            {
                account: EVM_CONTRACT,
                name: 'pushtx',
                data: {
                    miner: miner,
                    rlptx: rlptx
                },
                authorization: [{ actor: EVM_CONTRACT, permission: 'active' }],
            },
        ]);
    }
}

export default class Test extends UltraTest {
    constructor() {
        super();
    }

    async onChainStart(ultra: UltraTestAPI) {
        ultra.addPlugins([genesis(ultra), system(ultra), ultraContracts(ultra), await ultraStartup(ultra)]);
    }

    requiredAdditionalProtocolFeatures() {
        return [
            "fd28b9886c4469f34062a419232f4d5b8007d790b81ccc6d675fec941e7d80bb", // action return value
            "1a5b44765728146d71b9169dce52a31d5fac2143b61b008b7ca9328fdd8bac45", // get code hash
        ]
    }

    requiredContracts(): RequiredContract[] {
        return [{
            account: EVM_CONTRACT,
            contract: "../build/evm_runtime",
        }]
    }

    requiredAccounts(): (AccountConfiguration | string)[] {
        return [{
            name: EVM_CONTRACT,
            owner: {
                threshold: 1,
                accounts: [{ weight: 1, permission: { actor: 'ultra', permission: 'active' } }],
            },
            active: {
                threshold: 1,
                accounts: [{ weight: 1, permission: { actor: 'ultra', permission: 'admin' } }, { weight: 1, permission: { actor: EVM_CONTRACT, permission: 'eosio.code' } }],
            },
        },
        "uos.pool"
        ]
    }

    async tests(ultra: UltraTestAPI) {
        const systemAPI = new SystemAPI(ultra);
        const ultraAPI = new UltraAPIv2(ultra);
        const evmAPI = new EvmHelper(ultra, ultraAPI);

        return {
            'Initialize evm contract': async () => {
                await assertAsync(evmAPI.init(15555, {gas_price: 150000000000, miner_cut: 10000, ingress_bridge_fee: "0.01000000 UOS"}), "failed to initialize evm");
                const config = await evmAPI.getConfig();
                assert(config.chainid === 15555);
                assert(config.miner_cut === 10000);
                assert(config.gas_price == 150000000000);
            },
            'Perform a transfer and check balances': async () => {
                let accounts = await evmAPI.getAllAccounts();
                let balances = await evmAPI.getAllBalances();
                let balanceWithDust = await evmAPI.getBalanceWithDust();
                assert(accounts.length == 0, "wrong accounts length");
                assert(balances.length == 2, "wrong balances length");
                assert(balances[0].owner == EVM_CONTRACT, "wrong balance[0] owner");
                assert(balances[0].balance.balance == '0.00000000 UOS', "wrong balance[0] balance");
                assert(balances[1].owner == 'uos.pool', "wrong balance[1] owner");
                assert(balances[1].balance.balance == '0.00000000 UOS', "wrong balance[1] balance");
                assert(balanceWithDust?.balance == '0.00000000 UOS', "wrong balance with dust");
                
                await ultraAPI.transferTokens("ultra.eosio", EVM_CONTRACT, 1, EVM_CONTRACT);
                await ultraAPI.transferTokens("ultra.eosio", EVM_CONTRACT, 1, 'uos.pool');
                balances = await evmAPI.getAllBalances();
                assert(balances.length == 2, "wrong balances length after transfer");
                assert(balances[0].owner == EVM_CONTRACT, "wrong balance[0] owner after transfer");
                assert(balances[0].balance.balance == '1.00000000 UOS', "wrong balance[0] balance after transfer");
                assert(balances[1].owner == 'uos.pool', "wrong balance[1] owner after transfer");
                assert(balances[1].balance.balance == '1.00000000 UOS', "wrong balance[1] balance after transfer");
                
                await ultraAPI.transferTokens("ultra.eosio", EVM_CONTRACT, 1000, "0x14dC79964da2C08b23698B3D3cc7Ca32193d9955");
                accounts = await evmAPI.getAllAccounts();
                balances = await evmAPI.getAllBalances();
                balanceWithDust = await evmAPI.getBalanceWithDust();
                assert(accounts.length == 1, "wrong accounts length after evm address transfer");
                assert(accounts[0].eth_address == '14dc79964da2c08b23698b3d3cc7ca32193d9955', "wrong ethereum account address");
                assert(Number('0x' + accounts[0].balance) == 999990000000000000000, "wrong ethereum account balance");
                assert(balances.length == 2, "wrong balances length after evm address transfer");
                assert(balances[0].owner == EVM_CONTRACT, "wrong balance[0] owner after evm address transfer");
                assert(balances[0].balance.balance == '1.00000000 UOS', "wrong balance[0] balance after evm address transfer");
                assert(balances[1].owner == 'uos.pool', "wrong balance[1] owner after evm address transfer");
                assert(balances[1].balance.balance == '1.01000000 UOS', "wrong balance[1] balance after evm address transfer");
                assert(balanceWithDust?.balance == '999.99000000 UOS', "wrong balance with dust after evm address transfer");
                
                await ultraAPI.transferTokens("ultra.eosio", EVM_CONTRACT, 2, "0x3787b98fc4e731d0456b3941f0b3fe2e01439961");
                accounts = await evmAPI.getAllAccounts();
                balances = await evmAPI.getAllBalances();
                balanceWithDust = await evmAPI.getBalanceWithDust();
                assert(accounts.length == 2, "wrong accounts length after evm address transfer 2");
                assert(accounts[1].eth_address == '3787b98fc4e731d0456b3941f0b3fe2e01439961', "wrong ethereum account address 2");
                assert(Number('0x' + accounts[1].balance) == 1990000000000000000, "wrong ethereum account balance 2");
                assert(balances.length == 2, "wrong balances length after evm address transfer 2");
                assert(balances[0].owner == EVM_CONTRACT, "wrong balance[0] owner after evm address transfer 2");
                assert(balances[0].balance.balance == '1.00000000 UOS', "wrong balance[0] balance after evm address transfer 2");
                assert(balances[1].owner == 'uos.pool', "wrong balance[1] owner after evm address transfer 2");
                assert(balances[1].balance.balance == '1.02000000 UOS', "wrong balance[1] balance after evm address transfer 2");
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

                await assertAsync(evmAPI.pushTransaction(EVM_CONTRACT, signature.rawTransaction.substring(2)), 'Failed to send an EVM transfer');
                
                let accounts = await evmAPI.getAllAccounts();
                let balances = await evmAPI.getAllBalances();
                let balanceWithDust = await evmAPI.getBalanceWithDust();
                assert(accounts.length == 2, "wrong accounts length after evm transaction");
                assert(accounts[0].eth_address == '14dc79964da2c08b23698b3d3cc7ca32193d9955', "wrong ethereum account address 1");
                assert(Number('0x' + accounts[0].balance) == 999986850000000000000, "wrong ethereum account balance 1");
                assert(accounts[1].eth_address == '3787b98fc4e731d0456b3941f0b3fe2e01439961', "wrong ethereum account address 2");
                assert(Number('0x' + accounts[1].balance) == 1990000000000000001, "wrong ethereum account balance 2");
                assert(balances.length == 2, "wrong balances length after evm transaction");
                assert(balances[0].owner == EVM_CONTRACT, "wrong balance[0] owner after evm transaction");
                assert(balances[0].balance.balance == '1.00000000 UOS', "wrong balance[0] balance after evm transaction");
                assert(balances[1].owner == 'uos.pool', "wrong balance[1] owner after evm transaction");
                assert(balances[1].balance.balance == '1.02315000 UOS', "wrong balance[1] balance after evm transaction");
                assert(balanceWithDust?.balance == '1001.97685000 UOS', "wrong balance with dust after transaction");
            }
        };
    }
}
