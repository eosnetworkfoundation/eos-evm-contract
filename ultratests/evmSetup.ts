import { SignerLibTransactResponse, UltraTestAPI } from '@ultraos/ultratest/interfaces/test';
import { RequiredContract } from 'ultratest-ultra-startup-plugin/interfaces';
import { AccountConfiguration } from 'ultratest-ultra-startup-plugin/ultraStartupPermissions';
import { Plugin } from '@ultraos/ultratest/interfaces/plugin';
import { EvmAccount, EvmAccountCode, EvmBalance, EvmBalanceWithDust, EvmConfig, EvmConfig2, EvmMessageReceiver, EvmNextNonce, FeeParameters } from './interfaces/eosio.evm';
import { UltraAPIv2 } from 'ultratest-ultra-startup-plugin';
import { SystemAPI } from 'ultratest-system-plugin/system';
import { HTTP_API } from '@ultraos/ultratest/apis/pluginApi';

export class EvmContractHelpers {
    parent: UltraAPIv2;
    systemAPI: SystemAPI;
    api: HTTP_API;

    constructor(parent: UltraAPIv2, api: HTTP_API, system: SystemAPI) {
        this.parent = parent;
        this.systemAPI = system;
        this.api = api;
    }

    async init(chainid: number, fee_params: FeeParameters, token_contract: string | undefined = undefined) {
        return this.parent.transactOrThrow([
            {
                account: 'eosio.evm',
                name: 'init',
                authorization: [{actor: 'eosio.evm', permission: "active"}],
                data: {
                    chainid: chainid,
                    fee_params: fee_params,
                    token_contract: token_contract
                },
            },
        ]);
    }

    async getAccount(eth_address: string): Promise<EvmAccount | undefined> {
        const rows = await this.api.api.contract('eosio.evm').getTable<EvmAccount>('account', 'eosio.evm');
        return rows.find(v => { return v.eth_address === eth_address; })
    }

    async getAllAccounts(): Promise<EvmAccount[]> {
        return await this.api.api.contract('eosio.evm').getTable<EvmAccount>('account', 'eosio.evm');
    }

    async getAccountCode(id: number): Promise<EvmAccountCode | undefined> {
        const rows = await this.api.api.contract('eosio.evm').getTable<EvmAccountCode>('account', 'eosio.evm');
        return rows.find(v => { return v.id === id; })
    }

    async getBalance(owner: string): Promise<EvmBalance | undefined> {
        const rows = await this.api.api.contract('eosio.evm').getTable<EvmBalance>('balances', 'eosio.evm');
        return rows.find(v => { return v.owner === owner; })
    }

    async getAllBalances(): Promise<EvmBalance[]> {
        return await this.api.api.contract('eosio.evm').getTable<EvmBalance>('balances', 'eosio.evm');
    }

    async getConfig(): Promise<EvmConfig> {
        return (await this.api.api.contract('eosio.evm').getTable<EvmConfig>('config', 'eosio.evm'))[0];
    }

    async getConfig2(): Promise<EvmConfig2> {
        return (await this.api.api.contract('eosio.evm').getTable<EvmConfig2>('config2', 'eosio.evm'))[0];
    }

    async getBalanceWithDust(): Promise<EvmBalanceWithDust | undefined> {
        return (await this.api.api.contract('eosio.evm').getTable<EvmBalanceWithDust>('inevm', 'eosio.evm'))[0];
    }

    async getMessageReceiver(account: string): Promise<EvmMessageReceiver | undefined> {
        const rows = await this.api.api.contract('eosio.evm').getTable<EvmMessageReceiver>('msgreceiver', 'eosio.evm');
        return rows.find(v => { return v.account === account; })
    }

    async getNextNonce(owner: string): Promise<EvmNextNonce | undefined> {
        const rows = await this.api.api.contract('eosio.evm').getTable<EvmNextNonce>('nextnonces', 'eosio.evm');
        return rows.find(v => { return v.owner === owner; })
    }

    async pushTransaction(miner: string, rlptx: string): Promise<SignerLibTransactResponse> {
        return await this.parent.transactOrThrow([
            {
                account: 'eosio.evm',
                name: 'pushtx',
                data: {
                    miner: miner,
                    rlptx: rlptx
                },
                authorization: [{ actor: 'eosio.evm', permission: 'active' }],
            },
        ]);
    }
}

export default function evmSetup(): Plugin {
    return {
        name: 'evmSetup',
        params: () => { return '' },
        initCallback: async (plugin: Plugin, restoredFromSnapshot: boolean, ultra: UltraTestAPI) => {},
        stopCallback: async () => {},

        requiredAdditionalProtocolFeatures() {
            return [
                "fd28b9886c4469f34062a419232f4d5b8007d790b81ccc6d675fec941e7d80bb", // action return value
                "1a5b44765728146d71b9169dce52a31d5fac2143b61b008b7ca9328fdd8bac45", // get code hash
            ]
        },
    
        requiredContracts(): RequiredContract[] {
            return [{
                account: 'eosio.evm',
                contract: "../build/evm_runtime",
            }]
        },
    
        requiredAccounts(): (AccountConfiguration | string)[] {
            return [{
                    name: 'eosio.evm',
                    owner: {
                        threshold: 1,
                        accounts: [{ weight: 1, permission: { actor: 'ultra', permission: 'active' } }],
                    },
                    active: {
                        threshold: 1,
                        accounts: [{ weight: 1, permission: { actor: 'ultra', permission: 'admin' } }, { weight: 1, permission: { actor: 'eosio.evm', permission: 'eosio.code' } }],
                    },
                },
                "uos.pool"
            ]
        }
    }
}
