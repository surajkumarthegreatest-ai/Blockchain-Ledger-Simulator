#include <cassert>
#include <iostream>

#include "TestHelpers.h"
#include "Transaction.h"
#include "Wallet.h"

int main() {
    // Plain text forms.
    Transaction t("A", "B", 10, 1);
    assert(t.signingPayload() == "A|B|10|1");
    assert(t.toString() == "A|B|10|1||");
    assert(t.shortString() == "A->B:10 (nonce 1)");
    assert(!t.isCoinbase());

    // Coinbase.
    Transaction cb = Transaction::coinbase("M", 50);
    assert(cb.isCoinbase());
    assert(cb.receiver == "M" && cb.amount == 50);
    assert(cb.verifySignature());                 // no key, no signature: correct
    assert(cb.shortString() == "COINBASE->M:50");
    cb.signature = "ab";
    assert(!cb.verifySignature());                // a coinbase must not carry a signature

    // A signed transaction verifies, and the signature is part of the hashed text.
    Wallet w, other;
    Transaction s = w.createTransaction("B", 10, 1);
    assert(s.sender == w.address());
    assert(s.verifySignature());
    assert(contains(s.toString(), s.signature));

    // Changing any signed field breaks the signature.
    Transaction x = s;
    x.amount = 11;
    assert(!x.verifySignature());
    x = s;
    x.receiver = "C";
    assert(!x.verifySignature());
    x = s;
    x.nonce = 2;
    assert(!x.verifySignature());
    x = s;
    x.sender = other.address();
    assert(!x.verifySignature());

    // Someone else cannot sign on behalf of w.
    Transaction spoof(w.address(), "B", 10, 1, other.publicKey(), "");
    spoof.signature = other.sign(spoof.signingPayload());
    assert(!spoof.verifySignature());             // address does not belong to that public key

    // Unsigned or half-signed transactions fail.
    Transaction unsignedTx(w.address(), "B", 10, 1);
    assert(!unsignedTx.verifySignature());
    Transaction noSig(w.address(), "B", 10, 1, w.publicKey(), "");
    assert(!noSig.verifySignature());

    std::cout << "test_transaction: OK\n";
    return 0;
}
