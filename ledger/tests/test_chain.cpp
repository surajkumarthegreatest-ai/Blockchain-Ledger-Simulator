#include <cassert>
#include <iostream>

#include "Blockchain.h"
#include "Node.h"
#include "TestHelpers.h"
#include "Wallet.h"

static Wallet alice, bob, miner;

// 4 blocks: genesis, alice mines, alice pays bob 20 (miner mines), bob pays alice 5 (miner mines).
static Blockchain makeChain() {
    Blockchain c(stableConfig());
    c.minePending(alice.address());
    c.addTransaction(pay(c, alice, bob, 20));
    c.minePending(miner.address());
    c.addTransaction(pay(c, bob, alice, 5));
    c.minePending(miner.address());
    return c;
}

int main() {
    // ---------------------------------------------------------------- structure
    {
        Blockchain c = makeChain();
        assert(c.length() == 4);
        assert(c.isValid());
        assert(c.getHead()->prevHash == "0");
        for (int i = 1; i < c.length(); ++i) {
            assert(c.getBlock(i)->prevHash == c.getBlock(i - 1)->hash);
            assert(c.getBlock(i)->index == i);
        }
        assert(c.getBlock(4) == nullptr);
        assert(c.getPending().empty());
        const long long reward = c.getConfig().reward;
        assert(c.getBalance(alice.address()) == reward - 20 + 5);
        assert(c.getBalance(bob.address()) == 20 - 5);
        assert(c.getBalance(miner.address()) == 2 * reward);
    }

    // ---------------------------------------------------------------- tampering
    int bad = -1;
    std::string why;
    {
        Blockchain c = makeChain();                      // edited data, hash untouched
        assert(c.tamper(2, 9999));
        assert(!c.isValid(&bad, &why));
        assert(bad == 2 && contains(why, "hash"));
    }
    {
        Blockchain c = makeChain();                      // hash recomputed but not re-mined
        c.tamper(2, 9999);
        Block* b = const_cast<Block*>(c.getBlock(2));
        b->hash = b->calculateHash();
        assert(!c.isValid(&bad));
        assert(bad == 2 || bad == 3);                    // fails proof of work (or, rarely, the next link)
    }
    {
        Blockchain c = makeChain();                      // block 2 changed AND re-mined
        Block* b = const_cast<Block*>(c.getBlock(2));
        b->timestamp += 1;
        b->mine();
        assert(!c.isValid(&bad, &why));
        assert(bad == 3 && contains(why, "prevHash"));   // block 3 no longer points at it
    }
    {
        Blockchain c = makeChain();                      // broken link, hash re-mined
        Block* b = const_cast<Block*>(c.getBlock(3));
        b->prevHash = "deadbeef";
        b->mine();
        assert(!c.isValid(&bad, &why));
        assert(bad == 3 && contains(why, "prevHash"));
    }
    {
        Blockchain c(stableConfig(1));                   // hash that misses the difficulty
        Block* g = const_cast<Block*>(c.getHead());
        do {
            ++g->nonce;
            g->hash = g->calculateHash();
        } while (Block::meetsDifficulty(g->hash, g->difficulty));
        assert(!c.isValid(&bad, &why));
        assert(bad == 0 && contains(why, "proof-of-work"));
    }

    // ---------------------------------------------------------------- copying
    {
        Blockchain a = makeChain();
        a.addTransaction(pay(a, bob, alice, 1));         // leave one transaction pending
        Blockchain b = a;                                // copy constructor
        assert(b.length() == a.length());
        assert(b.getPending().size() == 1);
        b.tamper(1, 1);
        assert(a.isValid());                             // deep copy: original untouched
        assert(!b.isValid());

        Blockchain d(stableConfig());
        d = a;                                           // copy assignment
        assert(d.length() == a.length() && d.isValid());
        Blockchain& self = d;
        d = self;                                        // self-assignment is safe
        assert(d.length() == a.length() && d.isValid());
    }

    // ---------------------------------------------------------------- nodes
    {
        const ChainConfig cfg = stableConfig();
        Node a("A", cfg), b("B", cfg), c("C", cfg);
        a.mine(alice.address());
        a.addTransaction(pay(a.getChain(), alice, bob, 10));
        a.mine(miner.address());

        assert(b.receiveChain(a.getChain()));            // longer and valid: adopted
        assert(b.getChain().length() == 3 && b.getChain().isValid());
        assert(!b.receiveChain(a.getChain()));           // same amount of work: not adopted

        a.getChain().tamper(2, 500);
        assert(!c.receiveChain(a.getChain()));           // more work but invalid: rejected
        assert(c.getChain().length() == 1);

        Node fresh("F", cfg);
        assert(!a.receiveChain(fresh.getChain()));       // less work: not adopted

        // Same length of work but different rules (reward): refused even though valid.
        ChainConfig other = cfg;
        other.reward = 99;
        Node o("O", other);
        for (int i = 0; i < 3; ++i) o.mine(alice.address());
        assert(o.getChain().isValid() && o.getChain().length() == 4);
        assert(!b.receiveChain(o.getChain()));
        Node same("S", cfg);
        for (int i = 0; i < 3; ++i) same.mine(alice.address());
        assert(b.receiveChain(same.getChain()));         // same rules, more work: adopted
    }

    // Pending transactions that no longer make sense after a chain switch are dropped at mining.
    {
        const ChainConfig cfg = stableConfig();
        Node a("A", cfg), b("B", cfg);
        for (int i = 0; i < 3; ++i) a.mine(miner.address());   // in A's chain alice owns nothing
        b.mine(alice.address());                                // in B's chain alice owns a reward
        assert(b.addTransaction(pay(b.getChain(), alice, bob, 10)));
        assert(b.receiveChain(a.getChain()));
        assert(b.getChain().getPending().size() == 1);          // still waiting
        const Block* blk = b.mine(miner.address());
        assert(blk->txs.size() == 1);                           // only the reward: payment dropped
        assert(b.getChain().getPending().empty());
        assert(b.getChain().isValid());
    }

    std::cout << "test_chain: OK\n";
    return 0;
}
