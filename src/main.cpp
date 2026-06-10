// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Charles Kennedy <https://github.com/UnsignedChad>
//
// sikit-kicad: kicad ipc plugin shell around the sikit GUI.
//
// Modes:
//   sikit-kicad                      launched by kicad (env vars set):
//                                    fetch the open board, show sikit
//   sikit-kicad board.kicad_pcb      plain sikit on a file, no kicad
//   sikit-kicad --probe              headless handshake check, prints
//                                    version + open boards, no Qt
//   sikit-kicad --smoke-gui          GUI path but auto-quits; used by
//                                    scripts/itest.sh under offscreen Qt
#include <cstdio>
#include <string>

#include <QApplication>
#include <QTimer>

#include "KiCadIpc.h"
#include "MainWindow.h"

namespace {

int run_probe() {
    auto ipc = sikit_kicad::KiCadIpc::connect();
    if (!ipc) {
        std::fprintf(stderr, "connect failed: %s\n", ipc.error().c_str());
        return 1;
    }
    std::printf("socket: %s\n", ipc->address().c_str());

    if (auto p = ipc->ping(); !p) {
        std::fprintf(stderr, "ping failed: %s\n", p.error().c_str());
        return 1;
    }
    std::printf("ping: ok\n");

    auto v = ipc->version();
    if (!v) {
        std::fprintf(stderr, "version failed: %s\n", v.error().c_str());
        return 1;
    }
    std::printf("kicad: %u.%u.%u (%s)\n", v->major, v->minor, v->patch,
                v->full.c_str());

    auto boards = ipc->open_pcb_paths();
    if (!boards) {
        std::fprintf(stderr, "open documents failed: %s\n",
                     boards.error().c_str());
        return 1;
    }
    if (boards->empty()) {
        std::printf("open pcb: none\n");
        return 2;
    }
    for (const auto& b : *boards) std::printf("open pcb: %s\n", b.c_str());
    return 0;
}

// Board path for the GUI: positional arg wins, otherwise ask the
// running kicad over ipc. Empty string = open sikit with no board.
std::string resolve_board(const std::string& arg, bool* from_kicad) {
    *from_kicad = false;
    if (!arg.empty()) return arg;

    auto ipc = sikit_kicad::KiCadIpc::connect();
    if (!ipc) return {};
    auto boards = ipc->open_pcb_paths();
    if (!boards || boards->empty()) return {};
    *from_kicad = true;
    return boards->front();
}

}  // namespace

int main(int argc, char** argv) {
    bool probe = false;
    bool smoke = false;
    std::string board_arg;
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        if (a == "--probe") probe = true;
        else if (a == "--smoke-gui") smoke = true;
        else if (!a.empty() && a[0] != '-') board_arg = a;
    }

    if (probe) return run_probe();

    QApplication app(argc, argv);
    QApplication::setApplicationName("sikit-kicad");

    bool from_kicad = false;
    const std::string board = resolve_board(board_arg, &from_kicad);

    MainWindow w;
    bool loaded = false;
    if (!board.empty()) {
        loaded = w.loadKicadPcb(QString::fromStdString(board));
        if (loaded && from_kicad) {
            w.setWindowTitle(w.windowTitle() + " [from KiCad]");
        }
    }
    w.show();

    if (smoke) {
        // Long enough for deferred init to run, short enough for CI.
        QTimer::singleShot(1500, &app, &QCoreApplication::quit);
        const int rc = app.exec();
        if (rc != 0) return rc;
        return loaded ? 0 : 3;
    }
    return app.exec();
}
