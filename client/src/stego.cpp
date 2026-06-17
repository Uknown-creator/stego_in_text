#include "stego.hpp"

#include <fstream>
#include <sstream>

#include "errors.hpp"

namespace stego {
namespace {

constexpr std::size_t kAlphabetSize = 256;

}  // namespace

StegoCodec::StegoCodec(const std::filesystem::path& dictionary_path) {
    std::ifstream in(dictionary_path);
    if (!in) {
        throw StegoCodecError("Не удалось открыть словарь: " + dictionary_path.string());
    }

    std::string word;
    while (std::getline(in, word)) {
        // Срезаем возможный символ возврата каретки от файлов с CRLF.
        if (!word.empty() && word.back() == '\r') {
            word.pop_back();
        }
        if (word.empty()) {
            continue;
        }
        if (words_.size() >= kAlphabetSize) {
            break;
        }
        if (index_.find(word) != index_.end()) {
            throw StegoCodecError("Повторяющееся слово в словаре: " + word);
        }
        index_.emplace(word, static_cast<std::uint8_t>(words_.size()));
        words_.push_back(word);
    }

    if (words_.size() < kAlphabetSize) {
        throw StegoCodecError("В словаре меньше 256 уникальных слов");
    }
}

std::string StegoCodec::encode(const std::vector<std::uint8_t>& data) const {
    std::ostringstream out;
    for (std::size_t i = 0; i < data.size(); ++i) {
        if (i != 0) {
            out << ' ';
        }
        out << words_[data[i]];
    }
    return out.str();
}

std::vector<std::uint8_t> StegoCodec::decode(const std::string& text) const {
    std::istringstream in(text);
    std::vector<std::uint8_t> data;
    std::string word;
    while (in >> word) {
        auto it = index_.find(word);
        if (it == index_.end()) {
            throw StegoCodecError("Слово отсутствует в словаре: " + word);
        }
        data.push_back(it->second);
    }
    return data;
}

}  // namespace stego
