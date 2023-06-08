const hre = require("hardhat");

function eth(n) {
  return hre.ethers.utils.parseEther(n.toString());
}

async function main() {
  [signer] = await hre.ethers.getSigners();
  const deployer = hre.UniswapV2Deployer;
  const { factory, router, weth9 } = await deployer.deploy(signer);

  console.log("Router deployed: ", router.address);
  console.log("Factory deployed: ", factory.address);
  console.log("WETH9 deployed: ", weth9.address);

  const totalTokes = 26;
  var tokens=[];
  for(var i=0; i<totalTokes; ++i) {
    const symbol = String.fromCharCode('A'.charCodeAt(0)+i).repeat(3);
    const tokenName = "Token "+symbol;

    const Token = await hre.ethers.getContractFactory("Token");
    const token = await Token.deploy(tokenName, symbol);
    await token.deployed();
    console.log(tokenName + " deployed: ", token.address);
    tokens.push({symbol:symbol, token:token});
  }

  for(var i=0; i<tokens.length-1; i++) {
    await tokens[i].token.approve(router.address, eth(1000));
    await tokens[i+1].token.approve(router.address, eth(1000));

    const receipt = await router.addLiquidity(
      tokens[i].token.address,
      tokens[i+1].token.address,
      eth(1000),
      eth(1000),
      0,
      0,
      signer.address,
      Date.now() + 1000*60*10,
    );

    console.log("Liquidity added to pool " + tokens[i].symbol + "/" + tokens[i+1].symbol);
  }


}

// We recommend this pattern to be able to use async/await everywhere
// and properly handle errors.
main().catch((error) => {
  console.error(error);
  process.exitCode = 1;
});
