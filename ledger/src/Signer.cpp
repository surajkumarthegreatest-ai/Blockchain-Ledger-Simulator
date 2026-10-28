#include "Signer.h"

#include <openssl/ec.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/obj_mac.h>
#include <openssl/x509.h>

#include <memory>
#include <stdexcept>
#include <vector>

#include "Hasher.h"

namespace {

using PkeyPtr = std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)>;
using PkeyCtxPtr = std::unique_ptr<EVP_PKEY_CTX, decltype(&EVP_PKEY_CTX_free)>;
using MdCtxPtr = std::unique_ptr<EVP_MD_CTX, decltype(&EVP_MD_CTX_free)>;

std::string toHex(const unsigned char* data, size_t len) {
    static const char* digits = "0123456789abcdef";
    std::string out;
    out.reserve(len * 2);
    for (size_t i = 0; i < len; ++i) {
        out.push_back(digits[data[i] >> 4]);
        out.push_back(digits[data[i] & 0x0F]);
    }
    return out;
}

int nibble(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

bool fromHex(const std::string& hex, std::vector<unsigned char>& out) {
    out.clear();
    if (hex.empty() || hex.size() % 2 != 0) return false;
    for (size_t i = 0; i < hex.size(); i += 2) {
        int hi = nibble(hex[i]);
        int lo = nibble(hex[i + 1]);
        if (hi < 0 || lo < 0) return false;
        out.push_back(static_cast<unsigned char>(hi * 16 + lo));
    }
    return true;
}

PkeyPtr loadPrivateKey(const std::string& hex) {
    std::vector<unsigned char> der;
    if (!fromHex(hex, der)) return PkeyPtr(nullptr, EVP_PKEY_free);
    const unsigned char* p = der.data();
    return PkeyPtr(d2i_PrivateKey(EVP_PKEY_EC, nullptr, &p, static_cast<long>(der.size())),
                   EVP_PKEY_free);
}

PkeyPtr loadPublicKey(const std::string& hex) {
    std::vector<unsigned char> der;
    if (!fromHex(hex, der)) return PkeyPtr(nullptr, EVP_PKEY_free);
    const unsigned char* p = der.data();
    return PkeyPtr(d2i_PUBKEY(nullptr, &p, static_cast<long>(der.size())), EVP_PKEY_free);
}

}  // namespace

KeyPair Signer::generateKeyPair() {
    PkeyCtxPtr ctx(EVP_PKEY_CTX_new_id(EVP_PKEY_EC, nullptr), EVP_PKEY_CTX_free);
    if (!ctx || EVP_PKEY_keygen_init(ctx.get()) <= 0 ||
        EVP_PKEY_CTX_set_ec_paramgen_curve_nid(ctx.get(), NID_secp256k1) <= 0) {
        throw std::runtime_error("OpenSSL: cannot initialise EC key generation");
    }
    EVP_PKEY* raw = nullptr;
    if (EVP_PKEY_keygen(ctx.get(), &raw) <= 0) {
        throw std::runtime_error("OpenSSL: key generation failed");
    }
    PkeyPtr key(raw, EVP_PKEY_free);

    int privLen = i2d_PrivateKey(key.get(), nullptr);
    int pubLen = i2d_PUBKEY(key.get(), nullptr);
    if (privLen <= 0 || pubLen <= 0) {
        throw std::runtime_error("OpenSSL: cannot encode key");
    }
    std::vector<unsigned char> priv(static_cast<size_t>(privLen));
    std::vector<unsigned char> pub(static_cast<size_t>(pubLen));
    unsigned char* pp = priv.data();
    unsigned char* qq = pub.data();
    i2d_PrivateKey(key.get(), &pp);
    i2d_PUBKEY(key.get(), &qq);

    KeyPair kp;
    kp.privateKey = toHex(priv.data(), priv.size());
    kp.publicKey = toHex(pub.data(), pub.size());
    return kp;
}

std::string Signer::sign(const std::string& privateKeyHex, const std::string& message) {
    PkeyPtr key = loadPrivateKey(privateKeyHex);
    if (!key) {
        ERR_clear_error();
        throw std::runtime_error("Signer::sign: malformed private key");
    }
    MdCtxPtr md(EVP_MD_CTX_new(), EVP_MD_CTX_free);
    if (!md || EVP_DigestSignInit(md.get(), nullptr, EVP_sha256(), nullptr, key.get()) != 1) {
        ERR_clear_error();
        throw std::runtime_error("Signer::sign: cannot initialise signing");
    }
    const unsigned char* msg = reinterpret_cast<const unsigned char*>(message.data());
    size_t sigLen = 0;
    if (EVP_DigestSign(md.get(), nullptr, &sigLen, msg, message.size()) != 1) {
        ERR_clear_error();
        throw std::runtime_error("Signer::sign: cannot size signature");
    }
    std::vector<unsigned char> sig(sigLen);
    if (EVP_DigestSign(md.get(), sig.data(), &sigLen, msg, message.size()) != 1) {
        ERR_clear_error();
        throw std::runtime_error("Signer::sign: signing failed");
    }
    return toHex(sig.data(), sigLen);
}

bool Signer::verify(const std::string& publicKeyHex, const std::string& message,
                    const std::string& signatureHex) {
    PkeyPtr key = loadPublicKey(publicKeyHex);
    std::vector<unsigned char> sig;
    if (!key || !fromHex(signatureHex, sig)) {
        ERR_clear_error();
        return false;
    }
    MdCtxPtr md(EVP_MD_CTX_new(), EVP_MD_CTX_free);
    if (!md || EVP_DigestVerifyInit(md.get(), nullptr, EVP_sha256(), nullptr, key.get()) != 1) {
        ERR_clear_error();
        return false;
    }
    const unsigned char* msg = reinterpret_cast<const unsigned char*>(message.data());
    int rc = EVP_DigestVerify(md.get(), sig.data(), sig.size(), msg, message.size());
    ERR_clear_error();
    return rc == 1;
}

std::string Signer::addressFromPublicKey(const std::string& publicKeyHex) {
    if (publicKeyHex.empty()) return "";
    return Hasher::sha256(publicKeyHex).substr(0, 40);
}
