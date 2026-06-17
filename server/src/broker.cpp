#include "broker.hpp"

#include <algorithm>
#include <cctype>
#include <stdexcept>
#include <utility>

namespace stego {

using nlohmann::json;

std::string MessageBroker::handle(const std::string& request_text) {
    try {
        const json request = json::parse(request_text);
        if (!request.is_object()) {
            throw std::runtime_error("request must be a JSON object");
        }

        const std::string type = require_string(request, "type");
        json response;

        if (type == "register") {
            response = handle_register(request);
        } else if (type == "get_pubkey") {
            response = handle_get_pubkey(request);
        } else if (type == "send") {
            response = handle_send(request);
        } else if (type == "fetch") {
            response = handle_fetch(request);
        } else {
            throw std::runtime_error("unknown request type: " + type);
        }

        return response.dump();
    } catch (const std::exception& e) {
        return error_response(e.what()).dump();
    } catch (...) {
        return error_response("unknown server error").dump();
    }
}

json MessageBroker::handle_register(const json& request) {
    const std::string id = require_string(request, "id");
    const std::string pubkey = require_string(request, "pubkey");

    check_id(id, "id");
    check_pubkey(pubkey);

    std::lock_guard<std::mutex> lock(mutex_);
    public_keys_[id] = pubkey;
    return json{{"status", "ok"}};
}

json MessageBroker::handle_get_pubkey(const json& request) {
    const std::string id = require_string(request, "id");
    check_id(id, "id");

    std::lock_guard<std::mutex> lock(mutex_);
    const auto it = public_keys_.find(id);
    if (it == public_keys_.end()) {
        throw std::runtime_error("public key not found for id: " + id);
    }
    return json{{"pubkey", it->second}};
}

json MessageBroker::handle_send(const json& request) {
    const std::string from = require_string(request, "from");
    const std::string to = require_string(request, "to");
    const std::string words = require_string(request, "words");

    check_id(from, "from");
    check_id(to, "to");

    std::lock_guard<std::mutex> lock(mutex_);
    queues_[to].push_back(QueuedMessage{from, words});
    return json{{"status", "ok"}};
}

json MessageBroker::handle_fetch(const json& request) {
    const std::string id = require_string(request, "id");
    check_id(id, "id");

    std::vector<QueuedMessage> messages;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = queues_.find(id);
        if (it != queues_.end()) {
            messages = std::move(it->second);
            queues_.erase(it);
        }
    }

    json result = json::array();
    for (const QueuedMessage& message : messages) {
        result.push_back({{"from", message.from}, {"words", message.words}});
    }
    return json{{"messages", result}};
}

std::size_t MessageBroker::registered_count() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return public_keys_.size();
}

std::size_t MessageBroker::pending_count() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::size_t total = 0;
    for (const auto& item : queues_) {
        total += item.second.size();
    }
    return total;
}

std::string MessageBroker::require_string(const json& request, const std::string& field) {
    const auto it = request.find(field);
    if (it == request.end()) {
        throw std::runtime_error("missing field: " + field);
    }
    if (!it->is_string()) {
        throw std::runtime_error("field must be a string: " + field);
    }
    return it->get<std::string>();
}

void MessageBroker::check_id(const std::string& id, const std::string& field) {
    if (id.empty()) {
        throw std::runtime_error("empty " + field);
    }
}

void MessageBroker::check_pubkey(const std::string& pubkey) {
    if (pubkey.empty()) {
        throw std::runtime_error("empty pubkey");
    }
    if (!is_hex(pubkey)) {
        throw std::runtime_error("pubkey must be hex");
    }
}

bool MessageBroker::is_hex(const std::string& value) {
    return std::all_of(value.begin(), value.end(),
                       [](unsigned char ch) { return std::isxdigit(ch) != 0; });
}

json MessageBroker::error_response(const std::string& message) { return json{{"error", message}}; }

}  // namespace stego
