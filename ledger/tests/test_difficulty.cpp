#include <cassert>
#include <ctime>
#include <iostream>
#include <vector>

#include "Blockchain.h"
#include "TestHelpers.h"
#include "Wallet.h"

// `count` fake blocks of one difficulty, `spacing` seconds apart (no mining needed).
static std::vector<Block> fakeBlocks(int count, int difficulty, long long spacing) {
    std::vector<Block> blocks;
    blocks.reserve(static_cast<size_t>(count));
    for (int i = 0; i < count; ++i) {
        Block b(i, {}, "0", difficulty);
        b.timestamp = 1000 + i * spacing;
        blocks.push_back(b);
    }
    return blocks;
}

static int expectedFor(const std::vector<Block>& blocks, const ChainConfig& cfg) {
    std::vector<const Block*> prefix;
    for (const Block& b : blocks) prefix.push_back(&b);
    return Blockchain::computeExpectedDifficulty(prefix, cfg);
}

int main() {
    // ---- the pure adjustment function ----
    ChainConfig cfg;
    cfg.initialDifficulty = 3;
    cfg.targetBlockSeconds = 10;
    cfg.adjustInterval = 4;    // window = 4 blocks = 3 gaps = 30 s expected
    cfg.minDifficulty = 2;
    cfg.maxDifficulty = 5;

    assert(expectedFor({}, cfg) == 3);                         // first block: initial difficulty
    assert(expectedFor(fakeBlocks(4, 3, 1), cfg) == 4);        // 3 s  << 30 s : harder
    assert(expectedFor(fakeBlocks(4, 3, 10), cfg) == 3);       // 30 s == 30 s : unchanged
    assert(expectedFor(fakeBlocks(4, 3, 25), cfg) == 2);       // 75 s >> 30 s : easier
    assert(expectedFor(fakeBlocks(3, 3, 1), cfg) == 3);        // not on an adjustment boundary
    assert(expectedFor(fakeBlocks(8, 3, 1), cfg) == 4);        // next boundary
    assert(expectedFor(fakeBlocks(4, 5, 1), cfg) == 5);        // clamped at max
    assert(expectedFor(fakeBlocks(4, 2, 100), cfg) == 2);      // clamped at min
    assert(expectedFor(fakeBlocks(4, 3, 5), cfg) == 3);        // exactly half the time: unchanged
    assert(expectedFor(fakeBlocks(4, 3, 20), cfg) == 3);       // exactly double the time: unchanged

    // ---- a real chain speeds up when blocks arrive fast ----
    ChainConfig live;
    live.initialDifficulty = 1;
    live.minDifficulty = 1;
    live.maxDifficulty = 3;
    live.adjustInterval = 3;
    live.targetBlockSeconds = 10;
    Wallet miner;
    Blockchain c(live);
    c.minePending(miner.address());
    c.minePending(miner.address());
    assert(c.nextDifficulty() == 2);                           // 3 blocks, mined instantly
    c.minePending(miner.address());
    assert(c.length() == 4);
    assert(c.getBlock(1)->difficulty == 1);
    assert(c.getBlock(2)->difficulty == 1);
    assert(c.getBlock(3)->difficulty == 2);
    assert(c.getTail()->hash.rfind("00", 0) == 0);
    assert(c.isValid());
    assert(c.nextDifficulty() == 2);                           // not a boundary again yet
    assert(c.cumulativeWork() == 16ULL * 3 + 256ULL);          // 16^1 x3 + 16^2

    // A miner cannot grant itself an easier block: the difficulty field is re-derived.
    Block* tail = const_cast<Block*>(c.getTail());
    tail->difficulty = 1;
    tail->mine();                                              // honest-looking PoW at difficulty 1
    int bad = -1;
    std::string why;
    assert(!c.isValid(&bad, &why));
    assert(bad == 3);
    assert(contains(why, "difficulty"));

    // ---- timestamp rules ----
    Blockchain t(stableConfig());
    t.minePending(miner.address());
    t.minePending(miner.address());
    assert(t.isValid());
    Block* last = const_cast<Block*>(t.getTail());
    const long long original = last->timestamp;

    last->timestamp = t.getBlock(1)->timestamp - 5;            // earlier than its parent
    last->mine();
    assert(!t.isValid(&bad, &why));
    assert(bad == 2 && contains(why, "earlier"));

    last->timestamp = static_cast<long long>(std::time(nullptr)) + 3 * 60 * 60;   // 3 h ahead
    last->mine();
    assert(!t.isValid(&bad, &why));
    assert(bad == 2 && contains(why, "future"));

    last->timestamp = original;
    last->mine();
    assert(t.isValid());

    // ---- config values are forced into safe ranges ----
    ChainConfig wild;
    wild.initialDifficulty = 99;
    wild.maxDifficulty = 2;
    wild.minDifficulty = 0;
    wild.adjustInterval = 0;
    wild.targetBlockSeconds = 0;
    wild.reward = -10;
    Blockchain w(wild);
    assert(w.getConfig().initialDifficulty == 2);
    assert(w.getConfig().minDifficulty == 1);
    assert(w.getConfig().adjustInterval == 2);
    assert(w.getConfig().targetBlockSeconds == 1);
    assert(w.getConfig().reward == 0);
    assert(w.isValid());

    std::cout << "test_difficulty: OK\n";
    return 0;
}
