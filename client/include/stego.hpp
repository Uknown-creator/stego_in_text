#ifndef STEGO_CLIENT_STEGO_HPP
#define STEGO_CLIENT_STEGO_HPP

#include <cstdint>
#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

/// @file stego.hpp
/// @brief Стеганографический кодек: представление байтов шифртекста словами
///        русского языка и обратное преобразование.

namespace stego {

/// @brief Кодек «байт ↔ слово»: индекс слова в словаре равен значению байта.
class StegoCodec {
   public:
    /// @brief Загружает словарь из файла и строит прямое и обратное отображения.
    /// @param dictionary_path Путь к файлу словаря (UTF-8, слово на строку).
    /// @throws StegoCodecError если файл недоступен, содержит менее 256 слов
    ///         или в нём есть повторяющиеся слова.
    explicit StegoCodec(const std::filesystem::path& dictionary_path);

    /// @brief Кодирует байты в строку слов, разделённых пробелами.
    /// @param data Произвольная последовательность байтов (шифртекст).
    /// @return Строка из слов русского языка, представляющая данные.
    std::string encode(const std::vector<std::uint8_t>& data) const;

    /// @brief Декодирует строку слов обратно в исходные байты.
    /// @param text Строка слов, разделённых пробельными символами.
    /// @return Восстановленная последовательность байтов.
    /// @throws StegoCodecError если встречено слово, которого нет в словаре.
    std::vector<std::uint8_t> decode(const std::string& text) const;

   private:
    std::vector<std::string> words_;                       ///< Байт -> слово.
    std::unordered_map<std::string, std::uint8_t> index_;  ///< Слово -> байт.
};

}  // namespace stego

#endif  // STEGO_CLIENT_STEGO_HPP
