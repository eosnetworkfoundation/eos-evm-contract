TrustEVM Upgradeable Contract Instructions using HardHat and Goerli Testnet:

1- On the terminal, create a project folder: 
* mkdir TrustEVMStakingContract
2- On the terminal, call directory into project folder: 
* cd TrustEVMStakingContract
3 - Place provided “package.json” into the project folder (TrustEVMStakingContract).
4- To install the packages, on the Terminal run: 
* npm i
5- On the terminal run:
* npx hardhat
6- Select “Create a JavaScript project” from the options
* .gitignore - y
* Do you want to install this sample project's dependencies with npm (@nomicfoundation/hardhat-toolbox)? (Y/n) · n
7- Inside the “contracts” folder, delete the sample contract (Lock.sol)
8- Inside the “test” folder, delete the sample test (Lock.js)
9- Inside the “scripts” folder, delete ‘deploy.js’
10- Copy ‘TrustEVMStakingContract_v1.sol’ inside “contracts” folder.
11- Place the provided ‘hardhat.config.js’ into the root directory.
12- Create a .env file on the root directory and populate:
	INFURA_API_KEY=
	ETHERSCAN_API_KEY=
	PRIVATE_KEY=
13- Create a file within the ‘scripts’ folder called ‘deploy_trustevmstaking_v1.js’ with the following content:
const { ethers, upgrades } = require("hardhat");

async function main() {
    const TrustEVMStakingContract = await ethers.getContractFactory("TrustEVMStakingContract_v1");
    const trustEVMStakingContract = await upgrades.deployProxy(TrustEVMStakingContract, [admin_address, evmRewardsPerBlock, evmStakingPeriodLength], {initializer: "initialize"});
    await trustEVMStakingContract.deployed();
    console.log(“TrustEVM Staking Contract deployed to:", trustEVMStakingContract.address);
}

main()
  .then(() => process.exit(0))
  .catch(error => {
    console.error(error);
    process.exit(1);
  });

Important Note: replace admin_address, evmRewardsPerBlock, evmStakingPeriodLength with actual values. 
    * For Example: [0x5B38Da6a701c568545dCfcB03FcB875f56beddC4, 1000000000000000000, 100]

14- Deploy Version 1 of the TrustEVM Staking Contract:
* npx hardhat run --network goerli scripts/deploy_trustevmstaking_v1.js
15- Three smart contracts will be deployed: The implementation contract (in this case TrustEVMStakingContract_v1), the Proxy contract and the Admin Proxy contract.
Verify the implementation contract in Etherscan:
* npx hardhat verify --network goerli <TrustEVMStakingContract_v1 deployed address>
16- We can check the initialization values in the implementation contract but they will all be zero or default values, because the storage is the Proxy contract and the function calls are delegated to the Implementation contract.
17- In Etherscan find the TransparentUpgradeableProxy contract, go to the ‘Contract’ tab and select ‘More Options’ >> ‘Is This a Proxy?’.
18- Verify the Proxy contract and go back to the ‘Contract’ tab, and now use ‘Read As Proxy’, here we will find the initialization values passed upon deployment (this is the constructor version in upgradeable contracts)

Upgrading to TrustEVMStakingContract_v2

1- Place the smart contract TrustEVMStakingContract_v2.sol in the “contracts” folder.
2- Create a new script file called ‘upgrade_trustevmstakingcontract_v2.js’ within the “scripts” folder with the following contents:
const { ethers, upgrades } = require("hardhat");

const PROXY = <proxiy-contract-address> 	For example: “0xfAC375Bd68205Cc452D2CDb068E184e2f285e0c3";

async function main() {
    const TrustEVMStakingContractV2 = await ethers.getContractFactory("TrustEVMStakingContract_v2”);
    await upgrades.upgradeProxy(PROXY, TrustEVMStakingContractV2);
    console.log("TrustEVM Staking Contract V2 Upgraded");
}

main()
  .then(() => process.exit(0))
  .catch(error => {
    console.error(error);
    process.exit(1);
  });

3- The implementation contract will be upgraded in the Proxy contract.
4- Verify the new implementation contract on Etherscan (optional):
* env $(cat .env) npx hardhat verify --network goerli <TrustEVMStakingContractV2-address>
5- Repeat this step, in Etherscan find the TransparentUpgradeableProxy contract, go to the ‘Contract’ tab and select ‘More Options’ >> ‘Is This a Proxy?’.
6- Verify the Proxy contract and go back to the ‘Contract’ tab, and now use ‘Read As Proxy’ and ‘Write As Proxy’ to interact with the new implementation contract (TrustEVM Staking Contract V2)

Example of basic rules for initializers (that replace constructors in upgradeable contracts):
* Notice that the initializer is going to run only once when the first version of the contract is deployed, and will not run in subsequent versions of the implementation (v2, v3, etc).
* We disable the initializer function explicitly in the further versions of the contract to disallow security risks.

Box_v1.sol

// SPDX-License-Identifier: MIT
pragma solidity ^0.8.17;

import "@openzeppelin/contracts-upgradeable/proxy/utils/Initializable.sol";

contract Box is Initializable {
    uint public val;

    function initialize(uint _val) public initializer {
        val = _val;
    }
}


Box_v2.sol (This contract extends v1 by adding one more function and keeping the order of the state variables in the exact position as layed out in v1)

// SPDX-License-Identifier: MIT
pragma solidity ^0.8.17;

import "@openzeppelin/contracts-upgradeable/proxy/utils/Initializable.sol";

contract BoxV2 is Initializable {
    uint public val;

    /// @custom:oz-upgrades-unsafe-allow constructor
    constructor() {
        _disableInitializers();
    }

    function inc() external {
        val += 1;
    }
}
