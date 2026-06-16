#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <nlohmann/json.hpp>
#include <string>

#include "broker.hpp"

using nlohmann::json;
using stego::MessageBroker;

namespace {

json call(MessageBroker& broker, const json& request) {
    return json::parse(broker.handle(request.dump()));
}

bool has_error(const json& response) { return response.contains("error"); }

}  // namespace

TEST_CASE("register saves public key") {
    MessageBroker broker;

    const json response =
        call(broker, {{"type", "register"}, {"id", "alice"}, {"pubkey", "abcdef1234"}});

    CHECK(response.value("status", "") == "ok");
    CHECK(broker.registered_count() == 1);
}

TEST_CASE("register rejects empty id") {
    MessageBroker broker;

    const json response = call(broker, {{"type", "register"}, {"id", ""}, {"pubkey", "aa"}});

    CHECK(has_error(response));
    CHECK(broker.registered_count() == 0);
}

TEST_CASE("register rejects bad pubkey") {
    MessageBroker broker;

    const json response =
        call(broker, {{"type", "register"}, {"id", "alice"}, {"pubkey", "not-hex"}});

    CHECK(has_error(response));
    CHECK(broker.registered_count() == 0);
}

TEST_CASE("get_pubkey returns saved key") {
    MessageBroker broker;
    call(broker, {{"type", "register"}, {"id", "bob"}, {"pubkey", "0123456789abcdef"}});

    const json response = call(broker, {{"type", "get_pubkey"}, {"id", "bob"}});

    CHECK(response.value("pubkey", "") == "0123456789abcdef");
    CHECK_FALSE(has_error(response));
}

TEST_CASE("get_pubkey reports missing key") {
    MessageBroker broker;

    const json response = call(broker, {{"type", "get_pubkey"}, {"id", "nobody"}});

    CHECK(has_error(response));
    CHECK_FALSE(response.contains("pubkey"));
}

TEST_CASE("send stores message") {
    MessageBroker broker;

    const json response =
        call(broker, {{"type", "send"}, {"from", "alice"}, {"to", "bob"}, {"words", "раз два"}});

    CHECK(response.value("status", "") == "ok");
    CHECK(broker.pending_count() == 1);
}

TEST_CASE("send rejects missing words") {
    MessageBroker broker;

    const json response = call(broker, {{"type", "send"}, {"from", "alice"}, {"to", "bob"}});

    CHECK(has_error(response));
    CHECK(broker.pending_count() == 0);
}

TEST_CASE("send rejects empty receiver") {
    MessageBroker broker;

    const json response =
        call(broker, {{"type", "send"}, {"from", "alice"}, {"to", ""}, {"words", "hello"}});

    CHECK(has_error(response));
    CHECK(broker.pending_count() == 0);
}

TEST_CASE("fetch returns messages and clears queue") {
    MessageBroker broker;
    call(broker, {{"type", "send"}, {"from", "alice"}, {"to", "bob"}, {"words", "один два"}});
    call(broker, {{"type", "send"}, {"from", "carol"}, {"to", "bob"}, {"words", "три"}});

    const json response = call(broker, {{"type", "fetch"}, {"id", "bob"}});

    REQUIRE(response.contains("messages"));
    REQUIRE(response.at("messages").is_array());
    REQUIRE(response.at("messages").size() == 2);
    CHECK(response.at("messages").at(0).value("from", "") == "alice");
    CHECK(response.at("messages").at(0).value("words", "") == "один два");
    CHECK(response.at("messages").at(1).value("from", "") == "carol");
    CHECK(response.at("messages").at(1).value("words", "") == "три");
    CHECK(broker.pending_count() == 0);
}

TEST_CASE("fetch returns empty array after queue was cleared") {
    MessageBroker broker;
    call(broker, {{"type", "send"}, {"from", "alice"}, {"to", "bob"}, {"words", "text"}});
    call(broker, {{"type", "fetch"}, {"id", "bob"}});

    const json response = call(broker, {{"type", "fetch"}, {"id", "bob"}});

    REQUIRE(response.contains("messages"));
    CHECK(response.at("messages").is_array());
    CHECK(response.at("messages").empty());
}

TEST_CASE("fetch rejects empty id") {
    MessageBroker broker;

    const json response = call(broker, {{"type", "fetch"}, {"id", ""}});

    CHECK(has_error(response));
    CHECK_FALSE(response.contains("messages"));
}

TEST_CASE("bad json returns error") {
    MessageBroker broker;

    const json response = json::parse(broker.handle("{bad json"));

    CHECK(has_error(response));
}

TEST_CASE("unknown type returns error") {
    MessageBroker broker;

    const json response = call(broker, {{"type", "ping"}});

    CHECK(has_error(response));
}

TEST_CASE("request type must be a string") {
    MessageBroker broker;

    const json response = call(broker, {{"type", 5}});

    CHECK(has_error(response));
}
