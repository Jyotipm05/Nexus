#include <wavex/Base/Chainable.hpp>
#include <iostream>
#include <cassert>
#include <functional>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

// 1. Concrete Chainable handler types
class PrefixHandler : public wavex::Chainable {
    std::string prefix_;
public:
    explicit PrefixHandler(std::string prefix) : prefix_(std::move(prefix)) {}

    std::string_view name_impl() const { return "PrefixHandler"; }

    std::string handle_impl(const std::string& input) const {
        return prefix_ + input;
    }
};

class SuffixHandler : public wavex::Chainable {
    std::string suffix_;
public:
    explicit SuffixHandler(std::string suffix) : suffix_(std::move(suffix)) {}

    std::string_view name_impl() const { return "SuffixHandler"; }

    std::string handle_impl(const std::string& input) const {
        return input + suffix_;
    }
};

// 2. Mutable and Rvalue handler
class UppercaseHandler : public wavex::Chainable {
public:
    std::string_view name_impl() const { return "UppercaseHandler"; }

    std::string handle_impl(std::string&& input) {
        for (char& c : input) {
            c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        }
        return std::move(input);
    }
};

// 3. Static compile-time pipeline
template <typename... Handlers>
class StaticChain {
    std::tuple<Handlers...> handlers_;

    // Transparent unwrap: plain handlers pass straight through,
    // reference_wrapper<T> unwraps to T& so .handle() resolves on T.
    template <typename T>
    static T& unwrap(T& t) { return t; }

    template <typename T>
    static T& unwrap(std::reference_wrapper<T> rw) { return rw.get(); }

public:
    constexpr explicit StaticChain(Handlers... handlers)
        : handlers_(std::move(handlers)...) {}

    template <typename Context>
    bool process_all(Context& ctx) {
        return std::apply([&ctx, this](auto&... h) {
            return (unwrap(h).handle(ctx) && ...);
        }, handlers_);
    }
};

// Pipeline element for boolean chain
struct AuthGuard : public wavex::Chainable {
    bool allow = true;

    explicit AuthGuard(bool a = true) : allow(a) {}

    std::string_view name_impl() const { return "AuthGuard"; }
    bool handle_impl(const std::string& user) const {
        return allow && user == "admin";
    }
};

struct AuditLogger : public wavex::Chainable {
    mutable std::vector<std::string> audit_log;

    AuditLogger() = default;

    std::string_view name_impl() const { return "AuditLogger"; }
    bool handle_impl(const std::string& user) const {
        audit_log.push_back(user);
        return true;
    }
};

int main() {
    std::cout << "[Test] Running Chainable deducing this unit tests...\n";

    // Test 1: Basic static dispatch and name()
    PrefixHandler prefix(">>> ");
    SuffixHandler suffix(" <<<");

    assert(prefix.name() == "PrefixHandler");
    assert(suffix.name() == "SuffixHandler");

    std::string res1 = prefix.handle(std::string("Hello"));
    assert(res1 == ">>> Hello");

    std::string res2 = suffix.handle(res1);
    assert(res2 == ">>> Hello <<<");

    // Test 2: Rvalue perfect forwarding dispatch
    UppercaseHandler upper;
    assert(upper.name() == "UppercaseHandler");
    std::string res3 = upper.handle(std::string("wavex"));
    assert(res3 == "WAVEX");

    // Test 3: Static Concept Verification
    static_assert(wavex::ChainableHandler<PrefixHandler, const std::string&>);
    static_assert(wavex::ChainableHandler<SuffixHandler, const std::string&>);
    static_assert(wavex::ChainableHandler<UppercaseHandler, std::string&&>);

    // Test 4: Static Pipeline execution with fold expression
    // Use std::ref so the pipeline holds references to auth/audit rather than
    // copies — mutations (audit_log.push_back) are visible on the originals.
    AuthGuard auth{true};
    AuditLogger audit{};
    StaticChain pipeline{std::ref(auth), std::ref(audit)};

    std::string user_admin = "admin";
    bool ok_admin = pipeline.process_all(user_admin);
    assert(ok_admin == true);
    assert(audit.audit_log.size() == 1);
    assert(audit.audit_log[0] == "admin");

    std::string user_guest = "guest";
    bool ok_guest = pipeline.process_all(user_guest);
    assert(ok_guest == false);
    // Short-circuited by AuthGuard, AuditLogger must NOT have run for "guest"
    assert(audit.audit_log.size() == 1);

    std::cout << "[Test] Chainable deducing this tests PASSED successfully!\n";
    return 0;
}
