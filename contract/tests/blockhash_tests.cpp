#include "native_token_tester.hpp"
#include <silkworm/execution/address.hpp>

struct blockhash_evm_tester : native_token_evm_tester_EOS {
   static constexpr eosio::chain::name evm_account_name = "evm"_n;
};

struct evm_account {
    uint64_t            id;
    std::vector<char>   eth_address;
    uint64_t            nonce;
    std::vector<char>   balance;
    std::vector<char>   code;
    std::vector<char>   code_hash;
};
FC_REFLECT(evm_account, (id)(eth_address)(nonce)(balance)(code)(code_hash));

struct evm_storage {
   uint64_t id;
   std::vector<char> key;
   std::vector<char> value;
};
FC_REFLECT(evm_storage, (id)(key)(value));

BOOST_AUTO_TEST_SUITE(blockhash_evm_tests)
BOOST_FIXTURE_TEST_CASE(blockhash_tests, blockhash_evm_tester) try {

   // tests/leap/nodeos_trust_evm_server/contracts/Blockhash.sol
   const std::string blockhash_bytecode = "608060405234801561001057600080fd5b506102e8806100206000396000f3fe608060405234801561001057600080fd5b506004361061007c5760003560e01c8063c059239b1161005b578063c059239b146100c7578063c835de3c146100e5578063edb572a814610103578063f9943638146101215761007c565b80627da6cb146100815780630f59f83a1461009f5780638603136b146100a9575b600080fd5b61008961013f565b6040516100969190610200565b60405180910390f35b6100a7610149565b005b6100b16101b6565b6040516100be9190610200565b60405180910390f35b6100cf6101c0565b6040516100dc9190610200565b60405180910390f35b6100ed6101ca565b6040516100fa9190610200565b60405180910390f35b61010b6101d4565b6040516101189190610200565b60405180910390f35b6101296101de565b6040516101369190610234565b60405180910390f35b6000600454905090565b4360008190555060014361015d919061027e565b40600181905550600243610171919061027e565b40600281905550600343610185919061027e565b40600381905550600443610199919061027e565b406004819055506005436101ad919061027e565b40600581905550565b6000600154905090565b6000600554905090565b6000600354905090565b6000600254905090565b60008054905090565b6000819050919050565b6101fa816101e7565b82525050565b600060208201905061021560008301846101f1565b92915050565b6000819050919050565b61022e8161021b565b82525050565b60006020820190506102496000830184610225565b92915050565b7f4e487b7100000000000000000000000000000000000000000000000000000000600052601160045260246000fd5b60006102898261021b565b91506102948361021b565b92508282039050818111156102ac576102ab61024f565b5b9291505056fea2646970667358221220c437dcf9206268de785ce81fef2e29e3da1e58070205eb27bc943f73938bd29264736f6c63430008110033";

   // Fund evm1 address with 100 EOS
   evm_eoa evm1;
   const int64_t to_bridge = 1000000;
   transfer_token("alice"_n, "evm"_n, make_asset(to_bridge), evm1.address_0x());

   silkworm::Transaction txn {
      .type = silkworm::Transaction::Type::kLegacy,
      .max_priority_fee_per_gas = 0,
      .max_fee_per_gas = 1,
      .gas_limit = 500000,
      .data = silkworm::from_hex(blockhash_bytecode).value()
   };
   
   evm1.sign(txn);
   pushtx(txn);

   produce_blocks(50);

   // Get blockhash contract address
   auto contract_addr = silkworm::create_address(evm1.address, 0);
   
   // Call method "go" on blockhash contract (sha3('go()') = 0x0f59f83a)
   // This method will store the current EVM block number and the 5 previous block hashes
   silkworm::Transaction txn2 {
      .type = silkworm::Transaction::Type::kLegacy,
      .max_priority_fee_per_gas = 0,
      .max_fee_per_gas = 1,
      .gas_limit = 500000,
      .to = contract_addr,
      .data = silkworm::from_hex("0x0f59f83a").value()
   };

   evm1.sign(txn2);
   pushtx(txn2);

   // NOTE/TODO: Hacky way to query contract storage. We should change this by having a new contract action
   // that executes readonly functions, or refactor evm_runtime_tests and extract the silkworm::State
   // implementation that works agains the leap db

   uint64_t contract_account_id=0;
   while(true) {
      auto a = fc::raw::unpack<evm_account>(get_row_by_account("evm"_n, "evm"_n, "account"_n, eosio::chain::name{contract_account_id}));
      if(fc::to_hex((char*)a.eth_address.data(), a.eth_address.size()) == fc::to_hex((char*)contract_addr.bytes, 20)) break;
      contract_account_id++;
   }

   // Retrieve 6 slots from contract storage where block number and hashes were saved
   // ...
   // contract Blockhash {
   //
   // uint256 curr_block;   //slot0
   // bytes32 prev1;        //slot1
   // bytes32 prev2;        //slot2
   // bytes32 prev3;        //slot3
   // bytes32 prev4;        //slot4
   // bytes32 prev5;        //slot5
   // ..

   // blockhash(n) = sha256(0x00 || n || chain_id)
   auto generate_block_hash = [](uint64_t height) {
         char buffer[1+8+8];
         datastream<char*>  ds(buffer, sizeof(buffer));
         ds << uint8_t{0};
         ds << uint64_t{height};
         ds << uint64_t{15555};
         auto h = fc::sha256::hash(buffer, sizeof(buffer));
         return fc::to_hex(h.data(), h.data_size());
   };

   uint64_t curr_block;
   for(size_t slot=0; slot<6; ++slot) {
      auto sid = 5 - slot;
      auto s = fc::raw::unpack<evm_storage>(get_row_by_account("evm"_n, eosio::chain::name{contract_account_id}, "storage"_n, eosio::chain::name{sid}));
      dlog("slot: ${slot}, k: ${k}, v: ${v}", ("slot",slot)("k",s.key)("v",s.value));
      
      if(slot == 0) {
         curr_block = static_cast<uint64_t>(intx::be::unsafe::load<intx::uint256>((const uint8_t*)s.value.data()));
      } else {
         auto retrieved_block_hash = fc::to_hex(s.value.data(), s.value.size());
         auto calculated_block_hash = generate_block_hash(curr_block-slot);
         BOOST_REQUIRE(calculated_block_hash == retrieved_block_hash);
      }
   }
} FC_LOG_AND_RETHROW()
BOOST_AUTO_TEST_SUITE_END()