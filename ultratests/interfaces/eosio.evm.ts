export interface FeeParameters {
    gas_price?: number;
    miner_cut?: number;
    ingress_bridge_fee?: string;
}

export interface EvmAccount {
    id: number;
    eth_address: string;
    nonce: number;
    balance: string;
    code_id?: number;
    flags: number;
}

export interface EvmAccountCode {
    id: number;
    ref_count: number;
    code: string;
    code_hahs: string;
}

export interface EvmBalance {
    owner: string;
    balance: EvmBalanceWithDust;
}

export interface EvmConsensusParameter {
    gas_parameter: {
        gas_txnewaccount: number;
        gas_newaccount: number;
        gas_txcreate: number;
        gas_codedeposit: number;
        gas_sset: number;   
    }
}

export interface EvmConfig {
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

export interface EvmConfig2 {
    next_account_id: number;
}

export interface EvmBalanceWithDust {
    balance: string;
    dust: number;
}

export interface EvmMessageReceiver {
    account: string;
    handler: string;
    min_fee: string;
    flags: number;
}

export interface EvmNextNonce {
    owner: string;
    next_nonce: number;
}