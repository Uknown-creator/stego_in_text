#ifndef STEGO_CLIENT_ERRORS_HPP
#define STEGO_CLIENT_ERRORS_HPP

#include <stdexcept>
#include <string>

/// @file errors.hpp
/// @brief Иерархия исключений приложения. Все ошибки между функциональными
///        блоками передаются через эти типы и обрабатываются в main.

namespace stego {

/// @brief Базовый класс для всех исключений приложения.
class StegoError : public std::runtime_error {
   public:
    /// @brief Создаёт исключение с текстовым описанием.
    /// @param message Человекочитаемое описание ошибки.
    explicit StegoError(const std::string& message) : std::runtime_error(message) {}
};

/// @brief Ошибки криптографических операций (DH-обмен, AES, хэширование).
class CryptoError : public StegoError {
   public:
    /// @brief Создаёт исключение криптографического слоя.
    /// @param message Описание криптографической ошибки.
    explicit CryptoError(const std::string& message) : StegoError(message) {}
};

/// @brief Ошибки кодирования/декодирования стеганографическим словарём.
class StegoCodecError : public StegoError {
   public:
    /// @brief Создаёт исключение слоя стеганографии.
    /// @param message Описание ошибки кодека.
    explicit StegoCodecError(const std::string& message) : StegoError(message) {}
};

/// @brief Ошибки сетевого взаимодействия с сервером по WebSocket.
class NetworkError : public StegoError {
   public:
    /// @brief Создаёт исключение сетевого слоя.
    /// @param message Описание сетевой ошибки.
    explicit NetworkError(const std::string& message) : StegoError(message) {}
};

/// @brief Ошибки разбора аргументов командной строки и ввода пользователя.
class CliError : public StegoError {
   public:
    /// @brief Создаёт исключение слоя командной строки.
    /// @param message Описание ошибки ввода.
    explicit CliError(const std::string& message) : StegoError(message) {}
};

}  // namespace stego

#endif  // STEGO_CLIENT_ERRORS_HPP
