#include "crypto.hpp"

#include <cryptopp/aes.h>
#include <cryptopp/gcm.h>
#include <cryptopp/integer.h>
#include <cryptopp/osrng.h>
#include <cryptopp/sha.h>

#include <algorithm>
#include <fstream>

#include "errors.hpp"

namespace stego {
namespace {

// Простое число MODP-группы №14 (2048 бит) из RFC 3526; генератор g = 2.
constexpr char kPrimeHex[] =
    "FFFFFFFFFFFFFFFFC90FDAA22168C234C4C6628B80DC1CD129024E088A67CC74"
    "020BBEA63B139B22514A08798E3404DDEF9519B3CD3A431B302B0A6DF25F1437"
    "4FE1356D6D51C245E485B576625E7EC6F44C42E9A637ED6B0BFF5CB6F406B7ED"
    "EE386BFB5A899FA5AE9F24117C4B1FE649286651ECE45B3DC2007CB8A163BF05"
    "98DA48361C55D39A69163FA8FD24CF5F83655D23DCA3AD961C62F356208552BB"
    "9ED529077096966D670C354E4ABC9804F1746C08CA18217C32905E462E36CE3B"
    "E39E772C180E86039B2783A2EC07A28FB5C55DF06F4C52C9DE2BCBF695581718"
    "3995497CEA956AE515D2261898FA051015728E5A8AACAA68FFFFFFFFFFFFFFFF";

constexpr std::size_t kAesKeySize = 32;  // AES-256.
constexpr std::size_t kIvSize = 12;      // Рекомендованная длина IV для GCM.
constexpr std::size_t kTagSize = 16;     // Длина тега аутентификации GCM.

/// @brief Переводит массив байтов в строку hex-символов верхнего регистра.
std::string to_hex(const std::uint8_t* data, std::size_t size) {
    static constexpr char kDigits[] = "0123456789ABCDEF";
    std::string out;
    out.reserve(size * 2);
    for (std::size_t i = 0; i < size; ++i) {
        out.push_back(kDigits[data[i] >> 4]);
        out.push_back(kDigits[data[i] & 0x0F]);
    }
    return out;
}

/// @brief Переводит один hex-символ в числовое значение 0..15.
int hex_value(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    throw CryptoError("Недопустимый символ в hex-строке");
}

/// @brief Разбирает hex-строку чётной длины в массив байтов.
std::vector<std::uint8_t> from_hex(const std::string& hex) {
    if (hex.size() % 2 != 0) {
        throw CryptoError("Нечётная длина hex-строки");
    }
    std::vector<std::uint8_t> out;
    out.reserve(hex.size() / 2);
    for (std::size_t i = 0; i < hex.size(); i += 2) {
        out.push_back(static_cast<std::uint8_t>((hex_value(hex[i]) << 4) | hex_value(hex[i + 1])));
    }
    return out;
}

}  // namespace

DiffieHellman::DiffieHellman() {
    try {
        CryptoPP::Integer p(("0x" + std::string(kPrimeHex)).c_str());
        CryptoPP::Integer g(2L);
        dh_.AccessGroupParameters().Initialize(p, g);

        // borrowed: идиома генерации ключевой пары DH из Crypto++ wiki
        // (https://www.cryptopp.com/wiki/Diffie-Hellman).
        CryptoPP::AutoSeededRandomPool rng;
        private_ = CryptoPP::SecByteBlock(dh_.PrivateKeyLength());
        public_ = CryptoPP::SecByteBlock(dh_.PublicKeyLength());
        dh_.GenerateKeyPair(rng, private_, public_);
        // end borrowed
    } catch (const CryptoPP::Exception& e) {
        throw CryptoError(std::string("Не удалось создать ключи DH: ") + e.what());
    }
}

std::string DiffieHellman::public_key_hex() const {
    return to_hex(public_.BytePtr(), public_.size());
}

CryptoPP::SecByteBlock DiffieHellman::compute_shared(const std::string& peer_public_hex) const {
    std::vector<std::uint8_t> peer_bytes = from_hex(peer_public_hex);
    CryptoPP::SecByteBlock peer(peer_bytes.data(), peer_bytes.size());
    CryptoPP::SecByteBlock shared(dh_.AgreedValueLength());
    if (!dh_.Agree(shared, private_, peer)) {
        throw CryptoError("Согласование общего секрета DH не удалось");
    }
    return shared;
}

void DiffieHellman::save_private(const std::filesystem::path& path) const {
    std::ofstream out(path, std::ios::trunc);
    if (!out) {
        throw CryptoError("Не удалось открыть файл ключа для записи: " + path.string());
    }
    out << to_hex(private_.BytePtr(), private_.size());
    if (!out) {
        throw CryptoError("Ошибка записи файла ключа: " + path.string());
    }
}

void DiffieHellman::load_private(const std::filesystem::path& path) {
    std::ifstream in(path);
    if (!in) {
        throw CryptoError("Не удалось открыть файл ключа для чтения: " + path.string());
    }
    std::string hex;
    in >> hex;
    std::vector<std::uint8_t> bytes = from_hex(hex);
    private_ = CryptoPP::SecByteBlock(bytes.data(), bytes.size());

    // Публичный ключ восстанавливается как g^private mod p.
    CryptoPP::Integer x(private_.BytePtr(), private_.size());
    CryptoPP::Integer y = dh_.GetGroupParameters().ExponentiateBase(x);
    public_ = CryptoPP::SecByteBlock(dh_.PublicKeyLength());
    y.Encode(public_.BytePtr(), public_.size());
}

CryptoPP::SecByteBlock DiffieHellman::derive_aes_key(const CryptoPP::SecByteBlock& shared) {
    CryptoPP::SHA256 hash;
    CryptoPP::SecByteBlock key(kAesKeySize);
    hash.CalculateDigest(key, shared.BytePtr(), shared.size());
    return key;
}

std::vector<std::uint8_t> AesCipher::encrypt(const std::vector<std::uint8_t>& plain,
                                             const CryptoPP::SecByteBlock& key) const {
    if (key.size() != kAesKeySize) {
        throw CryptoError("Неверная длина ключа AES");
    }
    try {
        CryptoPP::AutoSeededRandomPool rng;
        CryptoPP::SecByteBlock iv(kIvSize);
        rng.GenerateBlock(iv, iv.size());

        CryptoPP::GCM<CryptoPP::AES>::Encryption enc;
        enc.SetKeyWithIV(key, key.size(), iv, iv.size());

        std::vector<std::uint8_t> out(kIvSize + plain.size() + kTagSize);
        std::copy(iv.BytePtr(), iv.BytePtr() + iv.size(), out.begin());
        enc.EncryptAndAuthenticate(out.data() + kIvSize, out.data() + kIvSize + plain.size(),
                                   kTagSize, iv, static_cast<int>(iv.size()), nullptr, 0,
                                   plain.data(), plain.size());
        return out;
    } catch (const CryptoPP::Exception& e) {
        throw CryptoError(std::string("Ошибка шифрования AES-GCM: ") + e.what());
    }
}

std::vector<std::uint8_t> AesCipher::decrypt(const std::vector<std::uint8_t>& cipher,
                                             const CryptoPP::SecByteBlock& key) const {
    if (key.size() != kAesKeySize) {
        throw CryptoError("Неверная длина ключа AES");
    }
    if (cipher.size() < kIvSize + kTagSize) {
        throw CryptoError("Шифртекст слишком короткий");
    }
    try {
        const std::size_t cipher_len = cipher.size() - kIvSize - kTagSize;
        const std::uint8_t* iv = cipher.data();
        const std::uint8_t* body = cipher.data() + kIvSize;
        const std::uint8_t* tag = cipher.data() + kIvSize + cipher_len;

        CryptoPP::GCM<CryptoPP::AES>::Decryption dec;
        dec.SetKeyWithIV(key, key.size(), iv, kIvSize);

        std::vector<std::uint8_t> out(cipher_len);
        const bool ok = dec.DecryptAndVerify(out.data(), tag, kTagSize, iv, kIvSize, nullptr, 0,
                                             body, cipher_len);
        if (!ok) {
            throw CryptoError("Проверка подлинности не пройдена (неверный ключ или данные)");
        }
        return out;
    } catch (const CryptoPP::Exception& e) {
        throw CryptoError(std::string("Ошибка расшифрования AES-GCM: ") + e.what());
    }
}

}  // namespace stego
