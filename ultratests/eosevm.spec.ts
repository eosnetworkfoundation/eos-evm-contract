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
            // 'set up eos evm':async() => {
            //     assert((<SignerLibTransactResponse>(await ultra.api.activateFeature("fd28b9886c4469f34062a419232f4d5b8007d790b81ccc6d675fec941e7d80bb"))).status, "failed to activate action return value feature");
            //     assert((<SignerLibTransactResponse>(await ultra.api.activateFeature("1a5b44765728146d71b9169dce52a31d5fac2143b61b008b7ca9328fdd8bac45"))).status, "failed to activate get code hash feature");


            //     await ultra.api.activateFeature("1a5b44765728146d71b9169dce52a31d5fac2143b61b008b7ca9328fdd8bac45");

            //     // diable KYC
            //     assert(await cleos(`push action eosio.kyc togglekyc '[]' -p ultra.kyc`), "Failed to addcode to evm contract",  {swallow:hide_action_log});

            //     // activate get hash protocol feature
            //     assert(await activate_feature("fd28b9886c4469f34062a419232f4d5b8007d790b81ccc6d675fec941e7d80bb"), `failed to activate action return value feature`);
            //     assert(await activate_feature("1a5b44765728146d71b9169dce52a31d5fac2143b61b008b7ca9328fdd8bac45"), `failed to activate get code hash feature`);
            //     //assert(await activate_feature("cb79d449f8194bf6b403e40fda319a1e2f9d2b2108f7b6cb981662e6393ed6b9"), `failed to activate crypto primitives feature`);

            //     await sleep(3000);

            //     assert(await cleos(`set contract evmevmevmevm ${__dirname}/../build/evm_runtime evm_runtime.wasm evm_runtime.abi -p evmevmevmevm@active`), "Failed to publish evm contract",  {swallow:hide_action_log});
                
            //     // assert(await cleos(`push action evmevmevmevm init "{\"chainid\":15555,\"fee_params\":{\"gas_price\":150000000000,\"miner_cut\":10000,\"ingress_bridge_fee\":\"0.01000000 UOS\"}}" -p evmevmevmevm`), "Failed to init evm contract",  {swallow:hide_action_log});
            //     assert(await cleos(`push action evmevmevmevm init '[15555,[150000000000,10000,"0.01000000 UOS"]]' -p evmevmevmevm`), "Failed to init evm contract",  {swallow:hide_action_log});
            
            //     assert(await cleos(`set account permission evmevmevmevm active --add-code -p evmevmevmevm@owner`), "Failed to addcode to evm contract",  {swallow:hide_action_log});

            //     // 1 UOS to evm contract
            //     assert(await cleos(`transfer eosio evmevmevmevm "1.00000000 UOS" "evmevmevmevm"`), "Failed to transfer to evmevmevmevm",  {swallow:hide_action_log});
            //     // bridge 1000 UOS to "0x2787b98fc4e731d0456b3941f0b3fe2e01439961"
            //     assert(await cleos(`transfer eosio evmevmevmevm "1000.00000000 UOS" "0x2787b98fc4e731d0456b3941f0b3fe2e01439961"`), "Failed to transfer to evmevmevmevm",  {swallow:hide_action_log});
            //      // bridge 1 UOS to "0x3787b98fc4e731d0456b3941f0b3fe2e01439961"
            //     assert(await cleos(`transfer eosio evmevmevmevm "1.00000000 UOS" "0x3787b98fc4e731d0456b3941f0b3fe2e01439961"`), "Failed to transfer to evmevmevmevm",  {swallow:hide_action_log});
            
            //     // open account balance for evmminer account
            //     assert(await cleos(`push action evmevmevmevm open '{"owner":"evmminer"}' -p evmminer`), "Failed to transfer to evmevmevmevm",  {swallow:hide_action_log});
            // },
            'Initialize evm contract': async () => {
                await assertAsync(evmAPI.init(15555, {gas_price: 150000000000, miner_cut: 10000, ingress_bridge_fee: "0.01000000 UOS"}), "failed to initialize evm");
                console.log(await evmAPI.getConfig());
            },
            'Perform a transfer and check balances': async () => {
                console.log(await evmAPI.getAllBalances());
                console.log(await evmAPI.getBalanceWithDust());
                await ultraAPI.transferTokens("ultra.eosio", EVM_CONTRACT, 1, EVM_CONTRACT);
                await ultraAPI.transferTokens("ultra.eosio", EVM_CONTRACT, 1, 'uos.pool');
                console.log(await evmAPI.getAllBalances());
                console.log(await evmAPI.getBalanceWithDust());
                await ultraAPI.transferTokens("ultra.eosio", EVM_CONTRACT, 1000, "0x14dC79964da2C08b23698B3D3cc7Ca32193d9955");
                console.log(await evmAPI.getAllBalances());
                console.log(await evmAPI.getBalanceWithDust());
                await ultraAPI.transferTokens("ultra.eosio", EVM_CONTRACT, 2, "0x3787b98fc4e731d0456b3941f0b3fe2e01439961");
                console.log(await evmAPI.getAllBalances());
                console.log(await evmAPI.getBalanceWithDust());
            },
            'Perform a transfer in EVM': async () => {
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

                let result = await evmAPI.pushTransaction(EVM_CONTRACT, signature.rawTransaction.substring(2));
                //console.log(JSON.stringify(result, null, 4));
                console.log(await evmAPI.getAllAccounts());
                console.log(await evmAPI.getAllBalances());
                console.log(await evmAPI.getBalanceWithDust());
            }
        };
    }
}
