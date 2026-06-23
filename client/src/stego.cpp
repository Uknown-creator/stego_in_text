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
    while (in >> word) {
        if (words_.size() >= kAlphabetSize) {
            break;
        }
        if (index_.count(word) > 0) {
            throw StegoCodecError("Повторяющееся слово в словаре: " + word);
        }
        index_[word] = static_cast<std::uint8_t>(words_.size());
        words_.push_back(word);
    }

    if (words_.size() < kAlphabetSize) {
        throw StegoCodecError("В словаре меньше 256 уникальных слов");
    }
}

std::string StegoCodec::encode(const std::vector<std::uint8_t>& data) const {
    std::string result;
    for (std::size_t i = 0; i < data.size(); ++i) {
        if (i > 0) {
            result += " ";
        }
        result += words_[data[i]];
    }
    return result;
}

std::vector<std::uint8_t> StegoCodec::decode(const std::string& text) const {
    std::istringstream in(text);
    std::vector<std::uint8_t> data;
    std::string word;
    while (in >> word) {
        if (index_.count(word) == 0) {
            throw StegoCodecError("Слово отсутствует в словаре: " + word);
        }
        data.push_back(index_.at(word));
    }
    return data;
}

}  // namespace stego
