#include "crypto.hpp"

#include <cryptopp/aes.h>
#include <cryptopp/gcm.h>
#include <cryptopp/integer.h>
#include <cryptopp/osrng.h>
#include <cryptopp/sha.h>

#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

#include "errors.hpp"

namespace stego {
namespace {

constexpr char kPrimeHex[] =
    "FFFFFFFFFFFFFFFFC90FDAA22168C234C4C6628B80DC1CD129024E088A67CC74"
    "020BBEA63B139B22514A08798E3404DDEF9519B3CD3A431B302B0A6DF25F1437"
    "4FE1356D6D51C245E485B576625E7EC6F44C42E9A637ED6B0BFF5CB6F406B7ED"
    "EE386BFB5A899FA5AE9F24117C4B1FE649286651ECE45B3DC2007CB8A163BF05"
    "98DA48361C55D39A69163FA8FD24CF5F83655D23DCA3AD961C62F356208552BB"
    "9ED529077096966D670C354E4ABC9804F1746C08CA18217C32905E462E36CE3B"
    "E39E772C180E86039B2783A2EC07A28FB5C55DF06F4C52C9DE2BCBF695581718"
    "3995497CEA956AE515D2261898FA051015728E5A8AACAA68FFFFFFFFFFFFFFFF";

constexpr std::size_t kAesKeySize = 32;
constexpr std::size_t kIvSize = 12;
constexpr std::size_t kTagSize = 16;

std::string to_hex(const CryptoPP::SecByteBlock& bytes) {
    std::ostringstream out;
    out << std::hex << std::setfill('0');
    for (std::size_t i = 0; i < bytes.size(); ++i) {
        out << std::setw(2) << static_cast<int>(bytes[i]);
    }
    return out.str();
}

std::vector<std::uint8_t> from_hex(const std::string& hex) {
    if (hex.size() % 2 != 0) {
        throw CryptoError("Нечётная длина hex-строки");
    }
    std::vector<std::uint8_t> bytes;
    for (std::size_t i = 0; i < hex.size(); i += 2) {
        std::string pair = hex.substr(i, 2);
        try {
            int value = std::stoi(pair, nullptr, 16);
            bytes.push_back(static_cast<std::uint8_t>(value));
        } catch (const std::exception&) {
            throw CryptoError("Недопустимый символ в hex-строке");
        }
    }
    return bytes;
}

}  // namespace

DiffieHellman::DiffieHellman() {
    try {
        CryptoPP::Integer p(("0x" + std::string(kPrimeHex)).c_str());
        CryptoPP::Integer g(2L);
        dh_.AccessGroupParameters().Initialize(p, g);

        CryptoPP::AutoSeededRandomPool rng;
        private_ = CryptoPP::SecByteBlock(dh_.PrivateKeyLength());
        public_ = CryptoPP::SecByteBlock(dh_.PublicKeyLength());
        dh_.GenerateKeyPair(rng, private_, public_);
    } catch (const CryptoPP::Exception& e) {
        throw CryptoError(std::string("Не удалось создать ключи DH: ") + e.what());
    }
}

std::string DiffieHellman::public_key_hex() const { return to_hex(public_); }

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
    out << to_hex(private_);
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
        std::vector<std::uint8_t> iv(kIvSize);
        rng.GenerateBlock(iv.data(), iv.size());

        CryptoPP::GCM<CryptoPP::AES>::Encryption enc;
        enc.SetKeyWithIV(key, key.size(), iv.data(), iv.size());

        std::vector<std::uint8_t> ciphertext(plain.size());
        std::vector<std::uint8_t> tag(kTagSize);
        enc.EncryptAndAuthenticate(ciphertext.data(), tag.data(), kTagSize, iv.data(),
                                   static_cast<int>(iv.size()), nullptr, 0, plain.data(),
                                   plain.size());

        std::vector<std::uint8_t> result;
        result.insert(result.end(), iv.begin(), iv.end());
        result.insert(result.end(), ciphertext.begin(), ciphertext.end());
        result.insert(result.end(), tag.begin(), tag.end());
        return result;
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
        std::size_t body_size = cipher.size() - kIvSize - kTagSize;
        std::vector<std::uint8_t> iv(cipher.begin(), cipher.begin() + kIvSize);
        std::vector<std::uint8_t> body(cipher.begin() + kIvSize,
                                       cipher.begin() + kIvSize + body_size);
        std::vector<std::uint8_t> tag(cipher.end() - kTagSize, cipher.end());

        CryptoPP::GCM<CryptoPP::AES>::Decryption dec;
        dec.SetKeyWithIV(key, key.size(), iv.data(), iv.size());

        std::vector<std::uint8_t> result(body_size);
        bool ok = dec.DecryptAndVerify(result.data(), tag.data(), kTagSize, iv.data(),
                                       static_cast<int>(iv.size()), nullptr, 0, body.data(),
                                       body.size());
        if (!ok) {
            throw CryptoError("Проверка подлинности не пройдена (неверный ключ или данные)");
        }
        return result;
    } catch (const CryptoPP::Exception& e) {
        throw CryptoError(std::string("Ошибка расшифрования AES-GCM: ") + e.what());
    }
}

}  // namespace stego
