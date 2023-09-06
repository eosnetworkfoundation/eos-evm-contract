#include "basic_evm_tester.hpp"
#include <silkworm/execution/address.hpp>

using namespace evm_test;
struct blockhash_evm_tester : basic_evm_tester {
   blockhash_evm_tester() {
      create_accounts({"alice"_n});
      transfer_token(faucet_account_name, "alice"_n, make_asset(10000'0000));
      init();
   }
};

BOOST_AUTO_TEST_SUITE(blockhash_evm_tests)
BOOST_FIXTURE_TEST_CASE(blockhash_tests, blockhash_evm_tester) try {

   // tests/leap/nodeos_eos_evm_server/contracts/Blockhash.sol
   const std::string blockhash_bytecode = 
      "608060405234801561001057600080fd5b506102e8806100206000396000f3fe608060405234801561001057600080fd5b5060"
      "04361061007c5760003560e01c8063c059239b1161005b578063c059239b146100c7578063c835de3c146100e5578063edb572"
      "a814610103578063f9943638146101215761007c565b80627da6cb146100815780630f59f83a1461009f5780638603136b1461"
      "00a9575b600080fd5b61008961013f565b6040516100969190610200565b60405180910390f35b6100a7610149565b005b6100"
      "b16101b6565b6040516100be9190610200565b60405180910390f35b6100cf6101c0565b6040516100dc9190610200565b6040"
      "5180910390f35b6100ed6101ca565b6040516100fa9190610200565b60405180910390f35b61010b6101d4565b604051610118"
      "9190610200565b60405180910390f35b6101296101de565b6040516101369190610234565b60405180910390f35b6000600454"
      "905090565b4360008190555060014361015d919061027e565b40600181905550600243610171919061027e565b406002819055"
      "50600343610185919061027e565b40600381905550600443610199919061027e565b406004819055506005436101ad91906102"
      "7e565b40600581905550565b6000600154905090565b6000600554905090565b6000600354905090565b600060025490509056"
      "5b60008054905090565b6000819050919050565b6101fa816101e7565b82525050565b60006020820190506102156000830184"
      "6101f1565b92915050565b6000819050919050565b61022e8161021b565b82525050565b600060208201905061024960008301"
      "84610225565b92915050565b7f4e487b7100000000000000000000000000000000000000000000000000000000600052601160"
      "045260246000fd5b60006102898261021b565b91506102948361021b565b92508282039050818111156102ac576102ab61024f"
      "565b5b9291505056fea2646970667358221220c437dcf9206268de785ce81fef2e29e3da1e58070205eb27bc943f73938bd292"
      "64736f6c63430008110033";

   // Fund evm1 address with 100 EOS
   evm_eoa evm1;
   const int64_t to_bridge = 1000000;
   transfer_token("alice"_n, "evm"_n, make_asset(to_bridge), evm1.address_0x());

   // Deploy blockhash contract and get its address and account ID
   auto contract_addr = deploy_contract(evm1, evmc::from_hex(blockhash_bytecode).value());
   uint64_t contract_account_id = find_account_by_address(contract_addr).value().id;

   // Generate some blocks
   produce_blocks(50);

   // Call method "go" on blockhash contract (sha3('go()') = 0x0f59f83a)
   // This method will store the current EVM block number and the 5 previous block hashes
   auto txn = generate_tx(contract_addr, 0, 500'000);
   txn.data = evmc::from_hex("0x0f59f83a").value();
   evm1.sign(txn);
   pushtx(txn);

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
   std::map<uint64_t, intx::uint256> slots;
   bool successful_scan = scan_account_storage(contract_account_id, [&](storage_slot&& slot) -> bool {
      if(slot.key <= 5) {
         auto key_u64 = static_cast<uint64_t>(slot.key);
         BOOST_REQUIRE(slots.count(key_u64) == 0);
         slots[key_u64] = slot.value;
         return false;
      } 
      BOOST_ERROR("unexpected storage in contract");
      return true;
   });

   BOOST_REQUIRE(slots.size() == 6);
   auto curr_block = static_cast<uint64_t>(slots[0]);

   for (auto n : { 1, 2, 3, 4, 5 }) {
      auto retrieved_block_hash = intx::hex(slots[n]);
      auto calculated_block_hash = generate_block_hash(curr_block-n);
      BOOST_REQUIRE(calculated_block_hash == retrieved_block_hash);
   }

} FC_LOG_AND_RETHROW()
BOOST_AUTO_TEST_SUITE_END()