TrustEVM Unit Tests Verbose:

Running 18 tests for test/TrustEVMStakingV2.t.sol:TrustEVMTest
[PASS] testAdminAddStaker() (gas: 111630)
[PASS] testAdminChangeRewardsPerBlock() (gas: 20774)
Logs:
  The Rewards Per Block are now: 500000000000000000

[PASS] testEVMContractStake() (gas: 224485)
Logs:
  New staker balance: 9000000000000000000
  TrustEVM Contract Balance: 1000000000000000000

[PASS] testEmergencyWithdraw() (gas: 199309)
Logs:
  Block Number: 1
  Staking amount: 5000000000000000000
  

  Block Number: 10
  -- staker Emergencywithdraw() --
  Staking amount: 0
  Owed: 0
  Reward Debt: 0

[PASS] testGetAmount() (gas: 220020)
[PASS] testIsAdmin() (gas: 7980)
[PASS] testPendingEVMForOneStaker() (gas: 495997)
Logs:
  #### Initialization & Formulas #################################################################
  ################################################################################################
  EVM Rewards Per Block (1 eth): 1000000000000000000
  EVM Rewards = Number of Blocks * EVM Rewards Per Block
  Accumulated EVM Per Share = Accumulated EVM Per Share + (EVM Rewards * 1e12 / Staking Contract Balance)
  PendingEVM = (Staked Amount * Accumulated EVM Per Share / 1e12)- Reward Debt
  ################################################################################################
  Staking Contract Balance: 10000000000000000000
  User staking amount: 5000000000000000000
  ################################################################################################
  Block Number: 2
  Reward Debt: 0
  Accumulated EVM Per Share: 200000000000
  Last Reward Block: 2
  EVM Rewards Per Block: 0
  Pending EVM: 1000000000000000000
  ---------------
  Block Number: 3
  Reward Debt: 0
  Accumulated EVM Per Share: 200000000000
  Last Reward Block: 2
  EVM Rewards Per Block: 1000000000000000000
  Pending EVM: 1500000000000000000
  ---------------
  Block Number: 4
  Reward Debt: 0
  Accumulated EVM Per Share: 200000000000
  Last Reward Block: 2
  EVM Rewards Per Block: 2000000000000000000
  Pending EVM: 2000000000000000000
  ---------------
  Block Number: 5
  Reward Debt: 0
  Accumulated EVM Per Share: 200000000000
  Last Reward Block: 2
  EVM Rewards Per Block: 3000000000000000000
  Pending EVM: 2500000000000000000

[PASS] testRemoveStaker() (gas: 91892)
[PASS] testRevokeStakerRole() (gas: 92408)
[PASS] testRewardsAlgorithmInspectOneStaker() (gas: 2340092)
Logs:
  #### Initialization & Formulas #################################################################
  ################################################################################################
  EVM Rewards Per Block (1 EVM): 1000000000000000000
  EVM Rewards = Number of Blocks * EVM Rewards Per Block
  Accumulated EVM Per Share = Accumulated EVM Per Share + (EVM Rewards * 1e12 / Staking Contract Balance) // This updates only when deposit or withdraw
  PendingEVM = (Staked Amount * Accumulated EVM Per Share / 1e12) - (Reward Debt - Owed Amount)
  ################################################################################################
  

  Block Number: 1
  Last Reward Block: 1
  Staking Pool Balance: 7000000000000000000
  Rewards Pool Balance: 0
  Staker1 Initial Staking Amount: 7000000000000000000
  Accumulated EVM Per Share: 0
  EVM Rewards: 0
  Reward Debt: 0
  Owed: 0
  Pending EVM: 0
  

  Block Number: 2
  Last Reward Block: 1
  Rewards Pool Balance: 0
  EVM Rewards: 1000000000000000000
  Accumulated EVM Per Share: 0
  Reward Debt: 0
  Owed: 0
  Pending EVM: 999999999999000000
  

  Block Number: 3
  Last Reward Block: 1
  Rewards Pool Balance: 0
  EVM Rewards before withdraw: 2000000000000000000
  --Withdraw(0)--
  Accumulated EVM Per Share: 285714285714
  Last Reward Block: 3
  EVM Rewards after withdraw: 0
  Reward Debt: 1999999999998000000
  Owed: 1999999999998000000
  Pending EVM: 1999999999998000000
  

  Block Number: 4
  Last Reward Block: 3
  Rewards Pool Balance: 0
  EVM Rewards: 1000000000000000000
  Accumulated EVM Per Share: 285714285714
  Reward Debt: 1999999999998000000
  Owed: 1999999999998000000
  Pending EVM: 2999999999997000000
  

  Block Number: 5
  Last Reward Block: 3
  Rewards Pool Balance: 0
  EVM Rewards: 2000000000000000000
  Accumulated EVM Per Share: 285714285714
  Reward Debt: 1999999999998000000
  Owed: 1999999999998000000
  Pending EVM: 3999999999996000000
  

  Block Number: 6
  Last Reward Block: 3
  Rewards Pool Balance: 0
  EVM Rewards: 3000000000000000000
  Accumulated EVM Per Share: 285714285714
  Reward Debt: 1999999999998000000
  Owed: 1999999999998000000
  Pending EVM: 4999999999995000000
  

  Block Number: 7
  Last Reward Block: 3
  Rewards Pool Balance: 0
  EVM Rewards before withdraw: 4000000000000000000
  --Staker1 Withdraw(0)--
  Accumulated EVM Per Share: 857142857142
  Last Reward Block: 7
  EVM Rewards after withdraw: 0
  Reward Debt: 5999999999994000000
  Owed: 5999999999994000000
  Pending EVM: 5999999999994000000
  

  Block Number: 8
  Last Reward Block: 7
  Rewards Pool Balance: 0
  EVM Rewards: 1000000000000000000
  Accumulated EVM Per Share: 857142857142
  Reward Debt: 5999999999994000000
  Owed: 5999999999994000000
  Pending EVM: 6999999999993000000
  @@ Add Rewards: 20 ether @@
  

  Block Number: 9
  Last Reward Block: 7
  Rewards Pool Balance: 20000000000000000000
  EVM Rewards before withdraw: 2000000000000000000
  --Staker1 Withdraw(0)--
  Accumulated EVM Per Share: 1142857142856
  Last Reward Block: 9
  EVM Rewards after withdraw: 0
  Reward Debt: 7999999999992000000
  @@ Here is where the transfer of rewards to the user occurs @@ Owed: 0
  Pending EVM: 0
  

  New Rewards Pool Balance: 12000000000008000000
  

  To see the transfer from the contract to the Staker1 type 'forge test -vvvv'
  

  This line shows the transaction from:
  0xAb8483F64d9C6d1EcF9b849Ae677dD3315835cb2::fallback{value: 7999999999992000000}()
  


[PASS] testRewardsAlgorithmInspectThreeStakers() (gas: 3191805)
Logs:
  #### Initialization & Formulas #################################################################
  ################################################################################################
  EVM Rewards Per Block (1 EVM): 1000000000000000000
  EVM Rewards = Number of Blocks * EVM Rewards Per Block
  Accumulated EVM Per Share = Accumulated EVM Per Share + (EVM Rewards * 1e12 / Staking Contract Balance) // This updates only when deposit or withdraw
  PendingEVM = (Staked Amount * Accumulated EVM Per Share / 1e12) - (Reward Debt - Owed Amount)
  ################################################################################################
  

  Block Number: 1
  Last Reward Block: 1
  Staking Pool Balance: 30000000000000000000
  Rewards Pool Balance: 0
  Accumulated EVM Per Share: 0
  EVM Rewards: 0
  -----------------------------------------
  Staker1 Initial Staking Amount: 15000000000000000000
  Reward Debt: 0
  Owed: 0
  Pending EVM: 0
  -----------------------------------------
  Staker2 Initial Staking Amount: 10000000000000000000
  Reward Debt: 0
  Owed: 0
  Pending EVM: 0
  -----------------------------------------
  Staker3 Initial Staking Amount: 5000000000000000000
  Reward Debt: 0
  Owed: 0
  Pending EVM: 0
  -----------------------------------------
  

  Block Number: 2
  Last Reward Block: 1
  Rewards Pool Balance: 0
  EVM Rewards: 1000000000000000000
  Accumulated EVM Per Share: 0
  -----------------------------------------
  Staker1
  Reward Debt: 0
  Owed: 0
  Pending EVM: 499999999995000000
  -----------------------------------------
  Staker2
  Reward Debt: 0
  Owed: 0
  Pending EVM: 333333333330000000
  -----------------------------------------
  Staker3
  Reward Debt: 0
  Owed: 0
  Pending EVM: 166666666665000000
  

  Block Number: 3
  Last Reward Block: 1
  Rewards Pool Balance: 0
  EVM Rewards before withdraw: 2000000000000000000
  --Staker1 Withdraw(0)--
  Accumulated EVM Per Share: 66666666666
  Last Reward Block: 3
  EVM Rewards after withdraw: 0
  -----------------------------------------
  Staker1
  Reward Debt: 999999999990000000
  Owed: 999999999990000000
  Pending EVM: 999999999990000000
  -----------------------------------------
  Staker2
  Reward Debt: 0
  Owed: 0
  Pending EVM: 666666666660000000
  -----------------------------------------
  Staker3
  Reward Debt: 0
  Owed: 0
  Pending EVM: 333333333330000000
  

  Block Number: 4
  Last Reward Block: 3
  Rewards Pool Balance: 0
  EVM Rewards: 1000000000000000000
  Accumulated EVM Per Share: 66666666666
  -----------------------------------------
  Staker1
  Reward Debt: 999999999990000000
  Owed: 999999999990000000
  Pending EVM: 1499999999985000000
  -----------------------------------------
  Staker2
  Reward Debt: 0
  Owed: 0
  Pending EVM: 999999999990000000
  -----------------------------------------
  Staker3
  Reward Debt: 0
  Owed: 0
  Pending EVM: 499999999995000000
  

  Block Number: 5
  Last Reward Block: 3
  Rewards Pool Balance: 0
  EVM Rewards: 2000000000000000000
  Accumulated EVM Per Share: 66666666666
  -----------------------------------------
  Staker1
  Reward Debt: 999999999990000000
  Owed: 999999999990000000
  Pending EVM: 1999999999980000000
  -----------------------------------------
  Staker2
  Reward Debt: 0
  Owed: 0
  Pending EVM: 1333333333320000000
  -----------------------------------------
  Staker3
  Reward Debt: 0
  Owed: 0
  Pending EVM: 666666666660000000
  

  Block Number: 6
  Last Reward Block: 3
  Rewards Pool Balance: 0
  EVM Rewards: 3000000000000000000
  Accumulated EVM Per Share: 66666666666
  -----------------------------------------
  Staker1
  Reward Debt: 999999999990000000
  Owed: 999999999990000000
  Pending EVM: 2499999999990000000
  -----------------------------------------
  Staker2
  Reward Debt: 0
  Owed: 0
  Pending EVM: 1666666666660000000
  -----------------------------------------
  Staker3
  Reward Debt: 0
  Owed: 0
  Pending EVM: 833333333330000000
  

  Block Number: 7
  Last Reward Block: 3
  Rewards Pool Balance: 0
  EVM Rewards before withdraw: 4000000000000000000
  Staker1 Pending EVM before withdraw: 2999999999985000000
  --Staker2 Withdraw(0)--
  --Staker3 Withdraw(0)--
  Accumulated EVM Per Share: 199999999999
  Last Reward Block: 7
  EVM Rewards after withdraw: 0
  -----------------------------------------
  Staker1
  Reward Debt: 999999999990000000
  Owed: 999999999990000000
  Pending EVM: 2999999999985000000
  -----------------------------------------
  Staker2
  Reward Debt: 1999999999990000000
  Owed: 1999999999990000000
  Pending EVM: 1999999999990000000
  -----------------------------------------
  Staker3
  Reward Debt: 999999999995000000
  Owed: 999999999995000000
  Pending EVM: 999999999995000000
  

  Block Number: 8
  Last Reward Block: 7
  Rewards Pool Balance: 0
  EVM Rewards: 1000000000000000000
  Accumulated EVM Per Share: 199999999999
  -----------------------------------------
  Staker1 before withdraw
  Reward Debt: 999999999990000000
  Owed: 999999999990000000
  Pending EVM: 3499999999980000000
  --Staker1 Withdraw(0)--
  Accumulated EVM Per Share: 233333333332
  EVM Rewards: 0
  -----------------------------------------
  Staker1
  Reward Debt: 3499999999980000000
  Owed: 3499999999980000000
  Pending EVM: 3499999999980000000
  -----------------------------------------
  Staker2
  Reward Debt: 1999999999990000000
  Owed: 1999999999990000000
  Pending EVM: 2333333333320000000
  -----------------------------------------
  Staker3
  Reward Debt: 999999999995000000
  Owed: 999999999995000000
  Pending EVM: 1166666666660000000
  @@ Add Rewards: 20 ether @@
  

  Block Number: 9
  Last Reward Block: 8
  Rewards Pool Balance: 20000000000000000000
  EVM Rewards before withdraw: 1000000000000000000
  Staker1 Reward Debt before withdraw: 3499999999980000000
  Staker1 Owed before withdraw: 3499999999980000000
  --Staker1 Withdraw(5 ether)--
  Accumulated EVM Per Share: 266666666665
  Last Reward Block: 9
  EVM Rewards after withdraw: 0
  -----------------------------------------
  Staker1
  @@ Here is where the transfer of rewards to the staker1 occurs @@
  New Staker1 Staking Amount: 10000000000000000000
  Reward Debt: 2666666666650000000
  Owed: 0
  Pending EVM: 0
  -----------------------------------------
  Staker2
  Reward Debt: 1999999999990000000
  Owed: 1999999999990000000
  Pending EVM: 2666666666650000000
  -----------------------------------------
  Staker3
  Reward Debt: 999999999995000000
  Owed: 999999999995000000
  Pending EVM: 1333333333325000000
  

  New Rewards Pool Balance: 16000000000025000000
  

  To see the transfer from the contract to the staker/user type 'forge test -vvvv'
  

  This line shows the transaction from:
  Staker1: 0xAb8483F64d9C6d1EcF9b849Ae677dD3315835cb2::fallback{value: 3999999999975000000}() 
  

  Block Number: 10
  Last Reward Block: 9
  Rewards Pool Balance: 16000000000025000000
  EVM Rewards: 1000000000000000000
  Accumulated EVM Per Share: 266666666665
  -----------------------------------------
  Staker1
  Reward Debt: 2666666666650000000
  Owed: 0
  Pending EVM: 400000000000000000
  -----------------------------------------
  Staker2
  Reward Debt: 1999999999990000000
  Owed: 1999999999990000000
  Pending EVM: 3066666666650000000
  -----------------------------------------
  Staker3
  Reward Debt: 999999999995000000
  Owed: 999999999995000000
  Pending EVM: 1533333333325000000
  

  Block Number: 11
  Last Reward Block: 9
  Rewards Pool Balance: 16000000000025000000
  EVM Rewards: 2000000000000000000
  Accumulated EVM Per Share: 266666666665
  Last Reward Block: 9
  EVM Rewards: 2000000000000000000
  Accumulated EVM Per Share: 266666666665
  -----------------------------------------
  Staker1
  Reward Debt: 2666666666650000000
  Owed: 0
  Pending EVM: 800000000000000000
  -----------------------------------------
  Staker2
  Reward Debt: 1999999999990000000
  Owed: 1999999999990000000
  Pending EVM: 3466666666650000000
  -----------------------------------------
  Staker3
  Reward Debt: 999999999995000000
  Owed: 999999999995000000
  Pending EVM: 1733333333325000000
  

  --Staker3 Withdraw(5 ether)--
  

  New Rewards Pool Balance: 14266666666700000000
  Staker3 After withdraw
  New Staker3 Staking Amount: 0
  Reward Debt: 0
  Owed: 0
  Pending EVM: 0
  

  To see the transfer from the contract to the staker/user type 'forge test -vvvv'
  

  Rewards transfer from the stack trace (staker3): 0x78731D3Ca6b7E34aC0F824c42a7cC18A495cabaB::fallback{value: 1733333333325000000}() 
  Staking transfer from the stack trace (staker3): 0x78731D3Ca6b7E34aC0F824c42a7cC18A495cabaB::fallback{value: 5000000000000000000}()
  


[PASS] testSetEVMRewardsPerBlockByUserReverts() (gas: 10935)
[PASS] testSetUpstreamURLOnlyStaker() (gas: 138317)
[PASS] testStakingAmount() (gas: 227647)
Logs:
  User staking amount: 7000000000000000000

[PASS] testStakingBalance() (gas: 219233)
[PASS] testWithdrawStakeAfterEndingPeriod() (gas: 301234)
[PASS] testWithdrawStakeRevertsBeforeEndingPeriodReverts() (gas: 221125)
[PASS] testclaimOwedRewards() (gas: 285526)
Logs:
  Block Number: 1
  Staking amount: 7000000000000000000
  

  Block Number: 15
  -- staker withdraw(7 ether) -- all staking balance --
  Staking amount after withdrawal: 0
  (rewards were not paid with the withdrawal because there are no rewards in the pool balance) Owed: 14000000000000000000
  Staker removed from the Whitelist
  @@ Admin added rewards: 50 ether @@
  Rewards Pool Balance (before claim owed rewards): 50000000000000000000
  Staker Claimed Owed Rewards
  New Rewards Pool Balance after claim owed rewards: 36000000000000000000

Test result: ok. 18 passed; 0 failed; finished in 2.86ms
