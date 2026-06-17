#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
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
        if (token.rfind("--", 0) != 0) {
            throw stego::CliError("Ожидался параметр вида --имя, получено: " + token);
        }
        if (i + 1 >= argc) {
            throw stego::CliError("Отсутствует значение для параметра " + token);
        }
        args[token.substr(2)] = argv[++i];
    }
    return args;
}

const std::string& require(const std::map<std::string, std::string>& args,
                           const std::string& name) {
    auto it = args.find(name);
    if (it == args.end()) {
        throw stego::CliError("Не задан обязательный параметр --" + name);
    }
    return it->second;
}

std::vector<std::uint8_t> read_file(const fs::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        throw stego::CliError("Не удалось открыть файл для чтения: " + path.string());
    }
    return std::vector<std::uint8_t>(std::istreambuf_iterator<char>(in),
                                     std::istreambuf_iterator<char>());
}

void write_file(const fs::path& path, const std::vector<std::uint8_t>& data) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) {
        throw stego::CliError("Не удалось открыть файл для записи: " + path.string());
    }
    out.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
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

// Шифрует файл и публикует его на сервере в виде слов.
void run_send(const std::map<std::string, std::string>& args) {
    const stego::DiffieHellman dh = load_or_create_key(require(args, "keyfile"));
    const stego::StegoCodec codec(require(args, "dictionary"));
    const std::vector<std::uint8_t> plain = read_file(require(args, "plaintext"));

    stego::WsClient client;
    client.connect(require(args, "server"));
    client.register_key(require(args, "id"), dh.public_key_hex());

    const std::string peer_pub = client.request_public_key(require(args, "to"));
    const CryptoPP::SecByteBlock shared = dh.compute_shared(peer_pub);
    const CryptoPP::SecByteBlock key = stego::DiffieHellman::derive_aes_key(shared);

    const stego::AesCipher cipher;
    const std::vector<std::uint8_t> encrypted = cipher.encrypt(plain, key);
    const std::string words = codec.encode(encrypted);

    client.send_message(require(args, "id"), require(args, "to"), words);
    std::cout << "Сообщение отправлено получателю " << require(args, "to") << ".\n";
}

// Забирает сообщения с сервера, декодирует и расшифровывает их.
void run_receive(const std::map<std::string, std::string>& args) {
    const stego::DiffieHellman dh = load_or_create_key(require(args, "keyfile"));
    const stego::StegoCodec codec(require(args, "dictionary"));

    stego::WsClient client;
    client.connect(require(args, "server"));
    const std::string self = require(args, "id");
    client.register_key(self, dh.public_key_hex());

    const std::vector<stego::StoredMessage> messages = client.fetch(self);
    if (messages.empty()) {
        std::cout << "Новых сообщений нет.\n";
        return;
    }

    const stego::AesCipher cipher;
    for (const stego::StoredMessage& message : messages) {
        const std::string peer_pub = client.request_public_key(message.from);
        const CryptoPP::SecByteBlock shared = dh.compute_shared(peer_pub);
        const CryptoPP::SecByteBlock key = stego::DiffieHellman::derive_aes_key(shared);

        const std::vector<std::uint8_t> encrypted = codec.decode(message.words);
        const std::vector<std::uint8_t> plain = cipher.decrypt(encrypted, key);

        auto out = args.find("out");
        if (out != args.end()) {
            write_file(out->second, plain);
            std::cout << "Сообщение от " << message.from << " сохранено в " << out->second << ".\n";
        } else {
            std::cout << "От " << message.from << ": ";
            std::cout.write(reinterpret_cast<const char*>(plain.data()),
                            static_cast<std::streamsize>(plain.size()));
            std::cout << "\n";
        }
    }
}

}  // namespace

int main(int argc, char** argv) {
    try {
        const std::map<std::string, std::string> args = parse_args(argc, argv);
        const std::string& mode = require(args, "mode");
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
