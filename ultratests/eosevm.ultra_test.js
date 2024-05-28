module.exports = class test {

    constructor() {}

    requiresSystemContracts(){
        return true;
    }

    requiredUnlimitedAccounts(){
        return [];
    }

    requiredAccounts(){
        return [
            'evmevmevmevm',
            'evmminer',
            'testa'
        ];
    }

    nodeosConfigs(){
        return {
            config:{
                'abi-serializer-max-time-ms':100000,
            },
        }
    }

    tests({assert, endpoint, cleos, rpc, api, ecc, keychain}){
        const sleep = ms => new Promise(r => setTimeout(r, ms));
        const POST = (route, body = {}) => fetch(`${endpoint}/v1/${route}`, {method: 'POST', headers: {Accept: 'application/json'}, body: JSON.stringify(body)}).then(x => x.json())
        const activate_feature = (digest) => cleos(`push action eosio activate '["${digest}"]' -p eosio@active`, {swallow: true})

        const assert_msg = (msg) => {
            return 'assertion failure with message: ' + msg;
        };
        const transact = actions => api.transact({actions}, { blocksBehind: 3, expireSeconds: 3600 }).then(x => true).catch(err => {
            return false;
        });
        const transact_assert = (actions, msg) => api.transact({actions}, { blocksBehind: 3, expireSeconds: 3600 }).then(x => true).catch(err => {
            if(err.json.error.details[0].message !== msg)
                assert(err.json.error.details[0].message === assert_msg(msg), 'Assertion message mismatch. Expected: "' + assert_msg(msg) + '". Got: "' + err.json.error.details[0].message + '"');
            return false;
        });

        const register_id_providers = (prov) => {
            return transact([{
                account: 'eosio.eba',
                name: 'regidp',
                authorization: [{ actor: 'ultra.eba', permission: 'active' }],
                data: { id_providers:prov }
            }]);
        };

        const get_provider = (provider) => rpc.get_table_rows({ json: true, code: 'eosio.eba', scope: 'eosio.eba', table: 'idproviders'}).catch(() => null).then(data => {
            if (typeof data.rows[0].id_providers !== "undefined") {
                const prod = data.rows[0].id_providers.find((prod) => {
                    return prod.account === provider;
                });

                if (typeof prod !== "undefined")
                    return true;
            }
            return false;
        });

        const hide_action_log = false;

        return {
            'set up eos evm':async() => {
                // diable KYC
                assert(await cleos(`push action eosio.kyc togglekyc '[]' -p ultra.kyc`), "Failed to addcode to evm contract",  {swallow:hide_action_log});

                // activate get hash protocol feature
                assert(await activate_feature("fd28b9886c4469f34062a419232f4d5b8007d790b81ccc6d675fec941e7d80bb"), `failed to activate action return value feature`);
                assert(await activate_feature("1a5b44765728146d71b9169dce52a31d5fac2143b61b008b7ca9328fdd8bac45"), `failed to activate get code hash feature`);
                assert(await activate_feature("cb79d449f8194bf6b403e40fda319a1e2f9d2b2108f7b6cb981662e6393ed6b9"), `failed to activate crypto primitives feature`);

                await sleep(3000);

                assert(await cleos(`set contract evmevmevmevm ${__dirname}/../build/evm_runtime evm_runtime.wasm evm_runtime.abi -p evmevmevmevm@active`), "Failed to publish evm contract",  {swallow:hide_action_log});
                
                // assert(await cleos(`push action evmevmevmevm init "{\"chainid\":15555,\"fee_params\":{\"gas_price\":150000000000,\"miner_cut\":10000,\"ingress_bridge_fee\":\"0.01000000 UOS\"}}" -p evmevmevmevm`), "Failed to init evm contract",  {swallow:hide_action_log});
                assert(await cleos(`push action evmevmevmevm init '[15555,[150000000000,10000,"0.01000000 UOS"]]' -p evmevmevmevm`), "Failed to init evm contract",  {swallow:hide_action_log});
            
                assert(await cleos(`set account permission evmevmevmevm active --add-code -p evmevmevmevm@owner`), "Failed to addcode to evm contract",  {swallow:hide_action_log});

                // 1 UOS to evm contract
                assert(await cleos(`transfer eosio evmevmevmevm "1.00000000 UOS" "evmevmevmevm"`), "Failed to transfer to evmevmevmevm",  {swallow:hide_action_log});
                // bridge 1000 UOS to "0x2787b98fc4e731d0456b3941f0b3fe2e01439961"
                assert(await cleos(`transfer eosio evmevmevmevm "1000.00000000 UOS" "0x2787b98fc4e731d0456b3941f0b3fe2e01439961"`), "Failed to transfer to evmevmevmevm",  {swallow:hide_action_log});
                 // bridge 1 UOS to "0x3787b98fc4e731d0456b3941f0b3fe2e01439961"
                assert(await cleos(`transfer eosio evmevmevmevm "1.00000000 UOS" "0x3787b98fc4e731d0456b3941f0b3fe2e01439961"`), "Failed to transfer to evmevmevmevm",  {swallow:hide_action_log});
            
                // open account balance for evmminer account
                assert(await cleos(`push action evmevmevmevm open '{"owner":"evmminer"}' -p evmminer`), "Failed to transfer to evmevmevmevm",  {swallow:hide_action_log});
            },
        }
    }
}
