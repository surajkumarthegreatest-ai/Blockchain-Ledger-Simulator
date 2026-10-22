#ifndef SIGNER_H
#define SIGNER_H

#include <string>

// A key pair, both halves hex-encoded DER (private: EC private key, public: SubjectPublicKeyInfo).
struct KeyPair {
    std::string privateKey;
    std::string publicKey;
};

// Thin wrapper around OpenSSL ECDSA (curve secp256k1, the same curve Bitcoin uses).
// The rest of the project never includes an OpenSSL header: only Signer.cpp does.
class Signer {
public:
    // Generates a fresh random secp256k1 key pair. Throws std::runtime_error on failure.
    static KeyPair generateKeyPair();

    // Signs SHA-256(message) with the private key. Returns the DER signature as hex.
    // Throws std::runtime_error if the private key is malformed.
    static std::string sign(const std::string& privateKeyHex, const std::string& message);

    // True only if signatureHex is a valid signature of `message` under publicKeyHex.
    // Never throws: any malformed input simply returns false.
    static bool verify(const std::string& publicKeyHex, const std::string& message,
                       const std::string& signatureHex);

    // An address is the first 40 hex characters of SHA-256(publicKeyHex).
    static std::string addressFromPublicKey(const std::string& publicKeyHex);
};

#endif
