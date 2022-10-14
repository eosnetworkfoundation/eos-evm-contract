// SPDX-License-Identifier: MIT
pragma solidity ^0.8.17;

import "https://github.com/OpenZeppelin/openzeppelin-contracts/blob/master/contracts/access/AccessControlEnumerable.sol";

contract TrustEVMStaking is AccessControlEnumerable {
    
    struct StakingPool {
        uint256 balance;
        uint256 lastRewardBlock;
        uint256 accumulatedEvmPerShare; 
    }

    struct UserInfo {
        uint256 amount;
        uint256 rewardDebt;
        string upstreamUrl;
        uint256 owedAmount;
        uint256 stakingPeriodEndBlock;
    }

    mapping(address => UserInfo) public stakingUserInfo;

    StakingPool public stakingPool = StakingPool({balance: 0, lastRewardBlock: 0, accumulatedEvmPerShare: 0});
    uint256 public rewardsPoolBalance;

    uint256 public evmRewardsPerBlock;

    uint256 public evmStakingPeriodLength;
   
    bytes32 public constant STAKER_ROLE = keccak256('STAKER');

    event Stake(address indexed user, uint256 amount);
    event Withdraw(address indexed user, uint256 amount);
    event EmergencyWithdraw(address indexed user, uint256 amount);
    event AddedReward(uint256 amount, uint256 blockNumber);
    event UpdatePool(uint256 lastRewardBlock, uint256 balance, uint256 accumulatedEvmPerShare);
    event AddToAllowList(address indexed user);
    event RemoveFromAllowList(address indexed user);

    constructor(address _adminAccount, uint256 _evmRewardsPerBlock, uint256 _evmStakingPeriodLength) 
    {
        _setupRole(DEFAULT_ADMIN_ROLE, _adminAccount);
        _setRoleAdmin(STAKER_ROLE, DEFAULT_ADMIN_ROLE);

        evmRewardsPerBlock = _evmRewardsPerBlock;
        evmStakingPeriodLength = _evmStakingPeriodLength;
    }

    function pendingEvm(address _user) external returns (uint256)
    {
        if (block.number > stakingPool.lastRewardBlock && stakingPool.balance != 0) {
            uint256 blocks = block.number - stakingPool.lastRewardBlock;
            uint256 evmReward = blocks * evmRewardsPerBlock;
            stakingPool.accumulatedEvmPerShare += evmReward * 1e12 / stakingPool.balance;
        }

        return (stakingUserInfo[_user].amount * stakingPool.accumulatedEvmPerShare / 1e12) - (stakingUserInfo[_user].rewardDebt - stakingUserInfo[_user].owedAmount);
    }

    function updatePool() public 
    {
        if (block.number > stakingPool.lastRewardBlock) {
            if (stakingPool.balance > 0) {
                uint256 blocks = block.number - stakingPool.lastRewardBlock;
                uint256 evmReward = blocks * evmRewardsPerBlock;

                stakingPool.accumulatedEvmPerShare += evmReward * 1e12 / stakingPool.balance;
            }
                    
            stakingPool.lastRewardBlock = block.number;

            emit UpdatePool(stakingPool.lastRewardBlock, stakingPool.balance, stakingPool.accumulatedEvmPerShare);
        }
    }

    function stake() public payable onlyStaker
    {
        updatePool(); 

        if (stakingUserInfo[msg.sender].amount > 0) {
            uint256 pending = (stakingUserInfo[msg.sender].amount * stakingPool.accumulatedEvmPerShare / 1e12) - stakingUserInfo[msg.sender].rewardDebt;

            stakingUserInfo[msg.sender].owedAmount += pending; // we were going to send it but instead we add it to what we owe the user
        }
        
        stakingUserInfo[msg.sender].amount += msg.value;
        // MasterChefV1 Algorithm
        stakingUserInfo[msg.sender].rewardDebt = stakingUserInfo[msg.sender].amount * stakingPool.accumulatedEvmPerShare / 1e12;
        stakingUserInfo[msg.sender].stakingPeriodEndBlock = block.number + evmStakingPeriodLength;

        stakingPool.balance += msg.value;

        emit Stake(msg.sender, msg.value);
    }

    function withdraw(uint256 _amount) public onlyStaker
    {
        // Checks
        require(stakingUserInfo[msg.sender].amount >= _amount, "requested amount exceeds balance");
        require(block.number > stakingUserInfo[msg.sender].stakingPeriodEndBlock, "staking period is not complete");
        
        // Effects
        updatePool();
        
        // Here I substract the 'owed rewards' to rewardDebt to send them over with the 'current Rewards'
        uint256 pendingAndOwed = (stakingUserInfo[msg.sender].amount * stakingPool.accumulatedEvmPerShare / 1e12) - (stakingUserInfo[msg.sender].rewardDebt - stakingUserInfo[msg.sender].owedAmount);
        
        stakingUserInfo[msg.sender].owedAmount = 0;
        
        // Interactions
        safeEvmTransfer(msg.sender, pendingAndOwed);                               // Transferring the 'rewards' here.

        // Effects
        stakingUserInfo[msg.sender].amount -= _amount;

        stakingUserInfo[msg.sender].rewardDebt = stakingUserInfo[msg.sender].amount * stakingPool.accumulatedEvmPerShare / 1e12;
        stakingPool.balance -= _amount;

        // Interactions
        (bool sent, ) = payable(msg.sender).call{value: _amount}("");       // Transferring the 'staked amount' here.
        require(sent, "failed to send Ether");
        
        emit Withdraw(msg.sender, _amount);
    }

    function emergencyWithdraw() public 
    {
        // Effects
        uint256 amount = stakingUserInfo[msg.sender].amount;

        // Claim gas refunds
        stakingUserInfo[msg.sender].amount = 0;
        stakingUserInfo[msg.sender].rewardDebt = 0;
        stakingUserInfo[msg.sender].owedAmount = 0;
        stakingUserInfo[msg.sender].stakingPeriodEndBlock = 0;

        stakingPool.balance -= amount;

        // Interaction
        (bool sent, ) = payable(msg.sender).call{value: amount}("");
        require(sent, "failed to send EVM");

        emit EmergencyWithdraw(msg.sender, amount);
    }

    function claimOwedRewards() public
    {
        require(block.number > stakingUserInfo[msg.sender].stakingPeriodEndBlock, "staking period is not complete");
        require(stakingUserInfo[msg.sender].owedAmount > 0, "no rewards owed");
        
        uint256 owed = stakingUserInfo[msg.sender].owedAmount;
        stakingUserInfo[msg.sender].owedAmount = 0;

        safeEvmTransfer(msg.sender, owed);
    }

    function addRewards() public payable onlyAdmin 
    {
        rewardsPoolBalance += msg.value;

        emit AddedReward(msg.value, block.number);
    }

    function safeEvmTransfer(address _to, uint256 _amount) internal {
        
        if (_amount > rewardsPoolBalance) {
            
            // Effects
            uint256 poolBalance = rewardsPoolBalance;
            
            stakingUserInfo[_to].owedAmount += _amount - poolBalance;

            rewardsPoolBalance = 0;
            
            // Interactions
            (bool sent, ) = _to.call{value: poolBalance}("");
            require(sent, "failed to send EVM");

        } else {
            // Effects
            rewardsPoolBalance -= _amount;
            
            // Interactions
            (bool sent, ) = _to.call{value: _amount}("");
            require(sent, "failed to send EVM");
        }
    }

    function setEvmRewardsPerBlock(uint256 _newRewardsPerBlock) external onlyAdmin {
        evmRewardsPerBlock = _newRewardsPerBlock;
    }

    function getUpstreamUrl(address _user) external view returns (string memory) {
        return stakingUserInfo[_user].upstreamUrl;
    }

    function setUpstreamUrl (string memory _newUrl) public onlyStaker {
        stakingUserInfo[msg.sender].upstreamUrl = _newUrl;
    }

    function getAmount(address _user) external view returns (uint256) {
        return stakingUserInfo[_user].amount;
    }

    function evmBalance() public view returns (uint256) {
        return address(this).balance;
    }

    // Access Control //

    modifier onlyAdmin()
    {
        require(isAdmin(msg.sender), 'Restricted to admins');
        _;
    }

    modifier onlyStaker()
    {
        require(isStaker(msg.sender), 'Restricted to stakers');
        _;
    }

    function isAdmin(address account) public virtual view returns (bool)
    {
        return hasRole(DEFAULT_ADMIN_ROLE, account);
    }

    function isStaker(address account) public virtual view returns (bool)
    {
        return hasRole(STAKER_ROLE, account);
    }

    function addStaker(address account) public virtual onlyAdmin
    {
        grantRole(STAKER_ROLE, account);

        emit AddToAllowList(account);
    }

    function addAdmin(address account) public virtual onlyAdmin
    {
        grantRole(DEFAULT_ADMIN_ROLE, account);
    }

    function removeStaker(address account) public virtual onlyAdmin
    {
        revokeRole(STAKER_ROLE, account);

        emit RemoveFromAllowList(account);
    }

    function renounceAdmin() public virtual
    {
        renounceRole(DEFAULT_ADMIN_ROLE, msg.sender);
    }

    // MultiCall //

    function multiCall(address[] calldata targets, bytes[] calldata data) external view returns (bytes[] memory) {
        require(targets.length == data.length, "target length != data length");
        bytes[] memory results = new bytes[](data.length);

        for (uint i; i < targets.length; i++) {
            (bool success, bytes memory result) = targets[i].staticcall(data[i]);
            require(success, 'call failed');
            results[i] = result;
        }

        return results;
    }

}
