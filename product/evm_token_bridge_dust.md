## Bridge Dust Buckets

### Background

Ethereum chains store their native token value in a 256-bit number which unit is 'wei'. 1 wei is  10<sup>-18</sup> ETH, that is, representation of the token value stores 18 digits of decimal precision.

Antelope gives considerable flexibility to how a contract can handle its data, but the standard expected practice for a token is to represent its value via an `asset` type which allows for a signed 63-bit value. The `asset` also includes a definition of a `symbol`, and the `symbol` defines the decimal precision as an 8-bit value. For example, if the `symbol` states a decimal precision of 6 digits, then the max value representable in the `asset` is 2<sup>62</sup>/10<sup>6</sup>, i.e. 4611686018427.387904

This difference in number range means that while an Antelope token's `asset` value could represent the full decimal precision of an Ethereum native token value, the range of values will be substantially less when configured that way. Consider that 2<sup>62</sup>/10<sup>18</sup> ≈ 4.6! If the Antelope token were to represent the entire decimal precision of an Ethereum native token it would be impossible to represent 5.000000000000000000 EVM as an Antelope token. Thus, the Antelope token must use considerably less decimal precision so that it can represent the maximum possible value for all expected minted EVM native tokens.

### Dust Buckets

The exact value of the Antelope token's precision with be left to the `init` action to define, and the token contract math is expected to handle any value between 0 and 18. 6 digits will be used in the  examples within this document.

The reduction in precision when bridging the token from EVM to Antelope  would mean that if a user bridges 5.123456789012345678 EVM from EVM to Antelope, Antelope can only represent 5.123456 EVM as an `asset` which users can then `transfer` etc. What to do with the left over "dust" 0.000000789012345678 EVM?

We've elected to have the user's Antelope account retain ownership of this dust (it isn't burned or transferred to another pool). This means that the Antelope contract must maintain an additional dust balance in addition to the standard Antelope `asset` balance a token contract would have.

To minimize impact on RAM usage, this dust balance will be appended to the typical token balance row. That is, the table row definition becomes something like:
```c++
 struct [[eosio::table]] account {
    asset    balance;
    uint64_t dust;

    uint64_t primary_key()const { return balance.symbol.code().raw(); }
 };
```
A 64-bit value is enough to hold all ranges of possible dust: if the contract is `init`ed with an Antelope token precision of 0, `dust` could have a value up to 10<sup>18</sup>-1 which can still fit in 2<sup>64</sup>.

Upon a transfer from EVM to Antelope, the token value modulo configured precision will be accumulated in `dust`. If `dust` ever exceeds the minimal value represented via the Antelope token's precision, that excess value is transferred from `dust` to the Antelope token's standard balance.

This dust balance is inaccessible to the user except for one action: transferring the token from Antelope to EVM (via an EVM transaction). This action has the possibility to bridge the dust back to EVM if the transfer specifies a value utilizing precision more than the Antelope token precision. i.e. this is the only time the user's `balance`+`dust` is truly considered one balance.

### Examples

| action                                              | `balance`    | `dust`       | notes                                                           |
|-----------------------------------------------------|--------------|--------------|-----------------------------------------------------------------|
| user `open`s balance                                | 0.000000 EVM | 0            |
| EVM->Antelope transfer<br/>5.123456789012345678 EVM | 5.123456 EVM | 789012345678 |
| antelope token `transfer`<br/>4.000000 EVM          | 1.123456 EVM | 789012345678 |
| EVM->Antelope transfer<br/>0.000000789012345678 EVM | 1.123457 EVM | 578024691356 | `dust` "rolled over"                                            |
| Antelope->EVM transfer<br/>1.000000400000000000 EVM | 0.123457 EVM | 178024691356 | value less than Antelope `balance` precision pulled from `dust` |
| antelope token `transfer`<br/>0.123457 EVM          | 0.000000 EVM | 178024691356 | user unable to `close`                                          |
| Antelope->EVM transfer<br/>0.000000178024691356 EVM | 0.000000 EVM | 0            | user can `close`                                                |


### Compatibility Impacts

The `eosio.token` interface is the de facto token interface on Antelope chains. Adding more data in to the `account` table rows arguably changes the interface to no longer be fully compliant even though no compliance specification has ever been published unlike ERC20, for example. (there are other deviations too: we do not have a `create` action).

The expectation is that no other contracts, block explorers, or wallets will care about the extra row data: they'll simply deserialize the `balance` and stop; ignoring the remaining bytes in the row. Being disturbed by this extra data would seemingly only come about if some consumer was being deliberately obtuse.

Nevertheless, it is worth empirically testing to ensure the token balance is properly displayed for users in typical use cases. A contract was deployed on Jungle4 lacking both a `create` action and including the extra `account` table row data. Eosq correctly displayed that a token holder had the expected `balance`, ignoring the `dust`. It may be worth trying on another explorers and/or wallets.

### Reponsibliy Of Deployer & `init` Caller

The caller of the contract's `init` action will need to set the Antelope token's precision with consideration of the EVM native token's tokenomics (supply, inflation, etc). For example, while a setting of 0 will "only" allow the Antelope token to represent 4611686018427387904 EVM which is less than what the EVM native token could represent, realistically that may well exceed the maximum supply & inflation of the token in practice.

The bottom line is that the onus is on the caller of `init` to set this number appropriately. The contract will have to be prepared to assert on transactions that overflow the asset should the combination of `init`ed precision and native EVM supply be 'misconfigured'.