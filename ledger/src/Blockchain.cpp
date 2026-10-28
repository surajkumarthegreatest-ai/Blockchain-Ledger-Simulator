#include "Blockchain.h"

#include <algorithm>
#include <chrono>
#include <iostream>
#include <limits>
#include <unordered_map>

namespace {

// Applies a transaction that has already been checked.
void applyTx(LedgerState& st, const Transaction& tx) {
    if (!tx.isCoinbase()) {
        st.balance[tx.sender] -= tx.amount;
        st.lastNonce[tx.sender] = tx.nonce;
    }
    st.balance[tx.receiver] += tx.amount;
}

bool badName(const std::string& s) {
    return s.empty() || s.find('|') != std::string::npos;
}

// Structural checks on a normal (non-coinbase) transaction.
bool basicChecks(const Transaction& tx, std::string& why) {
    if (badName(tx.sender) || badName(tx.receiver)) {
        why = "sender and receiver must be non-empty and must not contain '|'";
        return false;
    }
    if (tx.sender == tx.receiver) {
        why = "sender and receiver must differ";
        return false;
    }
    if (tx.amount <= 0) {
        why = "amount must be greater than 0";
        return false;
    }
    return true;
}

// Full check of a normal transaction against the ledger state it would be applied to.
bool checkAgainstState(const LedgerState& st, const Transaction& tx, std::string& why) {
    if (!basicChecks(tx, why)) return false;
    if (!tx.verifySignature()) {
        why = "invalid signature";
        return false;
    }
    long long expectedNonce = st.lastNonceOf(tx.sender) + 1;
    if (tx.nonce != expectedNonce) {
        why = "bad nonce (expected " + std::to_string(expectedNonce) + ", got " +
              std::to_string(tx.nonce) + ")";
        return false;
    }
    long long bal = st.balanceOf(tx.sender);
    if (bal < tx.amount) {
        why = "insufficient funds (balance " + std::to_string(bal) + ", needs " +
              std::to_string(tx.amount) + ")";
        return false;
    }
    return true;
}

ChainConfig normalize(ChainConfig c) {
    c.adjustInterval = std::max(2, c.adjustInterval);
    c.targetBlockSeconds = std::max(1, c.targetBlockSeconds);
    c.minDifficulty = std::max(1, c.minDifficulty);
    c.maxDifficulty = std::min(15, std::max(c.minDifficulty, c.maxDifficulty));
    c.initialDifficulty = std::min(c.maxDifficulty, std::max(c.minDifficulty, c.initialDifficulty));
    c.reward = std::max(0LL, c.reward);
    return c;
}

long long nowSeconds() {
    return std::chrono::duration_cast<std::chrono::seconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
}

}  // namespace

// ---------------------------------------------------------------------------
// construction, copying, destruction
// ---------------------------------------------------------------------------

void Blockchain::ensureState() const {
    if (stateDirty) {
        rebuildState();
    }
}

void Blockchain::rebuildState() const {
    confirmedState = LedgerState();
    for (const Block* cur = head; cur != nullptr; cur = cur->next) {
        for (const Transaction& tx : cur->txs) {
            applyTx(confirmedState, tx);
        }
    }
    stateDirty = false;
}

Blockchain::Blockchain(const ChainConfig& config)
    : head(nullptr), tail(nullptr), size(0), cfg(normalize(config)), stateDirty(false) {
    // Genesis block: index 0, prevHash "0", no transactions, mined like any other block.
    Block* genesis = new Block(0, {}, "0", cfg.initialDifficulty);
    genesis->mine();
    appendBlock(genesis);
}

Blockchain::Blockchain(const Blockchain& other)
    : head(nullptr), tail(nullptr), size(0), cfg(other.cfg), pending(other.pending),
      stateDirty(other.stateDirty) {
    copyBlocksFrom(other);
    other.ensureState();
    confirmedState = other.confirmedState;
}

Blockchain& Blockchain::operator=(const Blockchain& other) {
    if (this != &other) {
        Blockchain temp(other);
        std::swap(head, temp.head);
        std::swap(tail, temp.tail);
        std::swap(size, temp.size);
        std::swap(cfg, temp.cfg);
        std::swap(pending, temp.pending);
        std::swap(confirmedState, temp.confirmedState);
        std::swap(stateDirty, temp.stateDirty);
    }
    return *this;
}

Blockchain::~Blockchain() {
    clearBlocks();
}

void Blockchain::appendBlock(Block* block) {
    if (tail == nullptr) {
        head = tail = block;
    } else {
        tail->next = block;
        tail = block;
    }
    ++size;
    if (!stateDirty) {
        for (const Transaction& tx : block->txs) {
            applyTx(confirmedState, tx);
        }
    }
}

void Blockchain::clearBlocks() {
    Block* cur = head;
    while (cur != nullptr) {
        Block* nxt = cur->next;
        delete cur;
        cur = nxt;
    }
    head = tail = nullptr;
    size = 0;
    confirmedState = LedgerState();
    stateDirty = false;
}

void Blockchain::copyBlocksFrom(const Blockchain& other) {
    for (const Block* cur = other.head; cur != nullptr; cur = cur->next) {
        Block* copy = new Block(*cur);   // copies every field, including hash and nonce
        copy->next = nullptr;
        appendBlock(copy);
    }
}

std::vector<const Block*> Blockchain::blockPointers() const {
    std::vector<const Block*> v;
    v.reserve(static_cast<size_t>(size));
    for (const Block* cur = head; cur != nullptr; cur = cur->next) v.push_back(cur);
    return v;
}

// ---------------------------------------------------------------------------
// transactions and mining
// ---------------------------------------------------------------------------

bool Blockchain::addTransaction(const Transaction& tx, std::string* error) {
    std::string why;
    if (tx.isCoinbase()) {
        if (error) *error = "COINBASE transactions can only be created by the miner";
        return false;
    }
    // Ledger state as it will be once everything already pending is mined.
    ensureState();
    LedgerState st = confirmedState;
    for (const Transaction& p : pending) {
        std::string discard;
        if (checkAgainstState(st, p, discard)) {
            applyTx(st, p);
        }
    }
    if (!checkAgainstState(st, tx, why)) {
        if (error) *error = why;
        return false;
    }
    pending.push_back(tx);
    return true;
}

const Block* Blockchain::minePending(const std::string& minerAddress) {
    if (badName(minerAddress) || tail == nullptr) return nullptr;

    int difficulty = nextDifficulty();
    ensureState();
    LedgerState st = confirmedState;

    std::vector<Transaction> txs;
    Transaction reward = Transaction::coinbase(minerAddress, cfg.reward);
    txs.push_back(reward);
    applyTx(st, reward);

    // Re-check each pending transaction against the CURRENT chain (it may have changed
    // since the transaction was accepted, e.g. after replaceChain). Drop any that fail.
    std::string why;
    for (const Transaction& tx : pending) {
        if (checkAgainstState(st, tx, why)) {
            txs.push_back(tx);
            applyTx(st, tx);
        }
    }
    pending.clear();

    long long blockTime = std::max(nowSeconds(), tail->timestamp + 1);
    Block* block = new Block(tail->index + 1, txs, tail->hash, difficulty, blockTime);
    block->mine();
    appendBlock(block);
    return block;
}

long long Blockchain::getBalance(const std::string& address) const {
    ensureState();
    return confirmedState.balanceOf(address);
}

long long Blockchain::availableBalance(const std::string& address) const {
    ensureState();
    LedgerState st = confirmedState;
    for (const Transaction& p : pending) {
        std::string discard;
        if (checkAgainstState(st, p, discard)) {
            applyTx(st, p);
        }
    }
    return st.balanceOf(address);
}

long long Blockchain::nextNonce(const std::string& address) const {
    ensureState();
    LedgerState st = confirmedState;
    for (const Transaction& p : pending) {
        std::string discard;
        if (checkAgainstState(st, p, discard)) {
            applyTx(st, p);
        }
    }
    return st.lastNonceOf(address) + 1;
}

// ---------------------------------------------------------------------------
// difficulty and consensus
// ---------------------------------------------------------------------------

int Blockchain::computeExpectedDifficulty(const std::vector<const Block*>& prefix,
                                          const ChainConfig& config) {
    if (prefix.empty()) return config.initialDifficulty;

    const int n = static_cast<int>(prefix.size());      // blocks that already exist
    const int current = prefix.back()->difficulty;
    if (n % config.adjustInterval != 0) return current; // only adjust every adjustInterval blocks

    // Determine the measurement window:
    // For the very first adjustment epoch (n == adjustInterval), the chain starts at genesis
    // (prefix[0]), so there are (adjustInterval - 1) intervals between blocks 0 and (n - 1).
    // For all subsequent epochs (n > adjustInterval), the window starts at the final block
    // of the PREVIOUS epoch: prefix[n - adjustInterval - 1].
    // This measures exactly `adjustInterval` block intervals with ZERO skipped time between epochs.
    const bool isFirstEpoch = (n == config.adjustInterval);
    const size_t firstIdx = isFirstEpoch ? 0 : static_cast<size_t>(n - config.adjustInterval - 1);
    const Block* first = prefix[firstIdx];
    const Block* last = prefix.back();

    const int numGaps = isFirstEpoch ? (config.adjustInterval - 1) : config.adjustInterval;
    const long long actual = std::max(0LL, last->timestamp - first->timestamp);
    const long long expected = static_cast<long long>(numGaps) * config.targetBlockSeconds;

    int next = current;
    if (actual * 2 < expected) {
        next = current + 1;        // blocks came in far too fast: make mining harder
    } else if (actual > expected * 2) {
        next = current - 1;        // far too slow: make mining easier
    }
    return std::min(config.maxDifficulty, std::max(config.minDifficulty, next));
}

int Blockchain::nextDifficulty() const {
    return computeExpectedDifficulty(blockPointers(), cfg);
}

unsigned long long Blockchain::cumulativeWork() const {
    unsigned long long work = 0;
    for (const Block* cur = head; cur != nullptr; cur = cur->next) {
        int d = std::min(15, std::max(0, cur->difficulty));
        unsigned long long blockWork = 1ULL << (4 * d);   // each hex zero = 16x more expected hashing
        if (std::numeric_limits<unsigned long long>::max() - work < blockWork) {
            return std::numeric_limits<unsigned long long>::max(); // saturate on overflow
        }
        work += blockWork;
    }
    return work;
}

bool Blockchain::replaceChain(const Blockchain& other) {
    if (this == &other) return false;
    if (!(other.cfg == cfg)) return false;       // different rules: not our chain
    if (!other.isValid()) return false;
    if (other.cumulativeWork() <= cumulativeWork()) return false;

    // Exception-safe copy: build new blocks before touching our own
    Block* newHead = nullptr;
    Block* newTail = nullptr;
    int newSize = 0;
    try {
        for (const Block* cur = other.head; cur != nullptr; cur = cur->next) {
            Block* copy = new Block(*cur);
            copy->next = nullptr;
            if (newTail == nullptr) {
                newHead = newTail = copy;
            } else {
                newTail->next = copy;
                newTail = copy;
            }
            ++newSize;
        }
    } catch (...) {
        while (newHead != nullptr) {
            Block* nxt = newHead->next;
            delete newHead;
            newHead = nxt;
        }
        throw;
    }

    clearBlocks();
    head = newHead;
    tail = newTail;
    size = newSize;
    other.ensureState();
    confirmedState = other.confirmedState;
    stateDirty = other.stateDirty;
    return true;
}

// ---------------------------------------------------------------------------
// validation
// ---------------------------------------------------------------------------

bool Blockchain::isValid(int* badIndex, std::string* reason) const {
    auto fail = [&](int idx, const std::string& why) {
        if (badIndex) *badIndex = idx;
        if (reason) *reason = why;
        return false;
    };

    const long long now = nowSeconds();
    const long long maxFuture = 2 * 60 * 60;   // blocks may be at most 2 hours in the future

    LedgerState st;
    std::vector<const Block*> prefix;
    const Block* prev = nullptr;
    int expectedIndex = 0;

    for (const Block* cur = head; cur != nullptr; prev = cur, cur = cur->next, ++expectedIndex) {
        const int idx = expectedIndex;

        if (cur->index != idx) return fail(idx, "block index is out of sequence");
        if (cur->hash != cur->calculateHash()) return fail(idx, "stored hash does not match block contents");

        if (prev == nullptr) {
            if (cur->prevHash != "0") return fail(idx, "genesis prevHash must be \"0\"");
            if (cur->timestamp <= 0) return fail(idx, "genesis timestamp must be positive");
        } else if (cur->prevHash != prev->hash) {
            return fail(idx, "prevHash does not match previous block's hash");
        }

        const int expectedDifficulty = computeExpectedDifficulty(prefix, cfg);
        if (cur->difficulty != expectedDifficulty) {
            return fail(idx, "difficulty " + std::to_string(cur->difficulty) + " does not match required " +
                                 std::to_string(expectedDifficulty));
        }
        if (!Block::meetsDifficulty(cur->hash, cur->difficulty)) {
            return fail(idx, "hash does not meet proof-of-work difficulty");
        }

        if (prev != nullptr && cur->timestamp <= prev->timestamp) {
            return fail(idx, "timestamp is earlier than or equal to the previous block's");
        }
        if (cur->timestamp > now + maxFuture) {
            return fail(idx, "timestamp is too far in the future");
        }

        // ---- ledger rules ----
        if (idx == 0) {
            if (!cur->txs.empty()) return fail(idx, "genesis block must not contain transactions");
        } else {
            if (cur->txs.empty() || !cur->txs[0].isCoinbase()) {
                return fail(idx, "first transaction must be the coinbase reward");
            }
            const Transaction& cb = cur->txs[0];
            if (badName(cb.receiver)) return fail(idx, "coinbase receiver is invalid");
            if (cb.amount != cfg.reward) {
                return fail(idx, "coinbase amount " + std::to_string(cb.amount) +
                                     " does not equal the block reward " + std::to_string(cfg.reward));
            }
            if (!cb.verifySignature()) return fail(idx, "coinbase must not carry a key or signature");
            applyTx(st, cb);

            for (size_t i = 1; i < cur->txs.size(); ++i) {
                const Transaction& tx = cur->txs[i];
                if (tx.isCoinbase()) return fail(idx, "block contains more than one coinbase transaction");
                std::string why;
                if (!checkAgainstState(st, tx, why)) return fail(idx, "transaction " + std::to_string(i) + ": " + why);
                applyTx(st, tx);
            }
        }

        prefix.push_back(cur);
    }
    return true;
}

// ---------------------------------------------------------------------------
// demo / test helpers
// ---------------------------------------------------------------------------

Block* Blockchain::findBlock(int index) {
    for (Block* cur = head; cur != nullptr; cur = cur->next) {
        if (cur->index == index) return cur;
    }
    return nullptr;
}

const Block* Blockchain::getBlock(int index) const {
    for (const Block* cur = head; cur != nullptr; cur = cur->next) {
        if (cur->index == index) return cur;
    }
    return nullptr;
}

bool Blockchain::tamper(int index, long long newAmount) {
    Block* block = findBlock(index);
    if (block == nullptr) return false;
    if (block->txs.empty()) {
        block->txs.push_back(Transaction("Mallory", "Eve", newAmount));
    } else {
        block->txs.back().amount = newAmount;
    }
    stateDirty = true;
    // Deliberately does NOT recompute the hash: that is what validation must catch.
    return true;
}

const Block* Blockchain::appendUnchecked(const std::vector<Transaction>& txs) {
    if (tail == nullptr) return nullptr;
    long long blockTime = std::max(nowSeconds(), tail->timestamp + 1);
    Block* block = new Block(tail->index + 1, txs, tail->hash, nextDifficulty(), blockTime);
    block->mine();
    appendBlock(block);
    return block;
}

// ---------------------------------------------------------------------------
// printing
// ---------------------------------------------------------------------------

void Blockchain::print() const {
    for (const Block* cur = head; cur != nullptr; cur = cur->next) {
        std::cout << "--------------------------------------------\n";
        std::cout << "Block #" << cur->index << "\n";
        std::cout << "  timestamp  : " << cur->timestamp << "\n";
        std::cout << "  difficulty : " << cur->difficulty << "\n";
        std::cout << "  prevHash   : " << cur->prevHash << "\n";
        std::cout << "  hash       : " << cur->hash << "\n";
        std::cout << "  nonce      : " << cur->nonce << "\n";
        if (cur->txs.empty()) {
            std::cout << "  txs        : (none)\n";
        } else {
            std::cout << "  txs        :\n";
            for (const Transaction& tx : cur->txs) {
                std::cout << "    " << tx.shortString() << "\n";
            }
        }
    }
    std::cout << "--------------------------------------------\n";
}