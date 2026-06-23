#include <doctest/doctest.h>

#include <string>
#include <vector>

#include "crypto.hpp"
#include "errors.hpp"

using stego::AesCipher;
using stego::CryptoError;
using stego::DiffieHellman;

TEST_CASE("DH: два участника выводят одинаковый ключ AES") {
    DiffieHellman alice;
    DiffieHellman bob;

    CryptoPP::SecByteBlock shared_a = alice.compute_shared(bob.public_key_hex());
    CryptoPP::SecByteBlock shared_b = bob.compute_shared(alice.public_key_hex());

    CryptoPP::SecByteBlock key_a = DiffieHellman::derive_aes_key(shared_a);
    CryptoPP::SecByteBlock key_b = DiffieHellman::derive_aes_key(shared_b);

    REQUIRE(key_a.size() == 32);
    CHECK(key_a == key_b);
}

TEST_CASE("DH: некорректный публичный ключ вызывает исключение") {
    DiffieHellman alice;
    CHECK_THROWS_AS(alice.compute_shared("не-hex-строка"), CryptoError);
    CHECK_THROWS_AS(alice.compute_shared("ABC"), CryptoError);
}

TEST_CASE("DH: сохранение и загрузка ключа сохраняют публичный ключ") {
    std::string path = "test_alice.key";
    DiffieHellman original;
    original.save_private(path);

    DiffieHellman restored;
    restored.load_private(path);

    CHECK(original.public_key_hex() == restored.public_key_hex());
    std::remove(path.c_str());
}

TEST_CASE("DH: загрузка несуществующего файла ключа бросает исключение") {
    DiffieHellman dh;
    CHECK_THROWS_AS(dh.load_private("нет-такого-файла.key"), CryptoError);
}

TEST_CASE("AES: шифрование и расшифрование возвращают исходные данные") {
    DiffieHellman alice;
    DiffieHellman bob;
    CryptoPP::SecByteBlock key =
        DiffieHellman::derive_aes_key(alice.compute_shared(bob.public_key_hex()));

    AesCipher cipher;
    std::vector<std::uint8_t> plain = {0x00, 0x10, 0xFF, 0x42, 0x7A};
    std::vector<std::uint8_t> encrypted = cipher.encrypt(plain, key);

    CHECK(encrypted != plain);
    CHECK(cipher.decrypt(encrypted, key) == plain);
}

TEST_CASE("AES: пустое сообщение шифруется и расшифровывается") {
    DiffieHellman alice;
    DiffieHellman bob;
    CryptoPP::SecByteBlock key =
        DiffieHellman::derive_aes_key(alice.compute_shared(bob.public_key_hex()));
    AesCipher cipher;
    std::vector<std::uint8_t> empty;
    CHECK(cipher.decrypt(cipher.encrypt(empty, key), key).empty());
}

TEST_CASE("AES: повреждённый шифртекст не проходит проверку подлинности") {
    DiffieHellman alice;
    DiffieHellman bob;
    CryptoPP::SecByteBlock key =
        DiffieHellman::derive_aes_key(alice.compute_shared(bob.public_key_hex()));
    AesCipher cipher;
    std::vector<std::uint8_t> encrypted = cipher.encrypt({1, 2, 3, 4}, key);
    encrypted.back() ^= 0xFF;
    CHECK_THROWS_AS(cipher.decrypt(encrypted, key), CryptoError);
}

TEST_CASE("AES: слишком короткий шифртекст вызывает исключение") {
    DiffieHellman alice;
    DiffieHellman bob;
    CryptoPP::SecByteBlock key =
        DiffieHellman::derive_aes_key(alice.compute_shared(bob.public_key_hex()));
    AesCipher cipher;
    std::vector<std::uint8_t> tiny = {1, 2, 3};
    CHECK_THROWS_AS(cipher.decrypt(tiny, key), CryptoError);
}
