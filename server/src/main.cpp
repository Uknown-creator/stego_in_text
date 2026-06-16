#include <cstdint>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>

#include "ws_server.hpp"

namespace {

std::map<std::string, std::string> parse_args(int argc, char** argv) {
    std::map<std::string, std::string> args;
    for (int i = 1; i < argc; ++i) {
        const std::string name = argv[i];
        if (name.rfind("--", 0) != 0) {
            throw std::runtime_error("expected option, got: " + name);
        }
        if (i + 1 >= argc) {
            throw std::runtime_error("missing value for " + name);
        }
        args[name.substr(2)] = argv[++i];
    }
    return args;
}

void check_known_args(const std::map<std::string, std::string>& args) {
    for (const auto& item : args) {
        if (item.first != "host" && item.first != "port") {
            throw std::runtime_error("unknown option: --" + item.first);
        }
    }
}

}  // namespace

int main(int argc, char** argv) {
    try {
        const std::map<std::string, std::string> args = parse_args(argc, argv);
        check_known_args(args);

        std::string host = "0.0.0.0";
        std::uint16_t port = 8765;

        const auto host_it = args.find("host");
        if (host_it != args.end()) {
            host = host_it->second;
        }

        const auto port_it = args.find("port");
        if (port_it != args.end()) {
            port = stego::parse_port(port_it->second);
        }

        stego::WsServer server(host, port);
        server.run();
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    } catch (...) {
        std::cerr << "Unknown error\n";
        return 2;
    }
}
