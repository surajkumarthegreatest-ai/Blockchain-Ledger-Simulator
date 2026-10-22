#include <cassert>
#include <iostream>

#include "Blockchain.h"
#include "TestHelpers.h"
#include "Wallet.h"

static Wallet alice, bob, miner, mallory;

// A chain where alice already mined one block and so owns one reward.
static Blockchain fundedChain() {
    Blockchain c(stableConfig());
    c.minePending(alice.address());
    return c;
}

// Appends a mined block holding `txs` (skipping every pool rule) and checks that
// validation rejects the chain at that block with a reason containing `expectedReason`.
static void expectRejected(Blockchain& c, const std::vector<Transaction>& txs,
                           const std::string& expectedReason) {
    const int newIndex = c.length();
    c.appendUnchecked(txs);
    int bad = -1;
    std::string why;
    assert(!c.isValid(&bad, &why));
    assert(bad == newIndex);
    assert(contains(why, expectedReason));
}

int main() {
    // ------------------------------------------------------------ rewards and funds
    {
        Blockchain c(stableConfig());
        const long long reward = c.getConfig().reward;
        std::string err;

        // Nobody has coins yet.
        assert(!c.addTransaction(pay(c, alice, bob, 10), &err));
        assert(contains(err, "insufficient"));

        // Mining with an empty pool pays only the reward.
        const Block* b1 = c.minePending(alice.address());
        assert(b1 != nullptr);
        assert(b1->txs.size() == 1 && b1->txs[0].isCoinbase() && b1->txs[0].amount == reward);
        assert(c.getBalance(alice.address()) == reward);
        assert(c.isValid());

        // Spending: pending spends count against the available balance.
        assert(c.addTransaction(pay(c, alice, bob, 20), &err));
        assert(c.getBalance(alice.address()) == reward);             // not confirmed yet
        assert(c.availableBalance(alice.address()) == reward - 20);
        assert(!c.addTransaction(pay(c, alice, bob, reward), &err)); // would overspend
        assert(contains(err, "insufficient"));
        assert(c.addTransaction(pay(c, alice, bob, reward - 20), &err));  // exactly the rest
        assert(c.getPending().size() == 2);

        c.minePending(miner.address());
        assert(c.getPending().empty());
        assert(c.getBalance(alice.address()) == 0);
        assert(c.getBalance(bob.address()) == reward);
        assert(c.getBalance(miner.address()) == reward);
        // Money is conserved: total supply = reward x mined blocks.
        assert(c.getBalance(alice.address()) + c.getBalance(bob.address()) +
                   c.getBalance(miner.address()) == 2 * reward);
        assert(c.isValid());

        // ---------------------------------------------------- signatures
        Transaction forged(alice.address(), mallory.address(), 1, c.nextNonce(alice.address()),
                           mallory.publicKey(), "");
        forged.signature = mallory.sign(forged.signingPayload());
        assert(!c.addTransaction(forged, &err));                     // mallory cannot sign for alice
        assert(contains(err, "signature"));

        Transaction unsignedTx(bob.address(), alice.address(), 1, c.nextNonce(bob.address()));
        assert(!c.addTransaction(unsignedTx, &err));
        assert(contains(err, "signature"));

        // ---------------------------------------------------- replay protection
        Transaction t = pay(c, bob, alice, 5);
        assert(c.addTransaction(t, &err));
        c.minePending(miner.address());
        assert(!c.addTransaction(t, &err));                          // same signed tx again
        assert(contains(err, "nonce"));

        // ---------------------------------------------------- basic rules
        assert(!c.addTransaction(Transaction::coinbase(mallory.address(), reward), &err));
        assert(contains(err, "COINBASE"));
        assert(!c.addTransaction(bob.createTransaction(bob.address(), 1, c.nextNonce(bob.address())), &err));
        assert(contains(err, "differ"));
        assert(!c.addTransaction(bob.createTransaction(alice.address(), 0, c.nextNonce(bob.address())), &err));
        assert(contains(err, "greater than 0"));
        assert(!c.addTransaction(bob.createTransaction(alice.address(), -5, c.nextNonce(bob.address())), &err));
        assert(c.minePending("") == nullptr);
        assert(c.minePending("a|b") == nullptr);
        assert(c.isValid());
    }

    // ------------------------------------------------------------ forged blocks
    // These bypass the pool on purpose; only isValid() can catch them.
    {
        Blockchain c = fundedChain();
        const long long reward = c.getConfig().reward;
        // overspend: alice owns `reward` but spends reward + 1
        expectRejected(c, {Transaction::coinbase(miner.address(), reward),
                           alice.createTransaction(bob.address(), reward + 1, c.nextNonce(alice.address()))},
                       "insufficient");
    }
    {
        Blockchain c = fundedChain();
        // no coinbase at all
        expectRejected(c, {alice.createTransaction(bob.address(), 1, c.nextNonce(alice.address()))},
                       "coinbase");
    }
    {
        Blockchain c = fundedChain();
        // miner pays itself too much
        expectRejected(c, {Transaction::coinbase(miner.address(), c.getConfig().reward + 1)},
                       "coinbase amount");
    }
    {
        Blockchain c = fundedChain();
        // two rewards in one block
        Transaction cb = Transaction::coinbase(miner.address(), c.getConfig().reward);
        expectRejected(c, {cb, cb}, "more than one coinbase");
    }
    {
        Blockchain c = fundedChain();
        // coinbase that carries a signature
        Transaction cb = Transaction::coinbase(miner.address(), c.getConfig().reward);
        cb.signature = "aa";
        expectRejected(c, {cb}, "coinbase must not carry");
    }
    {
        Blockchain c = fundedChain();
        // mallory signs a payment that claims to come from alice
        Transaction forged(alice.address(), mallory.address(), 10, c.nextNonce(alice.address()),
                           mallory.publicKey(), "");
        forged.signature = mallory.sign(forged.signingPayload());
        expectRejected(c, {Transaction::coinbase(miner.address(), c.getConfig().reward), forged},
                       "signature");
    }
    {
        Blockchain c = fundedChain();
        // a genuine signed payment replayed into a later block
        Transaction t = pay(c, alice, bob, 10);
        assert(c.addTransaction(t));
        c.minePending(miner.address());
        assert(c.isValid());
        expectRejected(c, {Transaction::coinbase(miner.address(), c.getConfig().reward), t}, "nonce");
    }

    // ------------------------------------------------------------ tamper detection
    {
        Blockchain c = fundedChain();
        c.addTransaction(pay(c, alice, bob, 10));
        c.minePending(miner.address());
        assert(c.isValid());

        // Changing a signed amount after the fact is caught by the block hash.
        Blockchain copy = c;
        assert(copy.tamper(2, 1));
        int bad = -1;
        std::string why;
        assert(!copy.isValid(&bad, &why));
        assert(bad == 2 && contains(why, "hash"));

        // So is swapping a signature, because signatures are part of the hashed text.
        Blockchain copy2 = c;
        Block* blk = const_cast<Block*>(copy2.getBlock(2));
        blk->txs[1].signature[10] = (blk->txs[1].signature[10] == '0') ? '1' : '0';
        assert(!copy2.isValid(&bad, &why));
        assert(bad == 2 && contains(why, "hash"));

        // Tampering with the genesis block is caught too.
        Blockchain copy3 = c;
        assert(copy3.tamper(0, 5));
        assert(!copy3.isValid(&bad, &why));
        assert(bad == 0);
    }

    std::cout << "test_ledger: OK\n";
    return 0;
}
