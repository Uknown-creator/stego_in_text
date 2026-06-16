#include <doctest/doctest.h>

#include <cstdio>
#include <fstream>
#include <string>
#include <vector>

#include "errors.hpp"
#include "stego.hpp"

using stego::StegoCodec;
using stego::StegoCodecError;

#ifndef STEGO_DICTIONARY_PATH
#define STEGO_DICTIONARY_PATH "data/dictionary.txt"
#endif

namespace {

// Создаёт временный файл словаря с указанными строками и возвращает его путь.
std::string make_dictionary(const std::string& name, std::size_t count, bool duplicate = false) {
    std::ofstream out(name, std::ios::trunc);
    for (std::size_t i = 0; i < count; ++i) {
        out << (duplicate ? "слово" : "слово" + std::to_string(i)) << "\n";
    }
    return name;
}

}  // namespace

TEST_CASE("Stego: кодирование и декодирование возвращают исходные байты") {
    StegoCodec codec(STEGO_DICTIONARY_PATH);
    std::vector<std::uint8_t> data = {0, 1, 2, 255, 128, 64, 7};

    std::string words = codec.encode(data);
    CHECK(words.find(' ') != std::string::npos);  // слова разделены пробелами
    CHECK(codec.decode(words) == data);
}

TEST_CASE("Stego: все 256 значений байта обратимы") {
    StegoCodec codec(STEGO_DICTIONARY_PATH);
    std::vector<std::uint8_t> all(256);
    for (int i = 0; i < 256; ++i) {
        all[i] = static_cast<std::uint8_t>(i);
    }
    CHECK(codec.decode(codec.encode(all)) == all);
}

TEST_CASE("Stego: пустые данные дают пустую строку и обратно") {
    StegoCodec codec(STEGO_DICTIONARY_PATH);
    CHECK(codec.encode({}).empty());
    CHECK(codec.decode("").empty());
}

TEST_CASE("Stego: неизвестное слово при декодировании бросает исключение") {
    StegoCodec codec(STEGO_DICTIONARY_PATH);
    CHECK_THROWS_AS(codec.decode("этогословаточнонетвсловаре"), StegoCodecError);
}

TEST_CASE("Stego: отсутствующий файл словаря бросает исключение") {
    CHECK_THROWS_AS(StegoCodec("нет-такого-словаря.txt"), StegoCodecError);
}

TEST_CASE("Stego: словарь меньше 256 слов отвергается") {
    const std::string path = make_dictionary("dict_small.txt", 100);
    CHECK_THROWS_AS(StegoCodec{path}, StegoCodecError);
    std::remove(path.c_str());
}

TEST_CASE("Stego: повторяющиеся слова в словаре отвергаются") {
    const std::string path = make_dictionary("dict_dup.txt", 300, /*duplicate=*/true);
    CHECK_THROWS_AS(StegoCodec{path}, StegoCodecError);
    std::remove(path.c_str());
}
