### EVM 0.5.0 performance measurements

The numbers presented in the table below are derived from an analysis performed on the EVM transaction execution inside the EVM runtime contract following the instructions in the [PERFORMANCE](PERFORMANCE.md) document.

The detailed spreadsheet can be accessed at [this link](https://docs.google.com/spreadsheets/d/1AcfoAmHvSWM6iv2zMpj81Erm0mH0gb28Zjve8d1Zjtc/edit?usp=sharing).

Each column on the spreadsheet corresponds to a different aspect of the transaction processing time:

- **Init**: This measures the time it takes from when the transaction is received by the chain controller until control is passed to the smart contract.

- **Decode**: Time taken to decode the EVM transaction.

- **Recover**: Time taken to recover the sender of the EVM transaction.

- **Execute**: This records the actual transaction execution time.

- **Save**: This column captures the duration of the finalization phase where the changes are committed to the blockchain state.

- **Cleanup**: This measures the time taken to tear down the execution environment once the transaction has been processed.

- **Total**: The sum of all the times recorded (time billed by the chain)

Below is a table with the best times recorded in microseconds (us) for `eos-vm-jit` and `eos-vm-jit (OC)`:

```markdown
                    | eos-vm-jit | eos-vm-jit (OC) |
|:-----------------:|:----------:|:---------------:|
| native transfer   |    167 us  |       124 us    | 
| erc20 transfer    |    343 us  |       171 us    |
| uniswap v2 swap   |   1602 us  |       424 us    |
```

#### Measurement Environment

The measurements for this analysis were taken on a machine with the following specifications:

- **CPU**: Intel(R) Xeon(R) E-2286G @ 4.00GHz
- **Memory**: 32GB RAM
- **Operating System**: Ubuntu 22.04

The version of Leap used for these tests was `3.2.2-logtime`.

