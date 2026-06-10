// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Charles Kennedy <https://github.com/UnsignedChad>
//
// Minimal client for the KiCad IPC API (kicad 9+): nng req/rep socket
// carrying protobuf ApiRequest/ApiResponse envelopes. Just enough
// surface for the plugin to find the board the user has open; live
// item pull and selection come later.
#pragma once

#include <expected>
#include <string>
#include <vector>

#include <nng/nng.h>

namespace google::protobuf { class Message; }
namespace kiapi::common { class ApiResponse; }

namespace sikit_kicad {

struct KiCadVersion {
    unsigned major = 0;
    unsigned minor = 0;
    unsigned patch = 0;
    std::string full;
};

class KiCadIpc {
public:
    // Resolution order for the socket address: explicit arg,
    // $KICAD_API_SOCKET (set by kicad when it launches a plugin),
    // then kicad's default path.
    static std::expected<KiCadIpc, std::string> connect(
        std::string address = {});

    KiCadIpc(KiCadIpc&& other) noexcept;
    KiCadIpc& operator=(KiCadIpc&& other) noexcept;
    KiCadIpc(const KiCadIpc&) = delete;
    KiCadIpc& operator=(const KiCadIpc&) = delete;
    ~KiCadIpc();

    std::expected<void, std::string> ping();
    std::expected<KiCadVersion, std::string> version();

    // Absolute paths of the .kicad_pcb documents open in kicad.
    std::expected<std::vector<std::string>, std::string> open_pcb_paths();

    const std::string& address() const { return address_; }

private:
    KiCadIpc() = default;

    // One request/response round trip. Packs cmd into the envelope,
    // sends, blocks for the reply (bounded by the socket timeout) and
    // returns the parsed envelope after checking the status code.
    std::expected<kiapi::common::ApiResponse, std::string> call(
        const google::protobuf::Message& cmd);

    nng_socket sock_{};
    bool open_ = false;
    std::string address_;
    std::string token_;
    std::string client_name_;
};

}  // namespace sikit_kicad
