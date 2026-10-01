# Prerequisites & Planning for the Blockchain Ledger Simulator (C++)

## 1. Concepts to understand first

**Blockchain fundamentals**
- What a block contains: index, timestamp, data/transactions, previous hash, own hash, nonce
- Why each block stores the previous block's hash (this is the "chain", and it makes tampering detectable)
- Genesis block: the first block, with no real predecessor
- Chain validation: recompute every hash and check each link

**Cryptographic hashing**
- Properties of a hash function: deterministic, fixed-size output, avalanche effect, one-way, collision-resistant
- SHA-256 is the standard choice. Know what it does, not how to implement it
- Hex representation of digests

**Proof of Work**
- Goal: find a nonce so that `hash(block)` starts with N zeros (the "difficulty")
- Mining is just a brute-force loop: increment nonce, rehash, check the prefix
- Difficulty trade-off: higher difficulty means exponentially slower mining. Plan to keep it around 3-5 for a demo
- Why tampering requires re-mining all subsequent blocks

**Decentralization (simulated)**
- Since this is a simulator, decide how "nodes" are represented (objects in one process, not real networking)
- Basic idea of consensus: the longest valid chain wins

## 2. C++ skills to revise

| Requirement | What to be comfortable with |
|---|---|
| **OOP node structures** | Classes, constructors, encapsulation, getters, `const` correctness, maybe inheritance/abstract `Hasher` interface |
| **Linked lists** | Pointers, `std::unique_ptr`/`shared_ptr` vs raw pointers, node ownership, traversal, append |
| **STL** | `std::vector`, `std::string`, `std::map`/`unordered_map` (e.g., balances), `std::chrono` for timestamps, `std::stringstream` for hex formatting |
| **Hashing wrappers** | Writing a class that wraps a library call so the rest of your code never touches the library directly |
| **Misc** | Header/source split, include guards, namespaces, exceptions, basic CMake or Makefile |

## 3. Key design decisions (decide before coding)

1. **Hashing library:** implement SHA-256 yourself, use a small header-only implementation, or use OpenSSL (`libssl`). For a mini project, a header-only SHA-256 is the easiest to set up. Wrapping it behind your own `Hasher` class lets you swap later.
2. **Linked list approach:** a custom singly/doubly linked list of `Block` nodes (this is likely what your assignment wants) versus `std::list`. Custom is better for demonstrating the concept.
3. **What a block stores:** a plain string, or a list of `Transaction` objects (sender, receiver, amount)? Transactions make it feel more like a ledger.
4. **What "nodes" mean:** Is a node just a block in the list, or a network participant holding its own copy of the chain? Your requirement says "OOP node structures", so clarify this with your instructor if unsure.
5. **Difficulty:** fixed or adjustable?
6. **Interface:** CLI menu (add transaction, mine block, view chain, validate chain, tamper with a block to demo detection).

## 4. Planned class design (sketch on paper first)

- `Transaction`: sender, receiver, amount
- `Block`: index, timestamp, transactions, prevHash, hash, nonce, `calculateHash()`, `mine(difficulty)`
- `Hasher`: static `sha256(const std::string&)` wrapper
- `Blockchain`: head/tail pointers, `addBlock()`, `isValid()`, `print()`
- `Node` (optional): owns a `Blockchain`, mempool of pending transactions, can mine and sync

## 5. Suggested build order

1. Hash wrapper with a test (known SHA-256 vectors)
2. `Block` with hash calculation
3. Linked list chain with genesis block
4. Chain validation
5. Proof of work and mining
6. Transactions and a pending pool
7. Tamper demo and validation failure output
8. (Optional) multiple nodes and longest-chain sync

## 6. Things to prepare

- A short written spec: features in scope vs. out of scope
- Class diagram (UML) and a flow for "add block → mine → validate"
- Test cases: valid chain, tampered data, tampered hash, broken link
- Project structure: `include/`, `src/`, `main.cpp`, `CMakeLists.txt`
- Compiler setup (g++ with C++17) and a version-control repo

## 7. Common pitfalls

- Forgetting to include the nonce and previous hash in the hashed content
- Memory leaks or dangling pointers in the linked list (use smart pointers or a proper destructor)
- Setting difficulty too high and having mining hang
- Not hashing a consistent serialization of the block, so hashes differ between runs

If you tell me whether your assignment expects a networked simulation or just in-memory nodes, I can help you refine the class design next.
