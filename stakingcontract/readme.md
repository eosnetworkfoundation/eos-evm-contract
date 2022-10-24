TrustEVM Unit Tests Reports (using Foundry)

Install Rust: https://rustup.rs/
Install Foundry: Forge and Cast CLI commands can be installed by running cargo install --git https://github.com/gakonst/foundry --locked
Reference: Foundry Website: https://www.paradigm.xyz/2021/12/introducing-the-foundry-ethereum-development-toolbox

1- Initialize a Foundry repository in the terminal: 
* forge init <foldername>
2- Type: 
* cd <foldername>
3- Copy the test file (extension .t.sol) into “test” folder.
4- Copy the smart contract file (extension .sol) into “src” folder.

Running Tests in the terminal within <foldername>: 
* forge test -vvvv (See all the traces)
* forge test -vvv (Verbose +) **Recommended for a complete report of the rewards algorithm.
* forge test -vv (Verbose)
* forge test (Overview)
