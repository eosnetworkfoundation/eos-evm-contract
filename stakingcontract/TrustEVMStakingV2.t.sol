// SPDX-License-Identifier: UNLICENSED
pragma solidity ^0.8.17;

import "forge-std/Test.sol";
import "../src/TrustEVMStakingV2.sol";

contract TrustEVMTest is Test {
    TrustEVMStaking trustEVMStaking;
    
    address admin = 0x5B38Da6a701c568545dCfcB03FcB875f56beddC4;
    address staker = 0xAb8483F64d9C6d1EcF9b849Ae677dD3315835cb2;
    address stakerTwo = 0x4B20993Bc481177ec7E8f571ceCaE8A9e22C02db;
    address stakerThree = 0x78731D3Ca6b7E34aC0F824c42a7cC18A495cabaB;
    bytes32 STAKER_ROLE = 0x0103b18bff1250f48d347408776979b4841009bbecc6ddf42ad618d875ec7c4b;

    function setUp() public {
        trustEVMStaking = new TrustEVMStaking(0x5B38Da6a701c568545dCfcB03FcB875f56beddC4, 1000000000000000000, 5);
    }

    function testIsAdmin() public view {
        bool isAdmin = trustEVMStaking.isAdmin(0x5B38Da6a701c568545dCfcB03FcB875f56beddC4);
        assert(isAdmin);
    }

    function testAdminAddStaker() public {
        vm.startPrank(admin);
        trustEVMStaking.addStaker(staker);
        bool isStaker = trustEVMStaking.isStaker(staker);
        assert(isStaker);
    }

    function testAdminChangeRewardsPerBlock() public {
        vm.startPrank(admin);
        trustEVMStaking.setEvmRewardsPerBlock(500000000000000000);
        uint rewardsPerBlock = trustEVMStaking.evmRewardsPerBlock();
        assertEq(rewardsPerBlock, 500000000000000000);
        emit log_named_uint("The Rewards Per Block are now", rewardsPerBlock);
    }

    function testSetEVMRewardsPerBlockByUserReverts() public {
        vm.expectRevert("Restricted to admins");
        trustEVMStaking.setEvmRewardsPerBlock(500000000000000000);
    }

    function testSetUpstreamURLOnlyStaker() public {
        vm.prank(admin);
        trustEVMStaking.addStaker(staker);
        vm.startPrank(staker);
        trustEVMStaking.setUpstreamUrl("http://www.stakerurl.com");
        string memory newUpstreamURL = trustEVMStaking.getUpstreamUrl(staker);
        assertEq("http://www.stakerurl.com", newUpstreamURL);
    }

    function testStakingBalance() public {
        vm.prank(admin);
        trustEVMStaking.addStaker(staker);
        vm.deal(staker, 10 ether);
        vm.prank(staker);
        trustEVMStaking.stake{value: 7 ether}();
        uint256 contractBalance = trustEVMStaking.evmBalance();
        assertEq(contractBalance, 7 ether);
    }

    function testRevokeStakerRole() public {
        vm.startPrank(admin);
        trustEVMStaking.addStaker(staker);
        bool isStaker = trustEVMStaking.isStaker(staker);
        assert(isStaker);
        trustEVMStaking.revokeRole(STAKER_ROLE, staker);
        isStaker = trustEVMStaking.isStaker(staker);
        assertEq(isStaker, false);
    }

    function testRemoveStaker() public {
        vm.startPrank(admin);
        trustEVMStaking.addStaker(staker);
        bool isStaker = trustEVMStaking.isStaker(staker);
        assert(isStaker);
        trustEVMStaking.removeStaker(staker);
        isStaker = trustEVMStaking.isStaker(staker);
        assertEq(isStaker, false);
    }

    function testEVMContractStake() public {
        vm.startPrank(admin);
        trustEVMStaking.addStaker(staker);
        vm.stopPrank();
        vm.deal(staker, 10 ether);
        vm.prank(staker);
        trustEVMStaking.stake{value: 1 ether}();
        assertEq(trustEVMStaking.evmBalance(), 1 ether);
        emit log_named_uint('New staker balance', staker.balance);
        emit log_named_uint('TrustEVM Contract Balance', trustEVMStaking.evmBalance());
    }

    function testStakingAmount() public {
        vm.startPrank(admin);
        trustEVMStaking.addStaker(staker);
        vm.stopPrank();
        vm.deal(staker, 10 ether);
        vm.prank(staker);
        trustEVMStaking.stake{value: 7 ether}();
        (uint256 amount,,,,) = trustEVMStaking.stakingUserInfo(staker);
        emit log_named_uint('User staking amount', amount);
    }

    function testGetAmount() public {
        vm.startPrank(admin);
        trustEVMStaking.addStaker(staker);
        vm.stopPrank();
        vm.deal(staker, 10 ether);
        vm.prank(staker);
        trustEVMStaking.stake{value: 7 ether}();
        uint getAmount = trustEVMStaking.getAmount(staker);
        assertEq(getAmount, 7 ether);
    }

    function testWithdrawStakeRevertsBeforeEndingPeriodReverts() public {
        vm.startPrank(admin);
        trustEVMStaking.addStaker(staker);
        vm.stopPrank();
        vm.deal(staker, 10 ether);
        vm.startPrank(staker);
        trustEVMStaking.stake{value: 7 ether}();
        vm.roll(3);                                                 // moves 5 blocks ahead
        vm.expectRevert("staking period is not complete");
        trustEVMStaking.withdraw(5 ether);
    }

    function testWithdrawStakeAfterEndingPeriod() public {
        vm.startPrank(admin);
        trustEVMStaking.addStaker(staker);
        vm.stopPrank();
        vm.deal(staker, 10 ether);
        vm.startPrank(staker);
        trustEVMStaking.stake{value: 7 ether}();
        vm.roll(7);                                                 // moves 7 blocks ahead
        trustEVMStaking.withdraw(5 ether);
    }

    function testPendingEVMForOneStaker() public { 
        vm.startPrank(admin);
        trustEVMStaking.addStaker(staker);
        trustEVMStaking.addStaker(stakerTwo);
        vm.stopPrank();
        vm.deal(staker, 5 ether);
        vm.deal(stakerTwo, 5 ether);
        vm.prank(staker);
        trustEVMStaking.stake{value: 5 ether}();
        vm.prank(stakerTwo);
        vm.roll(2);
        trustEVMStaking.stake{value: 5 ether}();

        uint rewardsPerBlock = trustEVMStaking.evmRewardsPerBlock();
        (uint256 amount,uint256 rewardDebt,,,) = trustEVMStaking.stakingUserInfo(staker);
        emit log("#### Initialization & Formulas #################################################################");
        emit log("################################################################################################");                                                                                        
        emit log_named_uint("EVM Rewards Per Block (1 eth)", rewardsPerBlock);                                                                                        
        emit log("EVM Rewards = Number of Blocks * EVM Rewards Per Block");
        emit log("Accumulated EVM Per Share = Accumulated EVM Per Share + (EVM Rewards * 1e12 / Staking Contract Balance)");
        emit log("PendingEVM = (Staked Amount * Accumulated EVM Per Share / 1e12)- Reward Debt");

        emit log("################################################################################################");                                                                                        
        uint stakingBalance = trustEVMStaking.evmBalance();
        emit log_named_uint("Staking Contract Balance", stakingBalance);
        emit log_named_uint("User staking amount", amount);
        emit log("################################################################################################");

        emit log_named_uint('Block Number', block.number);
        emit log_named_uint("Reward Debt", rewardDebt);
        (uint256 balance,uint256 lastRewardBlock,uint256 accumulatedEvmPerShare) = trustEVMStaking.stakingPool();
        emit log_named_uint("Accumulated EVM Per Share", accumulatedEvmPerShare);
        emit log_named_uint("Last Reward Block", lastRewardBlock);
        uint EVMReward = (block.number - lastRewardBlock) * rewardsPerBlock;
        emit log_named_uint("EVM Rewards Per Block", EVMReward);
        uint pending = trustEVMStaking.pendingEvm(staker);
        emit log_named_uint("Pending EVM", pending);

        vm.roll(3);
        emit log("---------------");                                                 
        pending = trustEVMStaking.pendingEvm(staker);
        emit log_named_uint('Block Number', block.number);
        emit log_named_uint("Reward Debt", rewardDebt);
        (balance, lastRewardBlock, accumulatedEvmPerShare) = trustEVMStaking.stakingPool();
        emit log_named_uint("Accumulated EVM Per Share", accumulatedEvmPerShare);
        emit log_named_uint("Last Reward Block", lastRewardBlock);
        EVMReward = (block.number - lastRewardBlock) * rewardsPerBlock;
        emit log_named_uint("EVM Rewards Per Block", EVMReward);
        emit log_named_uint("Pending EVM", pending); 

        vm.roll(4); 
        emit log("---------------");                                                 
        pending = trustEVMStaking.pendingEvm(staker);
        emit log_named_uint('Block Number', block.number);
        emit log_named_uint("Reward Debt", rewardDebt);
        (balance, lastRewardBlock, accumulatedEvmPerShare) = trustEVMStaking.stakingPool();
        emit log_named_uint("Accumulated EVM Per Share", accumulatedEvmPerShare);
        emit log_named_uint("Last Reward Block", lastRewardBlock);
        EVMReward = (block.number - lastRewardBlock) * rewardsPerBlock;
        emit log_named_uint("EVM Rewards Per Block", EVMReward);
        emit log_named_uint("Pending EVM", pending);

        vm.roll(5); 
        emit log("---------------");                                                 
        pending = trustEVMStaking.pendingEvm(staker);
        emit log_named_uint('Block Number', block.number);
        emit log_named_uint("Reward Debt", rewardDebt);
        (balance, lastRewardBlock, accumulatedEvmPerShare) = trustEVMStaking.stakingPool();
        emit log_named_uint("Accumulated EVM Per Share", accumulatedEvmPerShare);
        emit log_named_uint("Last Reward Block", lastRewardBlock);
        EVMReward = (block.number - lastRewardBlock) * rewardsPerBlock;
        emit log_named_uint("EVM Rewards Per Block", EVMReward);
        emit log_named_uint("Pending EVM", pending);
    }

    function testRewardsAlgorithmInspectOneStaker() public { 
        trustEVMStaking = new TrustEVMStaking(0x5B38Da6a701c568545dCfcB03FcB875f56beddC4, 1000000000000000000, 0);

        vm.startPrank(admin);
        trustEVMStaking.addStaker(staker);
        vm.stopPrank();
        vm.deal(staker, 10 ether);
        vm.startPrank(staker);
        trustEVMStaking.stake{value: 7 ether}();
        uint rewardsPerBlock = trustEVMStaking.evmRewardsPerBlock();
        (uint256 amount,uint256 rewardDebt,string memory upstreamUrl,uint256 owedAmount, uint256 stakingPeriodEndBlock) = trustEVMStaking.stakingUserInfo(staker);
        emit log("#### Initialization & Formulas #################################################################");
        emit log("################################################################################################");
        emit log_named_uint("EVM Rewards Per Block (1 EVM)", rewardsPerBlock);                                                                                        
        emit log("EVM Rewards = Number of Blocks * EVM Rewards Per Block");
        emit log("Accumulated EVM Per Share = Accumulated EVM Per Share + (EVM Rewards * 1e12 / Staking Contract Balance) // This updates only when deposit or withdraw");
        emit log("PendingEVM = (Staked Amount * Accumulated EVM Per Share / 1e12) - (Reward Debt - Owed Amount)");
        emit log("################################################################################################"); 
        emit log("\n");
                                                                                               
        emit log_named_uint('Block Number', block.number);
        uint stakingBalance = trustEVMStaking.evmBalance();
        (uint256 balance,uint256 lastRewardBlock,uint256 accumulatedEvmPerShare) = trustEVMStaking.stakingPool();
        emit log_named_uint("Last Reward Block", lastRewardBlock);
        emit log_named_uint("Staking Pool Balance", stakingBalance);
        uint rewardsNewBalance = trustEVMStaking.rewardsPoolBalance();
        emit log_named_uint("Rewards Pool Balance", rewardsNewBalance); 
        emit log_named_uint("Staker1 Initial Staking Amount", amount);
        emit log_named_uint("Accumulated EVM Per Share", accumulatedEvmPerShare);
        uint EVMReward = (block.number - lastRewardBlock) * rewardsPerBlock;
        emit log_named_uint("EVM Rewards", EVMReward);
        emit log_named_uint("Reward Debt", rewardDebt);
        emit log_named_uint("Owed", owedAmount);
        uint pending = trustEVMStaking.pendingEvm(staker);
        emit log_named_uint("Pending EVM", pending);

        vm.roll(2);
        emit log("\n");                                                
        emit log_named_uint('Block Number', block.number);
        (balance, lastRewardBlock, accumulatedEvmPerShare) = trustEVMStaking.stakingPool();
        emit log_named_uint("Last Reward Block", lastRewardBlock);
        rewardsNewBalance = trustEVMStaking.rewardsPoolBalance();
        emit log_named_uint("Rewards Pool Balance", rewardsNewBalance); 
        EVMReward = (block.number - lastRewardBlock) * rewardsPerBlock;
        emit log_named_uint("EVM Rewards", EVMReward);
        emit log_named_uint("Accumulated EVM Per Share", accumulatedEvmPerShare);
        (, rewardDebt, upstreamUrl, owedAmount, stakingPeriodEndBlock) = trustEVMStaking.stakingUserInfo(staker);
        pending = trustEVMStaking.pendingEvm(staker);
        emit log_named_uint("Reward Debt", rewardDebt);
        emit log_named_uint("Owed", owedAmount);
        emit log_named_uint("Pending EVM", pending); 

        vm.roll(3); 
        emit log("\n");
        emit log_named_uint('Block Number', block.number);
        (balance, lastRewardBlock, accumulatedEvmPerShare) = trustEVMStaking.stakingPool();
        emit log_named_uint("Last Reward Block", lastRewardBlock);
        rewardsNewBalance = trustEVMStaking.rewardsPoolBalance();
        emit log_named_uint("Rewards Pool Balance", rewardsNewBalance); 
        EVMReward = (block.number - lastRewardBlock) * rewardsPerBlock;
        emit log_named_uint("EVM Rewards before withdraw", EVMReward);
        trustEVMStaking.withdraw(0);   
        emit log("--Withdraw(0)--");                                     
        (balance, lastRewardBlock, accumulatedEvmPerShare) = trustEVMStaking.stakingPool();
        emit log_named_uint("Accumulated EVM Per Share", accumulatedEvmPerShare);
        emit log_named_uint("Last Reward Block", lastRewardBlock);
        EVMReward = (block.number - lastRewardBlock) * rewardsPerBlock;
        emit log_named_uint("EVM Rewards after withdraw", EVMReward);
        (, rewardDebt, upstreamUrl, owedAmount, stakingPeriodEndBlock) = trustEVMStaking.stakingUserInfo(staker);
        pending = trustEVMStaking.pendingEvm(staker);
        emit log_named_uint("Reward Debt", rewardDebt);
        emit log_named_uint("Owed", owedAmount);
        emit log_named_uint("Pending EVM", pending);

        vm.roll(4); 
        emit log("\n");                                                
        pending = trustEVMStaking.pendingEvm(staker);
        emit log_named_uint('Block Number', block.number);
        emit log_named_uint("Last Reward Block", lastRewardBlock);
        rewardsNewBalance = trustEVMStaking.rewardsPoolBalance();
        emit log_named_uint("Rewards Pool Balance", rewardsNewBalance); 
        EVMReward = (block.number - lastRewardBlock) * rewardsPerBlock;
        emit log_named_uint("EVM Rewards", EVMReward);
        (balance, lastRewardBlock, accumulatedEvmPerShare) = trustEVMStaking.stakingPool();
        emit log_named_uint("Accumulated EVM Per Share", accumulatedEvmPerShare);
        (, rewardDebt, upstreamUrl, owedAmount, stakingPeriodEndBlock) = trustEVMStaking.stakingUserInfo(staker);
        emit log_named_uint("Reward Debt", rewardDebt);
        emit log_named_uint("Owed", owedAmount);
        emit log_named_uint("Pending EVM", pending);

        vm.roll(5); 
        emit log("\n");                                                
        pending = trustEVMStaking.pendingEvm(staker);
        emit log_named_uint('Block Number', block.number);
        emit log_named_uint("Last Reward Block", lastRewardBlock);
        rewardsNewBalance = trustEVMStaking.rewardsPoolBalance();
        emit log_named_uint("Rewards Pool Balance", rewardsNewBalance);
        EVMReward = (block.number - lastRewardBlock) * rewardsPerBlock;
        emit log_named_uint("EVM Rewards", EVMReward);
        (balance, lastRewardBlock, accumulatedEvmPerShare) = trustEVMStaking.stakingPool();
        emit log_named_uint("Accumulated EVM Per Share", accumulatedEvmPerShare);
        (, rewardDebt, upstreamUrl, owedAmount, stakingPeriodEndBlock) = trustEVMStaking.stakingUserInfo(staker);
        emit log_named_uint("Reward Debt", rewardDebt);
        emit log_named_uint("Owed", owedAmount);
        emit log_named_uint("Pending EVM", pending);

        vm.roll(6); 
        emit log("\n");                                                
        pending = trustEVMStaking.pendingEvm(staker);
        emit log_named_uint('Block Number', block.number);
        emit log_named_uint("Last Reward Block", lastRewardBlock);
        rewardsNewBalance = trustEVMStaking.rewardsPoolBalance();
        emit log_named_uint("Rewards Pool Balance", rewardsNewBalance);
        EVMReward = (block.number - lastRewardBlock) * rewardsPerBlock;
        emit log_named_uint("EVM Rewards", EVMReward);
        (balance, lastRewardBlock, accumulatedEvmPerShare) = trustEVMStaking.stakingPool();
        emit log_named_uint("Accumulated EVM Per Share", accumulatedEvmPerShare);
        (, rewardDebt, upstreamUrl, owedAmount, stakingPeriodEndBlock) = trustEVMStaking.stakingUserInfo(staker);
        emit log_named_uint("Reward Debt", rewardDebt);
        emit log_named_uint("Owed", owedAmount);
        emit log_named_uint("Pending EVM", pending);

        vm.roll(7); 
        emit log("\n");                                                
        emit log_named_uint('Block Number', block.number);
        emit log_named_uint("Last Reward Block", lastRewardBlock);
        rewardsNewBalance = trustEVMStaking.rewardsPoolBalance();
        emit log_named_uint("Rewards Pool Balance", rewardsNewBalance);
        EVMReward = (block.number - lastRewardBlock) * rewardsPerBlock;
        emit log_named_uint("EVM Rewards before withdraw", EVMReward);
        trustEVMStaking.withdraw(0);   
        emit log("--Staker1 Withdraw(0)--");  
        (balance, lastRewardBlock, accumulatedEvmPerShare) = trustEVMStaking.stakingPool();
        emit log_named_uint("Accumulated EVM Per Share", accumulatedEvmPerShare);
        emit log_named_uint("Last Reward Block", lastRewardBlock);
        EVMReward = (block.number - lastRewardBlock) * rewardsPerBlock;
        emit log_named_uint("EVM Rewards after withdraw", EVMReward);
        (, rewardDebt, upstreamUrl, owedAmount, stakingPeriodEndBlock) = trustEVMStaking.stakingUserInfo(staker);
        pending = trustEVMStaking.pendingEvm(staker);
        emit log_named_uint("Reward Debt", rewardDebt);
        emit log_named_uint("Owed", owedAmount);
        emit log_named_uint("Pending EVM", pending);

        vm.roll(8); 
        emit log("\n");                                                
        emit log_named_uint('Block Number', block.number);
        emit log_named_uint("Last Reward Block", lastRewardBlock);
        rewardsNewBalance = trustEVMStaking.rewardsPoolBalance();
        emit log_named_uint("Rewards Pool Balance", rewardsNewBalance);
        EVMReward = (block.number - lastRewardBlock) * rewardsPerBlock;
        emit log_named_uint("EVM Rewards", EVMReward);
        (balance, lastRewardBlock, accumulatedEvmPerShare) = trustEVMStaking.stakingPool();
        emit log_named_uint("Accumulated EVM Per Share", accumulatedEvmPerShare);
        (, rewardDebt, upstreamUrl, owedAmount, stakingPeriodEndBlock) = trustEVMStaking.stakingUserInfo(staker);
        pending = trustEVMStaking.pendingEvm(staker);
        emit log_named_uint("Reward Debt", rewardDebt);
        emit log_named_uint("Owed", owedAmount);
        emit log_named_uint("Pending EVM", pending);
        vm.stopPrank();
        vm.deal(admin, 20 ether);
        vm.prank(admin);
        trustEVMStaking.addRewards{value: 20 ether}();
        emit log("@@ Add Rewards: 20 ether @@");

        vm.roll(9);
        vm.startPrank(staker); 
        emit log("\n");                                                
        emit log_named_uint('Block Number', block.number);
        emit log_named_uint("Last Reward Block", lastRewardBlock);
        rewardsNewBalance = trustEVMStaking.rewardsPoolBalance();
        emit log_named_uint("Rewards Pool Balance", rewardsNewBalance);
        EVMReward = (block.number - lastRewardBlock) * rewardsPerBlock;
        emit log_named_uint("EVM Rewards before withdraw", EVMReward);
        trustEVMStaking.withdraw(0);   
        emit log("--Staker1 Withdraw(0)--");  
        (balance, lastRewardBlock, accumulatedEvmPerShare) = trustEVMStaking.stakingPool();
        emit log_named_uint("Accumulated EVM Per Share", accumulatedEvmPerShare);
        emit log_named_uint("Last Reward Block", lastRewardBlock);
        EVMReward = (block.number - lastRewardBlock) * rewardsPerBlock;
        emit log_named_uint("EVM Rewards after withdraw", EVMReward);
        (, rewardDebt, upstreamUrl, owedAmount, stakingPeriodEndBlock) = trustEVMStaking.stakingUserInfo(staker);
        pending = trustEVMStaking.pendingEvm(staker);
        emit log_named_uint("Reward Debt", rewardDebt);
        emit log_named_uint("@@ Here is where the transfer of rewards to the user occurs @@ Owed", owedAmount);
        emit log_named_uint("Pending EVM", pending);
        emit log("\n");  
        rewardsNewBalance = trustEVMStaking.rewardsPoolBalance();
        emit log_named_uint("New Rewards Pool Balance", rewardsNewBalance); 
        emit log("\n");                                                

        emit log("To see the transfer from the contract to the Staker1 type 'forge test -vvvv'");
        emit log("\n");                                                
        emit log("This line shows the transaction from:");
        emit log("0xAb8483F64d9C6d1EcF9b849Ae677dD3315835cb2::fallback{value: 7999999999992000000}()");
        emit log("\n");                                                
    }   

    function testRewardsAlgorithmInspectThreeStakers() public { 
        trustEVMStaking = new TrustEVMStaking(0x5B38Da6a701c568545dCfcB03FcB875f56beddC4, 1000000000000000000, 0);

        vm.startPrank(admin);
        trustEVMStaking.addStaker(staker);
        trustEVMStaking.addStaker(stakerTwo);
        trustEVMStaking.addStaker(stakerThree);
        vm.stopPrank();
        vm.deal(staker, 20 ether);
        vm.deal(stakerTwo, 20 ether);
        vm.deal(stakerThree, 20 ether);
        vm.prank(staker);
        trustEVMStaking.stake{value: 15 ether}();
        vm.prank(stakerTwo);
        trustEVMStaking.stake{value: 10 ether}();
        vm.prank(stakerThree);
        trustEVMStaking.stake{value: 5 ether}();
        uint rewardsPerBlock = trustEVMStaking.evmRewardsPerBlock();
        (uint256 amount,uint256 rewardDebt,string memory upstreamUrl,uint256 owedAmount, uint256 stakingPeriodEndBlock) = trustEVMStaking.stakingUserInfo(staker);
        emit log("#### Initialization & Formulas #################################################################");
        emit log("################################################################################################");
        emit log_named_uint("EVM Rewards Per Block (1 EVM)", rewardsPerBlock);                                                                                        
        emit log("EVM Rewards = Number of Blocks * EVM Rewards Per Block");
        emit log("Accumulated EVM Per Share = Accumulated EVM Per Share + (EVM Rewards * 1e12 / Staking Contract Balance) // This updates only when deposit or withdraw");
        emit log("PendingEVM = (Staked Amount * Accumulated EVM Per Share / 1e12) - (Reward Debt - Owed Amount)");
        emit log("################################################################################################"); 
        emit log("\n");
                                                                                               
        emit log_named_uint('Block Number', block.number);
        uint stakingBalance = trustEVMStaking.evmBalance();
        (uint256 balance,uint256 lastRewardBlock,uint256 accumulatedEvmPerShare) = trustEVMStaking.stakingPool();
        emit log_named_uint("Last Reward Block", lastRewardBlock);
        emit log_named_uint("Staking Pool Balance", stakingBalance);
        uint rewardsNewBalance = trustEVMStaking.rewardsPoolBalance();
        emit log_named_uint("Rewards Pool Balance", rewardsNewBalance); 
        emit log_named_uint("Accumulated EVM Per Share", accumulatedEvmPerShare);
        uint EVMReward = (block.number - lastRewardBlock) * rewardsPerBlock;
        emit log_named_uint("EVM Rewards", EVMReward);
        emit log("-----------------------------------------");
        emit log_named_uint("Staker1 Initial Staking Amount", amount);
        emit log_named_uint("Reward Debt", rewardDebt);
        emit log_named_uint("Owed", owedAmount);
        uint pending = trustEVMStaking.pendingEvm(staker);
        emit log_named_uint("Pending EVM", pending);
        emit log("-----------------------------------------");
        (amount, rewardDebt, upstreamUrl, owedAmount, ) = trustEVMStaking.stakingUserInfo(stakerTwo);
        emit log_named_uint("Staker2 Initial Staking Amount", amount);
        emit log_named_uint("Reward Debt", rewardDebt);
        emit log_named_uint("Owed", owedAmount);
        pending = trustEVMStaking.pendingEvm(staker);
        emit log_named_uint("Pending EVM", pending);
        emit log("-----------------------------------------");
        (amount, rewardDebt, upstreamUrl, owedAmount, ) = trustEVMStaking.stakingUserInfo(stakerThree);
        emit log_named_uint("Staker3 Initial Staking Amount", amount);
        emit log_named_uint("Reward Debt", rewardDebt);
        emit log_named_uint("Owed", owedAmount);
        pending = trustEVMStaking.pendingEvm(stakerThree);
        emit log_named_uint("Pending EVM", pending);
        emit log("-----------------------------------------");

        vm.roll(2);
        emit log("\n");                                                
        emit log_named_uint('Block Number', block.number);
        (balance, lastRewardBlock, accumulatedEvmPerShare) = trustEVMStaking.stakingPool();
        emit log_named_uint("Last Reward Block", lastRewardBlock);
        rewardsNewBalance = trustEVMStaking.rewardsPoolBalance();
        emit log_named_uint("Rewards Pool Balance", rewardsNewBalance); 
        EVMReward = (block.number - lastRewardBlock) * rewardsPerBlock;
        emit log_named_uint("EVM Rewards", EVMReward);
        emit log_named_uint("Accumulated EVM Per Share", accumulatedEvmPerShare);
        emit log("-----------------------------------------");
        emit log("Staker1");
        (, rewardDebt, upstreamUrl, owedAmount, stakingPeriodEndBlock) = trustEVMStaking.stakingUserInfo(staker);
        pending = trustEVMStaking.pendingEvm(staker);
        emit log_named_uint("Reward Debt", rewardDebt);
        emit log_named_uint("Owed", owedAmount);
        emit log_named_uint("Pending EVM", pending);
        emit log("-----------------------------------------");
        emit log("Staker2");
        (, rewardDebt, upstreamUrl, owedAmount, stakingPeriodEndBlock) = trustEVMStaking.stakingUserInfo(stakerTwo);
        pending = trustEVMStaking.pendingEvm(stakerTwo);
        emit log_named_uint("Reward Debt", rewardDebt);
        emit log_named_uint("Owed", owedAmount);
        emit log_named_uint("Pending EVM", pending); 
        emit log("-----------------------------------------");
        emit log("Staker3");
        (, rewardDebt, upstreamUrl, owedAmount, stakingPeriodEndBlock) = trustEVMStaking.stakingUserInfo(stakerThree);
        pending = trustEVMStaking.pendingEvm(stakerThree);
        emit log_named_uint("Reward Debt", rewardDebt);
        emit log_named_uint("Owed", owedAmount);
        emit log_named_uint("Pending EVM", pending); 

        vm.roll(3); 
        emit log("\n");
        emit log_named_uint('Block Number', block.number);
        (balance, lastRewardBlock, accumulatedEvmPerShare) = trustEVMStaking.stakingPool();
        emit log_named_uint("Last Reward Block", lastRewardBlock);
        rewardsNewBalance = trustEVMStaking.rewardsPoolBalance();
        emit log_named_uint("Rewards Pool Balance", rewardsNewBalance); 
        EVMReward = (block.number - lastRewardBlock) * rewardsPerBlock;
        emit log_named_uint("EVM Rewards before withdraw", EVMReward);
        vm.prank(staker);
        trustEVMStaking.withdraw(0);   
        emit log("--Staker1 Withdraw(0)--");                                     
        (balance, lastRewardBlock, accumulatedEvmPerShare) = trustEVMStaking.stakingPool();
        emit log_named_uint("Accumulated EVM Per Share", accumulatedEvmPerShare);
        emit log_named_uint("Last Reward Block", lastRewardBlock);
        EVMReward = (block.number - lastRewardBlock) * rewardsPerBlock;
        emit log_named_uint("EVM Rewards after withdraw", EVMReward);
        emit log("-----------------------------------------");
        emit log("Staker1");
        (, rewardDebt, upstreamUrl, owedAmount, stakingPeriodEndBlock) = trustEVMStaking.stakingUserInfo(staker);
        pending = trustEVMStaking.pendingEvm(staker);
        emit log_named_uint("Reward Debt", rewardDebt);
        emit log_named_uint("Owed", owedAmount);
        emit log_named_uint("Pending EVM", pending);
        emit log("-----------------------------------------");
        emit log("Staker2");
        (, rewardDebt, upstreamUrl, owedAmount, stakingPeriodEndBlock) = trustEVMStaking.stakingUserInfo(stakerTwo);
        pending = trustEVMStaking.pendingEvm(stakerTwo);
        emit log_named_uint("Reward Debt", rewardDebt);
        emit log_named_uint("Owed", owedAmount);
        emit log_named_uint("Pending EVM", pending);
        emit log("-----------------------------------------");
        emit log("Staker3");
        (, rewardDebt, upstreamUrl, owedAmount, stakingPeriodEndBlock) = trustEVMStaking.stakingUserInfo(stakerThree);
        pending = trustEVMStaking.pendingEvm(stakerThree);
        emit log_named_uint("Reward Debt", rewardDebt);
        emit log_named_uint("Owed", owedAmount);
        emit log_named_uint("Pending EVM", pending);

        vm.roll(4); 
        emit log("\n");                                                
        emit log_named_uint('Block Number', block.number);
        emit log_named_uint("Last Reward Block", lastRewardBlock);
        rewardsNewBalance = trustEVMStaking.rewardsPoolBalance();
        emit log_named_uint("Rewards Pool Balance", rewardsNewBalance); 
        EVMReward = (block.number - lastRewardBlock) * rewardsPerBlock;
        emit log_named_uint("EVM Rewards", EVMReward);
        (balance, lastRewardBlock, accumulatedEvmPerShare) = trustEVMStaking.stakingPool();
        emit log_named_uint("Accumulated EVM Per Share", accumulatedEvmPerShare);
        emit log("-----------------------------------------");
        emit log("Staker1");
        (, rewardDebt, upstreamUrl, owedAmount, stakingPeriodEndBlock) = trustEVMStaking.stakingUserInfo(staker);
        emit log_named_uint("Reward Debt", rewardDebt);
        emit log_named_uint("Owed", owedAmount);
        pending = trustEVMStaking.pendingEvm(staker);
        emit log_named_uint("Pending EVM", pending);
        emit log("-----------------------------------------");
        emit log("Staker2");
        (, rewardDebt, upstreamUrl, owedAmount, stakingPeriodEndBlock) = trustEVMStaking.stakingUserInfo(stakerTwo);
        pending = trustEVMStaking.pendingEvm(stakerTwo);
        emit log_named_uint("Reward Debt", rewardDebt);
        emit log_named_uint("Owed", owedAmount);
        emit log_named_uint("Pending EVM", pending);
        emit log("-----------------------------------------");
        emit log("Staker3");
        (, rewardDebt, upstreamUrl, owedAmount, stakingPeriodEndBlock) = trustEVMStaking.stakingUserInfo(stakerThree);
        pending = trustEVMStaking.pendingEvm(stakerThree);
        emit log_named_uint("Reward Debt", rewardDebt);
        emit log_named_uint("Owed", owedAmount);
        emit log_named_uint("Pending EVM", pending);

        vm.roll(5); 
        emit log("\n");                                                
        emit log_named_uint('Block Number', block.number);
        emit log_named_uint("Last Reward Block", lastRewardBlock);
        rewardsNewBalance = trustEVMStaking.rewardsPoolBalance();
        emit log_named_uint("Rewards Pool Balance", rewardsNewBalance);
        EVMReward = (block.number - lastRewardBlock) * rewardsPerBlock;
        emit log_named_uint("EVM Rewards", EVMReward);
        (balance, lastRewardBlock, accumulatedEvmPerShare) = trustEVMStaking.stakingPool();
        emit log_named_uint("Accumulated EVM Per Share", accumulatedEvmPerShare);
        emit log("-----------------------------------------");
        emit log("Staker1");
        (, rewardDebt, upstreamUrl, owedAmount, stakingPeriodEndBlock) = trustEVMStaking.stakingUserInfo(staker);
        emit log_named_uint("Reward Debt", rewardDebt);
        emit log_named_uint("Owed", owedAmount);
        pending = trustEVMStaking.pendingEvm(staker);
        emit log_named_uint("Pending EVM", pending);
        emit log("-----------------------------------------");
        emit log("Staker2");
        (, rewardDebt, upstreamUrl, owedAmount, stakingPeriodEndBlock) = trustEVMStaking.stakingUserInfo(stakerTwo);
        pending = trustEVMStaking.pendingEvm(stakerTwo);
        emit log_named_uint("Reward Debt", rewardDebt);
        emit log_named_uint("Owed", owedAmount);
        emit log_named_uint("Pending EVM", pending);
        emit log("-----------------------------------------");
        emit log("Staker3");
        (, rewardDebt, upstreamUrl, owedAmount, stakingPeriodEndBlock) = trustEVMStaking.stakingUserInfo(stakerThree);
        pending = trustEVMStaking.pendingEvm(stakerThree);
        emit log_named_uint("Reward Debt", rewardDebt);
        emit log_named_uint("Owed", owedAmount);
        emit log_named_uint("Pending EVM", pending);

        vm.roll(6); 
        emit log("\n");                                                
        emit log_named_uint('Block Number', block.number);
        emit log_named_uint("Last Reward Block", lastRewardBlock);
        rewardsNewBalance = trustEVMStaking.rewardsPoolBalance();
        emit log_named_uint("Rewards Pool Balance", rewardsNewBalance);
        EVMReward = (block.number - lastRewardBlock) * rewardsPerBlock;
        emit log_named_uint("EVM Rewards", EVMReward);
        (balance, lastRewardBlock, accumulatedEvmPerShare) = trustEVMStaking.stakingPool();
        emit log_named_uint("Accumulated EVM Per Share", accumulatedEvmPerShare);
        emit log("-----------------------------------------");
        emit log("Staker1");
        (, rewardDebt, upstreamUrl, owedAmount, stakingPeriodEndBlock) = trustEVMStaking.stakingUserInfo(staker);
        emit log_named_uint("Reward Debt", rewardDebt);
        emit log_named_uint("Owed", owedAmount);
        pending = trustEVMStaking.pendingEvm(staker);
        emit log_named_uint("Pending EVM", pending);
        emit log("-----------------------------------------");
        emit log("Staker2");
        (, rewardDebt, upstreamUrl, owedAmount, stakingPeriodEndBlock) = trustEVMStaking.stakingUserInfo(stakerTwo);
        pending = trustEVMStaking.pendingEvm(stakerTwo);
        emit log_named_uint("Reward Debt", rewardDebt);
        emit log_named_uint("Owed", owedAmount);
        emit log_named_uint("Pending EVM", pending);
        emit log("-----------------------------------------");
        emit log("Staker3");
        (, rewardDebt, upstreamUrl, owedAmount, stakingPeriodEndBlock) = trustEVMStaking.stakingUserInfo(stakerThree);
        pending = trustEVMStaking.pendingEvm(stakerThree);
        emit log_named_uint("Reward Debt", rewardDebt);
        emit log_named_uint("Owed", owedAmount);
        emit log_named_uint("Pending EVM", pending);

        vm.roll(7); 
        emit log("\n");                                                
        emit log_named_uint('Block Number', block.number);
        emit log_named_uint("Last Reward Block", lastRewardBlock);
        rewardsNewBalance = trustEVMStaking.rewardsPoolBalance();
        emit log_named_uint("Rewards Pool Balance", rewardsNewBalance);
        EVMReward = (block.number - lastRewardBlock) * rewardsPerBlock;
        emit log_named_uint("EVM Rewards before withdraw", EVMReward);
        (, rewardDebt, upstreamUrl, owedAmount, stakingPeriodEndBlock) = trustEVMStaking.stakingUserInfo(staker);
        pending = trustEVMStaking.pendingEvm(staker);
        emit log_named_uint("Staker1 Pending EVM before withdraw", pending);

        vm.prank(stakerTwo);
        trustEVMStaking.withdraw(0);   
        emit log("--Staker2 Withdraw(0)--");  
        vm.prank(stakerThree);
        trustEVMStaking.withdraw(0);   
        emit log("--Staker3 Withdraw(0)--");  
        (balance, lastRewardBlock, accumulatedEvmPerShare) = trustEVMStaking.stakingPool();
        emit log_named_uint("Accumulated EVM Per Share", accumulatedEvmPerShare);
        emit log_named_uint("Last Reward Block", lastRewardBlock);
        EVMReward = (block.number - lastRewardBlock) * rewardsPerBlock;
        emit log_named_uint("EVM Rewards after withdraw", EVMReward);
        emit log("-----------------------------------------");
        emit log("Staker1");
        (, rewardDebt, upstreamUrl, owedAmount, stakingPeriodEndBlock) = trustEVMStaking.stakingUserInfo(staker);
        emit log_named_uint("Reward Debt", rewardDebt);
        emit log_named_uint("Owed", owedAmount);
        pending = trustEVMStaking.pendingEvm(staker);
        emit log_named_uint("Pending EVM", pending);
        emit log("-----------------------------------------");
        emit log("Staker2");
        (, rewardDebt, upstreamUrl, owedAmount, stakingPeriodEndBlock) = trustEVMStaking.stakingUserInfo(stakerTwo);
        pending = trustEVMStaking.pendingEvm(stakerTwo);
        emit log_named_uint("Reward Debt", rewardDebt);
        emit log_named_uint("Owed", owedAmount);
        emit log_named_uint("Pending EVM", pending);
        emit log("-----------------------------------------");
        emit log("Staker3");
        (, rewardDebt, upstreamUrl, owedAmount, stakingPeriodEndBlock) = trustEVMStaking.stakingUserInfo(stakerThree);
        pending = trustEVMStaking.pendingEvm(stakerThree);
        emit log_named_uint("Reward Debt", rewardDebt);
        emit log_named_uint("Owed", owedAmount);
        emit log_named_uint("Pending EVM", pending);

        vm.roll(8); 
        emit log("\n");                                                
        emit log_named_uint('Block Number', block.number);
        emit log_named_uint("Last Reward Block", lastRewardBlock);
        rewardsNewBalance = trustEVMStaking.rewardsPoolBalance();
        emit log_named_uint("Rewards Pool Balance", rewardsNewBalance);
        EVMReward = (block.number - lastRewardBlock) * rewardsPerBlock;
        emit log_named_uint("EVM Rewards", EVMReward);
        (balance, lastRewardBlock, accumulatedEvmPerShare) = trustEVMStaking.stakingPool();
        emit log_named_uint("Accumulated EVM Per Share", accumulatedEvmPerShare);
        emit log("-----------------------------------------");
        emit log("Staker1 before withdraw");
        (, rewardDebt, upstreamUrl, owedAmount, stakingPeriodEndBlock) = trustEVMStaking.stakingUserInfo(staker);
        emit log_named_uint("Reward Debt", rewardDebt);
        emit log_named_uint("Owed", owedAmount);
        pending = trustEVMStaking.pendingEvm(staker);
        emit log_named_uint("Pending EVM", pending);
        emit log("--Staker1 Withdraw(0)--");  
        vm.prank(staker);
        trustEVMStaking.withdraw(0);
        (balance, lastRewardBlock, accumulatedEvmPerShare) = trustEVMStaking.stakingPool();
        emit log_named_uint("Accumulated EVM Per Share", accumulatedEvmPerShare);
        EVMReward = (block.number - lastRewardBlock) * rewardsPerBlock;
        emit log_named_uint("EVM Rewards", EVMReward);
        emit log("-----------------------------------------");
        emit log("Staker1");
        (, rewardDebt, upstreamUrl, owedAmount, stakingPeriodEndBlock) = trustEVMStaking.stakingUserInfo(staker);
        emit log_named_uint("Reward Debt", rewardDebt);
        emit log_named_uint("Owed", owedAmount);
        pending = trustEVMStaking.pendingEvm(staker);
        emit log_named_uint("Pending EVM", pending);
        emit log("-----------------------------------------");
        emit log("Staker2");
        (, rewardDebt, upstreamUrl, owedAmount, stakingPeriodEndBlock) = trustEVMStaking.stakingUserInfo(stakerTwo);
        pending = trustEVMStaking.pendingEvm(stakerTwo);
        emit log_named_uint("Reward Debt", rewardDebt);
        emit log_named_uint("Owed", owedAmount);
        emit log_named_uint("Pending EVM", pending);
        emit log("-----------------------------------------");
        emit log("Staker3");
        (, rewardDebt, upstreamUrl, owedAmount, stakingPeriodEndBlock) = trustEVMStaking.stakingUserInfo(stakerThree);
        pending = trustEVMStaking.pendingEvm(stakerThree);
        emit log_named_uint("Reward Debt", rewardDebt);
        emit log_named_uint("Owed", owedAmount);
        emit log_named_uint("Pending EVM", pending);
        vm.stopPrank();
        vm.deal(admin, 20 ether);
        vm.prank(admin);
        trustEVMStaking.addRewards{value: 20 ether}();
        emit log("@@ Add Rewards: 20 ether @@");

        vm.roll(9);
        emit log("\n");                                                
        emit log_named_uint('Block Number', block.number);
        (balance, lastRewardBlock, accumulatedEvmPerShare) = trustEVMStaking.stakingPool();
        emit log_named_uint("Last Reward Block", lastRewardBlock);
        rewardsNewBalance = trustEVMStaking.rewardsPoolBalance();
        emit log_named_uint("Rewards Pool Balance", rewardsNewBalance);
        EVMReward = (block.number - lastRewardBlock) * rewardsPerBlock;
        emit log_named_uint("EVM Rewards before withdraw", EVMReward);
        (amount, rewardDebt, upstreamUrl, owedAmount, ) = trustEVMStaking.stakingUserInfo(staker);
        emit log_named_uint("Staker1 Reward Debt before withdraw", rewardDebt);
        emit log_named_uint("Staker1 Owed before withdraw", owedAmount);
        vm.prank(staker);
        trustEVMStaking.withdraw(5 ether);   
        emit log("--Staker1 Withdraw(5 ether)--");  
        (balance, lastRewardBlock, accumulatedEvmPerShare) = trustEVMStaking.stakingPool();
        emit log_named_uint("Accumulated EVM Per Share", accumulatedEvmPerShare);
        emit log_named_uint("Last Reward Block", lastRewardBlock);
        EVMReward = (block.number - lastRewardBlock) * rewardsPerBlock;
        emit log_named_uint("EVM Rewards after withdraw", EVMReward);
       
        emit log("-----------------------------------------");
        emit log("Staker1");
        emit log("@@ Here is where the transfer of rewards to the staker1 occurs @@");
        (amount, rewardDebt, upstreamUrl, owedAmount, ) = trustEVMStaking.stakingUserInfo(staker);
        emit log_named_uint("New Staker1 Staking Amount", amount);

        emit log_named_uint("Reward Debt", rewardDebt); // doesn't match

        emit log_named_uint("Owed", owedAmount);
        pending = trustEVMStaking.pendingEvm(staker);
        emit log_named_uint("Pending EVM", pending);
       
        emit log("-----------------------------------------");
        emit log("Staker2");
        (, rewardDebt, upstreamUrl, owedAmount, ) = trustEVMStaking.stakingUserInfo(stakerTwo);
        pending = trustEVMStaking.pendingEvm(stakerTwo);
        emit log_named_uint("Reward Debt", rewardDebt);
        emit log_named_uint("Owed", owedAmount);
        emit log_named_uint("Pending EVM", pending);
       
        emit log("-----------------------------------------");
        emit log("Staker3");
        (, rewardDebt, upstreamUrl, owedAmount, ) = trustEVMStaking.stakingUserInfo(stakerThree);
        pending = trustEVMStaking.pendingEvm(stakerThree);
        emit log_named_uint("Reward Debt", rewardDebt);
        emit log_named_uint("Owed", owedAmount);
        emit log_named_uint("Pending EVM", pending);
        emit log("\n");  
        rewardsNewBalance = trustEVMStaking.rewardsPoolBalance();
        emit log_named_uint("New Rewards Pool Balance", rewardsNewBalance); 
        emit log("\n");                                                

        emit log("To see the transfer from the contract to the staker/user type 'forge test -vvvv'");
        emit log("\n");                                                
        emit log("This line shows the transaction from:");
        emit log("Staker1: 0xAb8483F64d9C6d1EcF9b849Ae677dD3315835cb2::fallback{value: 3999999999975000000}() ");

        vm.roll(10); 
        emit log("\n");                                                
        emit log_named_uint('Block Number', block.number);
        emit log_named_uint("Last Reward Block", lastRewardBlock);
        rewardsNewBalance = trustEVMStaking.rewardsPoolBalance();
        emit log_named_uint("Rewards Pool Balance", rewardsNewBalance);
        EVMReward = (block.number - lastRewardBlock) * rewardsPerBlock;
        emit log_named_uint("EVM Rewards", EVMReward);
        (balance, lastRewardBlock, accumulatedEvmPerShare) = trustEVMStaking.stakingPool();
        emit log_named_uint("Accumulated EVM Per Share", accumulatedEvmPerShare);
        emit log("-----------------------------------------");
        emit log("Staker1");
        (, rewardDebt, upstreamUrl, owedAmount, stakingPeriodEndBlock) = trustEVMStaking.stakingUserInfo(staker);
        emit log_named_uint("Reward Debt", rewardDebt);
        emit log_named_uint("Owed", owedAmount);
        pending = trustEVMStaking.pendingEvm(staker);
        emit log_named_uint("Pending EVM", pending);
        emit log("-----------------------------------------");
        emit log("Staker2");
        (, rewardDebt, upstreamUrl, owedAmount, stakingPeriodEndBlock) = trustEVMStaking.stakingUserInfo(stakerTwo);
        pending = trustEVMStaking.pendingEvm(stakerTwo);
        emit log_named_uint("Reward Debt", rewardDebt);
        emit log_named_uint("Owed", owedAmount);
        emit log_named_uint("Pending EVM", pending);
        emit log("-----------------------------------------");
        emit log("Staker3");
        (, rewardDebt, upstreamUrl, owedAmount, stakingPeriodEndBlock) = trustEVMStaking.stakingUserInfo(stakerThree);
        pending = trustEVMStaking.pendingEvm(stakerThree);
        emit log_named_uint("Reward Debt", rewardDebt);
        emit log_named_uint("Owed", owedAmount);
        emit log_named_uint("Pending EVM", pending);             

        vm.roll(11); 
        emit log("\n");                                                
        emit log_named_uint('Block Number', block.number);
        (balance, lastRewardBlock, accumulatedEvmPerShare) = trustEVMStaking.stakingPool();
        emit log_named_uint("Last Reward Block", lastRewardBlock);
        rewardsNewBalance = trustEVMStaking.rewardsPoolBalance();
        emit log_named_uint("Rewards Pool Balance", rewardsNewBalance);
        EVMReward = (block.number - lastRewardBlock) * rewardsPerBlock;
        emit log_named_uint("EVM Rewards", EVMReward);
        (balance, lastRewardBlock, accumulatedEvmPerShare) = trustEVMStaking.stakingPool();
        emit log_named_uint("Accumulated EVM Per Share", accumulatedEvmPerShare);
        (balance, lastRewardBlock, accumulatedEvmPerShare) = trustEVMStaking.stakingPool();
        emit log_named_uint("Last Reward Block", lastRewardBlock);
        EVMReward = (block.number - lastRewardBlock) * rewardsPerBlock;
        emit log_named_uint("EVM Rewards", EVMReward);
        (balance, lastRewardBlock, accumulatedEvmPerShare) = trustEVMStaking.stakingPool();
        emit log_named_uint("Accumulated EVM Per Share", accumulatedEvmPerShare);

        emit log("-----------------------------------------");
        emit log("Staker1");
        (, rewardDebt, upstreamUrl, owedAmount, stakingPeriodEndBlock) = trustEVMStaking.stakingUserInfo(staker);
        emit log_named_uint("Reward Debt", rewardDebt);
        emit log_named_uint("Owed", owedAmount);
        pending = trustEVMStaking.pendingEvm(staker);
        emit log_named_uint("Pending EVM", pending);
        emit log("-----------------------------------------");
        emit log("Staker2");
        (, rewardDebt, upstreamUrl, owedAmount, stakingPeriodEndBlock) = trustEVMStaking.stakingUserInfo(stakerTwo);
        pending = trustEVMStaking.pendingEvm(stakerTwo);
        emit log_named_uint("Reward Debt", rewardDebt);
        emit log_named_uint("Owed", owedAmount);
        emit log_named_uint("Pending EVM", pending);
        emit log("-----------------------------------------");
        emit log("Staker3");
        (, rewardDebt, upstreamUrl, owedAmount, stakingPeriodEndBlock) = trustEVMStaking.stakingUserInfo(stakerThree);
        pending = trustEVMStaking.pendingEvm(stakerThree);
        emit log_named_uint("Reward Debt", rewardDebt);
        emit log_named_uint("Owed", owedAmount);
        emit log_named_uint("Pending EVM", pending);
        emit log("\n");
        emit log("--Staker3 Withdraw(5 ether)--"); 
        emit log("\n");
        vm.prank(stakerThree);
        trustEVMStaking.withdraw(5 ether);
        rewardsNewBalance = trustEVMStaking.rewardsPoolBalance();
        emit log_named_uint("New Rewards Pool Balance", rewardsNewBalance);
        emit log("Staker3 After withdraw");
        (amount, rewardDebt, upstreamUrl, owedAmount, ) = trustEVMStaking.stakingUserInfo(stakerThree);
        pending = trustEVMStaking.pendingEvm(stakerThree);
        emit log_named_uint("New Staker3 Staking Amount", amount);
        emit log_named_uint("Reward Debt", rewardDebt);
        emit log_named_uint("Owed", owedAmount);
        emit log_named_uint("Pending EVM", pending);
        emit log("\n"); 
        emit log("To see the transfer from the contract to the staker/user type 'forge test -vvvv'");
        emit log("\n");    
        emit log("Rewards transfer from the stack trace (staker3): 0x78731D3Ca6b7E34aC0F824c42a7cC18A495cabaB::fallback{value: 1733333333325000000}() ");
        emit log("Staking transfer from the stack trace (staker3): 0x78731D3Ca6b7E34aC0F824c42a7cC18A495cabaB::fallback{value: 5000000000000000000}()");
        emit log("\n");                                   
    }

    function testclaimOwedRewards() public {
        vm.deal(admin, 100 ether);
        vm.prank(admin);
        trustEVMStaking.addStaker(staker);

        vm.deal(staker, 10 ether);
        vm.startPrank(staker);
        trustEVMStaking.stake{value: 7 ether}();
        (uint256 amount,uint256 rewardDebt,,uint256 owedAmount,) = trustEVMStaking.stakingUserInfo(staker);
        emit log_named_uint("Block Number", block.number);
        emit log_named_uint("Staking amount", amount);
        emit log("\n");                                   

        vm.roll(15);
        emit log_named_uint("Block Number", block.number);
        trustEVMStaking.withdraw(7 ether);
        emit log("-- staker withdraw(7 ether) -- all staking balance --");
        (amount, rewardDebt,, owedAmount,) = trustEVMStaking.stakingUserInfo(staker);
        emit log_named_uint("Staking amount after withdrawal", amount);
        emit log_named_uint("(rewards were not paid with the withdrawal because there are no rewards in the pool balance) Owed", owedAmount);
        vm.stopPrank();

        vm.startPrank(admin);
        trustEVMStaking.removeStaker(staker);
        emit log("Staker removed from the Whitelist");
        trustEVMStaking.addRewards{value:50 ether}();
        emit log("@@ Admin added rewards: 50 ether @@");
        uint256 rewardsPoolBalance = trustEVMStaking.rewardsPoolBalance();
        emit log_named_uint("Rewards Pool Balance (before claim owed rewards)", rewardsPoolBalance);
        vm.stopPrank();

        vm.prank(staker);
        trustEVMStaking.claimOwedRewards();
        emit log("Staker Claimed Owed Rewards");
        rewardsPoolBalance = trustEVMStaking.rewardsPoolBalance();
        emit log_named_uint("New Rewards Pool Balance after claim owed rewards", rewardsPoolBalance);
        assertEq(rewardsPoolBalance, 36000000000000000000);
    }

    function testEmergencyWithdraw() public {
        vm.deal(admin, 100 ether);
        vm.prank(admin);
        trustEVMStaking.addStaker(staker);

        vm.deal(staker, 10 ether);
        vm.startPrank(staker);
        trustEVMStaking.stake{value: 5 ether}();
        (uint256 amount,uint256 rewardDebt,,uint256 owedAmount,) = trustEVMStaking.stakingUserInfo(staker);
        emit log_named_uint("Block Number", block.number);
        emit log_named_uint("Staking amount", amount);
        emit log("\n");                                   

        vm.roll(10);
        emit log_named_uint("Block Number", block.number);
        trustEVMStaking.emergencyWithdraw();
        emit log("-- staker Emergencywithdraw() --");

        (amount, rewardDebt,, owedAmount,) = trustEVMStaking.stakingUserInfo(staker);
        emit log_named_uint("Staking amount", amount);
        emit log_named_uint("Owed", owedAmount);
        emit log_named_uint("Reward Debt", rewardDebt);

        assertEq(amount, 0);
        assertEq(owedAmount, 0);
        assertEq(rewardDebt, 0);
    }
}
