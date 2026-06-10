// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Charles Kennedy <https://github.com/UnsignedChad>
#include "KiCadIpc.h"

#include <cstdlib>
#include <filesystem>
#include <format>
#include <utility>

#include <nng/protocol/reqrep0/req.h>

#include "common/commands/base_commands.pb.h"
#include "common/commands/editor_commands.pb.h"
#include "common/envelope.pb.h"
#include "common/types/base_types.pb.h"

namespace sikit_kicad {

namespace {

constexpr const char* kDefaultSocket = "ipc:///tmp/kicad/api.sock";
constexpr int kTimeoutMs = 5000;

std::string env_or(const char* name, const char* fallback) {
    const char* v = std::getenv(name);
    return (v && *v) ? v : fallback;
}

}  // namespace

std::expected<KiCadIpc, std::string> KiCadIpc::connect(std::string address) {
    KiCadIpc c;
    c.address_ = !address.empty()
                     ? std::move(address)
                     : env_or("KICAD_API_SOCKET", kDefaultSocket);
    c.token_ = env_or("KICAD_API_TOKEN", "");
    c.client_name_ =
        std::format("com.unsignedchad.sikit-{}", ::getpid());

    if (int rv = nng_req0_open(&c.sock_); rv != 0) {
        return std::unexpected(
            std::format("nng_req0_open: {}", nng_strerror(rv)));
    }
    c.open_ = true;
    nng_socket_set_ms(c.sock_, NNG_OPT_RECVTIMEO, kTimeoutMs);
    nng_socket_set_ms(c.sock_, NNG_OPT_SENDTIMEO, kTimeoutMs);

    if (int rv = nng_dial(c.sock_, c.address_.c_str(), nullptr, 0); rv != 0) {
        return std::unexpected(std::format("dial {}: {}", c.address_,
                                           nng_strerror(rv)));
    }
    return c;
}

KiCadIpc::KiCadIpc(KiCadIpc&& other) noexcept
    : sock_(other.sock_),
      open_(std::exchange(other.open_, false)),
      address_(std::move(other.address_)),
      token_(std::move(other.token_)),
      client_name_(std::move(other.client_name_)) {}

KiCadIpc& KiCadIpc::operator=(KiCadIpc&& other) noexcept {
    if (this != &other) {
        if (open_) nng_close(sock_);
        sock_ = other.sock_;
        open_ = std::exchange(other.open_, false);
        address_ = std::move(other.address_);
        token_ = std::move(other.token_);
        client_name_ = std::move(other.client_name_);
    }
    return *this;
}

KiCadIpc::~KiCadIpc() {
    if (open_) nng_close(sock_);
}

std::expected<kiapi::common::ApiResponse, std::string> KiCadIpc::call(
    const google::protobuf::Message& cmd) {
    kiapi::common::ApiRequest req;
    req.mutable_header()->set_kicad_token(token_);
    req.mutable_header()->set_client_name(client_name_);
    if (!req.mutable_message()->PackFrom(cmd)) {
        return std::unexpected("failed to pack command");
    }

    std::string wire;
    if (!req.SerializeToString(&wire)) {
        return std::unexpected("failed to serialize request");
    }
    if (int rv = nng_send(sock_, wire.data(), wire.size(), 0); rv != 0) {
        return std::unexpected(std::format("send: {}", nng_strerror(rv)));
    }

    void* buf = nullptr;
    size_t len = 0;
    if (int rv = nng_recv(sock_, &buf, &len, NNG_FLAG_ALLOC); rv != 0) {
        return std::unexpected(std::format("recv: {}", nng_strerror(rv)));
    }

    kiapi::common::ApiResponse resp;
    const bool parsed = resp.ParseFromArray(buf, static_cast<int>(len));
    nng_free(buf, len);
    if (!parsed) return std::unexpected("unparseable response envelope");

    if (resp.status().status() != kiapi::common::AS_OK) {
        return std::unexpected(std::format(
            "kicad refused (status {}): {}",
            static_cast<int>(resp.status().status()),
            resp.status().error_message()));
    }
    return resp;
}

std::expected<void, std::string> KiCadIpc::ping() {
    kiapi::common::commands::Ping cmd;
    auto resp = call(cmd);
    if (!resp) return std::unexpected(resp.error());
    return {};
}

std::expected<KiCadVersion, std::string> KiCadIpc::version() {
    kiapi::common::commands::GetVersion cmd;
    auto resp = call(cmd);
    if (!resp) return std::unexpected(resp.error());

    kiapi::common::commands::GetVersionResponse out;
    if (!resp->message().UnpackTo(&out)) {
        return std::unexpected("unexpected reply type for GetVersion");
    }
    KiCadVersion v;
    v.major = out.version().major();
    v.minor = out.version().minor();
    v.patch = out.version().patch();
    v.full = out.version().full_version();
    return v;
}

std::expected<std::vector<std::string>, std::string>
KiCadIpc::open_pcb_paths() {
    kiapi::common::commands::GetOpenDocuments cmd;
    cmd.set_type(kiapi::common::types::DOCTYPE_PCB);
    auto resp = call(cmd);
    if (!resp) return std::unexpected(resp.error());

    kiapi::common::commands::GetOpenDocumentsResponse out;
    if (!resp->message().UnpackTo(&out)) {
        return std::unexpected("unexpected reply type for GetOpenDocuments");
    }

    std::vector<std::string> paths;
    for (const auto& doc : out.documents()) {
        if (doc.board_filename().empty()) continue;
        std::filesystem::path p = doc.board_filename();
        // kicad may hand back a bare filename; the project block carries
        // the directory in that case.
        if (p.is_relative() && !doc.project().path().empty()) {
            p = std::filesystem::path(doc.project().path()) / p;
        }
        paths.push_back(p.string());
    }
    return paths;
}

}  // namespace sikit_kicad
