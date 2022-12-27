require("@nomicfoundation/hardhat-toolbox");
require("@nomiclabs/hardhat-web3");

// task action function receives the Hardhat Runtime Environment as second argument
task("accounts", "Prints accounts", async (_, { web3 }) => {
  console.log(await web3.eth.getAccounts());
});

task("blockNumber", "Prints the current block number", async (_, { web3 }) => {
  console.log(await web3.eth.getBlockNumber());
});

task("block", "Prints block")
  .addParam("blocknum", "The block number")
  .setAction(async (taskArgs) => {
    const block = await ethers.provider.getBlockWithTransactions(taskArgs.blocknum);
    console.log(block);
});

task("balance", "Prints an account's balance")
  .addParam("account", "The account's address")
  .setAction(async (taskArgs) => {
    const balance = await ethers.provider.getBalance(taskArgs.account);
    console.log(ethers.utils.formatEther(balance), "ETH");
});

task("nonce", "Prints an account's nonce")
  .addParam("account", "The account's address")
  .setAction(async (taskArgs) => {
    const nonce = await ethers.provider.getTransactionCount(taskArgs.account);
    console.log(nonce);
});

task("transfer", "Send ERC20 tokens")
  .addParam("from", "from account")
  .addParam("to", "to account")
  .addParam("contract", "ERC20 token address")
  .addParam("amount", "amount to trasfer")
  .setAction(async (taskArgs) => {
    const Token = await ethers.getContractFactory('Token')
    const token = Token.attach(taskArgs.contract)
    const res = await token.connect(await ethers.getSigner(taskArgs.from)).transfer(taskArgs.to, ethers.utils.parseEther(taskArgs.amount.toString()),{gasLimit:50000});
    console.log(res);
});

task("send-loop", "Send ERC20 token in a loop")
  .addParam("contract", "Token contract address")
  .setAction(async (taskArgs) => {
    const accounts = await web3.eth.getAccounts();
    const Token = await ethers.getContractFactory('Token')
    const token = Token.attach(taskArgs.contract)
    while(true) {
      let res = [];
      for(let source_index=0; source_index<accounts.length; ++source_index) {
        let destination_index=source_index;
        while(destination_index == source_index) {
          destination_index = parseInt(Math.random()*accounts.length);
        }
        const amount = ethers.utils.parseEther((1+Math.random()*3).toString());
        res.push(token.connect(await ethers.getSigner(source_index)).transfer(accounts[destination_index], amount));
      }
      const rrr = await Promise.all(res);
      rrr.forEach((r,i)=>{
        console.log("txhash => ", r.hash);
      });
    }
});

task("storage-loop", "Store incremental values to the storage contract")
  .addParam("contract", "Token contract address")
  .setAction(async (taskArgs) => {
    const Storage = await ethers.getContractFactory('Storage')
    const storage = Storage.attach(taskArgs.contract)
    let value = 1;
    while(true) {
      const res = await storage.store(value, {gasLimit:50000})
      console.log("############################################ "+ value +" STORED #######");
      ++value;
    }
});


task("load-erc20", "Load erc20 tokens on every account")
  .addParam("contract", "Token contract address")
  .setAction(async (taskArgs) => {
    const accounts = await web3.eth.getAccounts();
    const Token = await ethers.getContractFactory('Token')
    const token = Token.attach(taskArgs.contract);

    for(let i=0; i<7; ++i) {
      const amount = ethers.utils.parseEther((1e6/8).toString());
      const r = await token.connect(await ethers.getSigner(0)).transfer(accounts[i+1], amount);
      console.log(`0 => ${i+1} : ${r.hash}`);
    }

    for(let i=0; i<10; ++i) {
      const amount = ethers.utils.parseEther((1e6/8/10).toString());
    
      let res = [];
      for(let j=0; j<8; ++j) {
        res.push(token.connect(await ethers.getSigner(j)).transfer(accounts[i*8+j], amount));
      }

      const r = await Promise.all(res);
      console.log(`${i+1}/10`);
    }
});

task("erc20-balance", "Prints erc20 balance")
  .addParam("contract", "The erc20 contract address")
  .addParam("account", "The account's address")
  .setAction(async (taskArgs) => {
    const Token = await ethers.getContractFactory('Token')
    const token = Token.attach(taskArgs.contract)
    const balance = await token.balanceOf(taskArgs.account);
    console.log(balance)
});

task("all-erc20-balance", "Prints erc20 balance of all accounts")
  .addParam("contract", "The erc20 contract address")
  .setAction(async (taskArgs) => {
    const Token = await ethers.getContractFactory('Token')
    const token = Token.attach(taskArgs.contract)
    const accounts = await web3.eth.getAccounts();
    
    let res = []
    for(let i=0; i<accounts.length; ++i) {
      res.push(token.balanceOf(accounts[i]));
    }

    const r = await Promise.all(res);
    r.forEach((b,i) => {
      console.log(accounts[i], b);
    });

});



/** @type import('hardhat/config').HardhatUserConfig */
module.exports = {
  defaultNetwork: "ltrust",
  networks: {
    ltrust: {
      url: "http://localhost:5000",
      accounts: {
        mnemonic: "test test test test test test test test test test test junk",
        path: "m/44'/60'/0'/0",
        initialIndex: 0,
        count: 80,
        passphrase: "",
      },
    }
  },
  solidity: "0.8.17",
};
