#ifndef STEGO_CLIENT_CRYPTO_HPP
#define STEGO_CLIENT_CRYPTO_HPP

#include <cryptopp/dh.h>
#include <cryptopp/secblock.h>

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

/// @file crypto.hpp
/// @brief Симметричное шифрование (AES-256-GCM) и согласование ключа по
///        протоколу Диффи-Хеллмана средствами библиотеки Crypto++.

namespace stego {

/// @brief Сторона обмена по Диффи-Хеллману (группа MODP из RFC 3526, 2048 бит).
class DiffieHellman {
   public:
    /// @brief Создаёт участника и генерирует новую пару ключей.
    /// @throws CryptoError при сбое генерации ключевой пары.
    DiffieHellman();

    /// @brief Возвращает публичный ключ участника в шестнадцатеричном виде.
    /// @return Публичный ключ как строка hex-символов в верхнем регистре.
    std::string public_key_hex() const;

    /// @brief Вычисляет общий секрет с публичным ключом другого участника.
    /// @param peer_public_hex Публичный ключ собеседника в hex-виде.
    /// @return Сырой общий секрет DH в виде защищённого блока байтов.
    /// @throws CryptoError если hex некорректен или согласование не удалось.
    CryptoPP::SecByteBlock compute_shared(const std::string& peer_public_hex) const;

    /// @brief Сохраняет приватный ключ участника в файл (hex, UTF-8).
    /// @param path Путь к файлу для записи приватного ключа.
    /// @throws CryptoError если файл недоступен для записи.
    void save_private(const std::filesystem::path& path) const;

    /// @brief Загружает приватный ключ из файла и восстанавливает публичный.
    /// @param path Путь к файлу с приватным ключом в hex-виде.
    /// @throws CryptoError если файл недоступен или содержит неверные данные.
    void load_private(const std::filesystem::path& path);

    /// @brief Выводит 32-байтный ключ AES из общего секрета DH через SHA-256.
    /// @param shared Общий секрет, полученный из compute_shared().
    /// @return Ключ длиной 32 байта для AES-256.
    static CryptoPP::SecByteBlock derive_aes_key(const CryptoPP::SecByteBlock& shared);

   private:
    CryptoPP::DH dh_;                  ///< Параметры группы Диффи-Хеллмана.
    CryptoPP::SecByteBlock private_;    ///< Приватный ключ участника.
    CryptoPP::SecByteBlock public_;     ///< Публичный ключ участника.
};

/// @brief Симметричный шифр AES-256-GCM. IV генерируется при каждом encrypt().
class AesCipher {
   public:
    /// @brief Шифрует данные ключом AES-256.
    /// @param plain Открытые данные произвольной длины.
    /// @param key Ключ длиной 32 байта (см. DiffieHellman::derive_aes_key).
    /// @return Байты вида [IV || шифртекст || тег аутентификации].
    /// @throws CryptoError при неверной длине ключа или сбое шифрования.
    std::vector<std::uint8_t> encrypt(const std::vector<std::uint8_t>& plain,
                                      const CryptoPP::SecByteBlock& key) const;

    /// @brief Расшифровывает данные, зашифрованные методом encrypt().
    /// @param cipher Байты вида [IV || шифртекст || тег].
    /// @param key Ключ длиной 32 байта, совпадающий с ключом шифрования.
    /// @return Восстановленные открытые данные.
    /// @throws CryptoError при неверном ключе, повреждении данных или теге.
    std::vector<std::uint8_t> decrypt(const std::vector<std::uint8_t>& cipher,
                                      const CryptoPP::SecByteBlock& key) const;
};

}  // namespace stego

#endif  // STEGO_CLIENT_CRYPTO_HPP
