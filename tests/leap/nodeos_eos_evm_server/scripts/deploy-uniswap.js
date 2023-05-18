const hre = require("hardhat");

async function main() {
  [signer] = await hre.ethers.getSigners();
  const deployer = hre.UniswapV2Deployer;
  const { factory, router, weth9 } = await deployer.deploy(signer);

  console.log("### FACTORY : ", factory.address);
  console.log("### ROUTER : ", router.address);
  console.log("### WETH9 : ", weth9.address);
}

// We recommend this pattern to be able to use async/await everywhere
// and properly handle errors.
main().catch((error) => {
  console.error(error);
  process.exitCode = 1;
});