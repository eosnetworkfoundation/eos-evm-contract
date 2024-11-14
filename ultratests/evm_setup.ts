import { UltraTestAPI } from '@ultraos/ultratest/interfaces/test';
import { RequiredContract } from 'ultratest-ultra-startup-plugin/interfaces';
import { AccountConfiguration } from 'ultratest-ultra-startup-plugin/ultraStartupPermissions';
import { Plugin } from '@ultraos/ultratest/interfaces/plugin';

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
