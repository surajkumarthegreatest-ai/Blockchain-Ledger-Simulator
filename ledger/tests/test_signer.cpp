#include <cassert>
#include <iostream>
#include <stdexcept>

#include "Signer.h"
#include "Wallet.h"

int main() {
    KeyPair kp = Signer::generateKeyPair();
    KeyPair kp2 = Signer::generateKeyPair();
    assert(!kp.privateKey.empty() && !kp.publicKey.empty());
    assert(kp.privateKey != kp2.privateKey);
    assert(kp.publicKey != kp2.publicKey);

    // Sign / verify round trip.
    std::string sig = Signer::sign(kp.privateKey, "hello");
    assert(!sig.empty());
    assert(Signer::verify(kp.publicKey, "hello", sig));

    // Wrong message, wrong key, damaged signature: all rejected.
    assert(!Signer::verify(kp.publicKey, "hellO", sig));
    assert(!Signer::verify(kp2.publicKey, "hello", sig));
    std::string damaged = sig;
    size_t mid = damaged.size() / 2;
    damaged[mid] = (damaged[mid] == '0') ? '1' : '0';
    assert(!Signer::verify(kp.publicKey, "hello", damaged));

    // verify() never throws on garbage input, it just says no.
    assert(!Signer::verify("", "hello", sig));
    assert(!Signer::verify("zz", "hello", sig));
    assert(!Signer::verify(kp.publicKey, "hello", ""));
    assert(!Signer::verify(kp.publicKey, "hello", "nothex"));

    // sign() with a malformed private key throws.
    bool threw = false;
    try {
        Signer::sign("nothex", "x");
    } catch (const std::runtime_error&) {
        threw = true;
    }
    assert(threw);

    // Addresses: 40 hex chars, deterministic, different per key.
    std::string addr = Signer::addressFromPublicKey(kp.publicKey);
    assert(addr.size() == 40);
    assert(addr == Signer::addressFromPublicKey(kp.publicKey));
    assert(addr != Signer::addressFromPublicKey(kp2.publicKey));

    // Wallet ties it together.
    Wallet w;
    assert(w.address() == Signer::addressFromPublicKey(w.publicKey()));
    assert(Signer::verify(w.publicKey(), "msg", w.sign("msg")));
    Wallet restored(kp);
    assert(restored.address() == addr);

    std::cout << "test_signer: OK\n";
    return 0;
}
