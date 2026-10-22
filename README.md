# Mini- Project
Blockchain Ledger Simulator: An application simulating a decentralized ledger with cryptographic hashing, block structures, and proof of work. Requires: OOP node structures, linked lists, STL, hashing wrappers.


# Blockchain Ledger Simulator (C++17)

A console simulation of a tamper-evident ledger. Transactions are signed with ECDSA, grouped into
blocks, linked by hash in a linked list, and accepted only after Proof of Work. Miners earn a
reward, spending is checked against balances, and mining difficulty adjusts itself.

## Requirements

- g++ with C++17, CMake 3.10+
- OpenSSL development files (Ubuntu/Debian: `sudo apt install libssl-dev`)

## Build and run

```bash
mkdir build && cd build
cmake ..
cmake --build .
./ledger                   
ctest --output-on-failure   
```

## Layout

| File | Role |
|---|---|
| `include/picosha2.h` | third-party header-only SHA-256 (unchanged) |
| `Hasher` | wrapper: `Hasher::sha256(string)` |
| `Signer` | wrapper around OpenSSL ECDSA (secp256k1): keygen, sign, verify, address |
| `Wallet` | key pair + address; `createTransaction()` returns a signed transaction |
| `Transaction` | sender, receiver, amount, nonce, public key, signature; coinbase for rewards |
| `Block` | list node: index, timestamp, difficulty, txs, prevHash, hash, nonce, next; `mine()` |
| `Blockchain` | linked list owner: genesis, pool, mining, ledger replay, difficulty rule, consensus |
| `Node` | simulated participant with its own chain; `receiveChain()` |
| `tests/` | one assert-based test program per component |

## What `isValid()` checks, per block

1. index is sequential, and the stored hash equals the recomputed hash
2. `prevHash` equals the previous block's hash
3. `difficulty` equals the value the rules require at that position, and the hash meets it
4. timestamp is not before the parent's and not more than 2 hours in the future
5. ledger replay: exactly one coinbase first, paying exactly the reward; every other
   transaction has a valid signature for its sender's address, the next nonce for that sender,
   and a sender balance that covers it

## Design notes

- **Signatures:** an address is the first 40 hex characters of SHA-256(public key). A transaction
  is valid only if its sender address matches its public key and the signature covers
  `sender|receiver|amount|nonce`. The signature is part of the hashed block text.
- **Replay protection:** each sender's nonces must be 1, 2, 3... so a signed transaction can
  never be applied twice.
- **Money:** amounts are `long long` whole coins (no floating-point rounding).
- **Difficulty:** every `adjustInterval` blocks the chain compares the real time taken with the
  target (`targetBlockSeconds`). Less than half the target time: +1. More than double: -1.
  Clamped to `minDifficulty..maxDifficulty`.
- **Consensus:** a node adopts another chain only if it uses the same rules, is valid, and has
  strictly more cumulative work (sum of 16^difficulty).
- Serialization is unambiguous: fields are separated by `|` and each transaction is length-prefixed.
- Private keys live only in memory inside `Wallet`; there is no key storage or encryption.
- Not modelled: transaction fees, networking, Merkle trees.
