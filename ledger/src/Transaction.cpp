#include "Transaction.h"

#include "Signer.h"

const std::string Transaction::COINBASE = "COINBASE";

Transaction::Transaction(const std::string& sender, const std::string& receiver, long long amount,
                         long long nonce, const std::string& senderPubKey,
                         const std::string& signature)
    : sender(sender),
      receiver(receiver),
      amount(amount),
      nonce(nonce),
      senderPubKey(senderPubKey),
      signature(signature) {}

Transaction Transaction::coinbase(const std::string& miner, long long reward) {
    return Transaction(COINBASE, miner, reward);
}

bool Transaction::isCoinbase() const {
    return sender == COINBASE;
}

std::string Transaction::signingPayload() const {
    return sender + "|" + receiver + "|" + std::to_string(amount) + "|" + std::to_string(nonce);
}

std::string Transaction::toString() const {
    return signingPayload() + "|" + senderPubKey + "|" + signature;
}

static std::string abbreviate(const std::string& s) {
    return s.size() > 12 ? s.substr(0, 8) + ".." : s;
}

std::string Transaction::shortString() const {
    std::string out = abbreviate(sender) + "->" + abbreviate(receiver) + ":" + std::to_string(amount);
    if (!isCoinbase()) out += " (nonce " + std::to_string(nonce) + ")";
    return out;
}

bool Transaction::verifySignature() const {
    if (isCoinbase()) {
        return senderPubKey.empty() && signature.empty();
    }
    if (senderPubKey.empty() || signature.empty()) return false;
    // 1. The claimed sender address must really belong to the public key supplied.
    if (sender != Signer::addressFromPublicKey(senderPubKey)) return false;
    // 2. The signature must be valid for this exact payload under that public key.
    return Signer::verify(senderPubKey, signingPayload(), signature);
}
