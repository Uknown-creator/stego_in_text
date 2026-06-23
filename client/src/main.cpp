#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <string>
#include <vector>

#include "crypto.hpp"
#include "errors.hpp"
#include "net_client.hpp"
#include "stego.hpp"

namespace {

namespace fs = std::filesystem;

std::map<std::string, std::string> parse_args(int argc, char** argv) {
    std::map<std::string, std::string> args;
    for (int i = 1; i < argc; ++i) {
        std::string token = argv[i];
        if (token.substr(0, 2) != "--") {
            throw stego::CliError("Ожидался параметр вида --имя, получено: " + token);
        }
        if (i + 1 >= argc) {
            throw stego::CliError("Отсутствует значение для параметра " + token);
        }
        std::string name = token.substr(2);
        std::string value = argv[i + 1];
        args[name] = value;
        i = i + 1;
    }
    return args;
}

std::string require(const std::map<std::string, std::string>& args, const std::string& name) {
    if (args.count(name) == 0) {
        throw stego::CliError("Не задан обязательный параметр --" + name);
    }
    return args.at(name);
}

std::vector<std::uint8_t> read_file(const fs::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        throw stego::CliError("Не удалось открыть файл для чтения: " + path.string());
    }
    std::vector<std::uint8_t> data;
    char symbol;
    while (in.get(symbol)) {
        data.push_back(static_cast<std::uint8_t>(symbol));
    }
    return data;
}

void write_file(const fs::path& path, const std::vector<std::uint8_t>& data) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) {
        throw stego::CliError("Не удалось открыть файл для записи: " + path.string());
    }
    for (std::uint8_t byte : data) {
        out.put(static_cast<char>(byte));
    }
}

stego::DiffieHellman load_or_create_key(const fs::path& keyfile) {
    stego::DiffieHellman dh;
    if (fs::exists(keyfile)) {
        dh.load_private(keyfile);
    } else {
        dh.save_private(keyfile);
    }
    return dh;
}

void run_send(const std::map<std::string, std::string>& args) {
    stego::DiffieHellman dh = load_or_create_key(require(args, "keyfile"));
    stego::StegoCodec codec(require(args, "dictionary"));
    std::vector<std::uint8_t> plain = read_file(require(args, "plaintext"));

    stego::WsClient client;
    client.connect(require(args, "server"));
    client.register_key(require(args, "id"), dh.public_key_hex());

    std::string peer_pub = client.request_public_key(require(args, "to"));
    CryptoPP::SecByteBlock shared = dh.compute_shared(peer_pub);
    CryptoPP::SecByteBlock key = stego::DiffieHellman::derive_aes_key(shared);

    stego::AesCipher cipher;
    std::vector<std::uint8_t> encrypted = cipher.encrypt(plain, key);
    std::string words = codec.encode(encrypted);

    client.send_message(require(args, "id"), require(args, "to"), words);
    std::cout << "Сообщение отправлено получателю " << require(args, "to") << ".\n";
}

void run_receive(const std::map<std::string, std::string>& args) {
    stego::DiffieHellman dh = load_or_create_key(require(args, "keyfile"));
    stego::StegoCodec codec(require(args, "dictionary"));

    stego::WsClient client;
    client.connect(require(args, "server"));
    std::string self = require(args, "id");
    client.register_key(self, dh.public_key_hex());

    std::vector<stego::StoredMessage> messages = client.fetch(self);
    if (messages.empty()) {
        std::cout << "Новых сообщений нет.\n";
        return;
    }

    stego::AesCipher cipher;
    for (const stego::StoredMessage& message : messages) {
        std::string peer_pub = client.request_public_key(message.from);
        CryptoPP::SecByteBlock shared = dh.compute_shared(peer_pub);
        CryptoPP::SecByteBlock key = stego::DiffieHellman::derive_aes_key(shared);

        std::vector<std::uint8_t> encrypted = codec.decode(message.words);
        std::vector<std::uint8_t> plain = cipher.decrypt(encrypted, key);

        if (args.count("out") > 0) {
            std::string out_path = args.at("out");
            write_file(out_path, plain);
            std::cout << "Сообщение от " << message.from << " сохранено в " << out_path << ".\n";
        } else {
            std::cout << "От " << message.from << ": ";
            for (std::uint8_t byte : plain) {
                std::cout.put(static_cast<char>(byte));
            }
            std::cout << "\n";
        }
    }
}

}  // namespace

int main(int argc, char** argv) {
    try {
        std::map<std::string, std::string> args = parse_args(argc, argv);
        std::string mode = require(args, "mode");
        if (mode == "send") {
            run_send(args);
        } else if (mode == "receive") {
            run_receive(args);
        } else {
            throw stego::CliError("Неизвестный режим: " + mode + " (ожидается send или receive)");
        }
        return 0;
    } catch (const stego::StegoError& e) {
        std::cerr << "Ошибка: " << e.what() << "\n";
        return 1;
    } catch (const std::exception& e) {
        std::cerr << "Непредвиденная ошибка: " << e.what() << "\n";
        return 2;
    }
}
