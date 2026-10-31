// Test harness only. Linker wrappers inject faults; production has no test hooks.
#include "pc/mod_integrations/mi_install.h"
#include "pc/mod_integrations/mi_updates.h"
#include "pc/utils/json.hpp"
#include <cassert>
#include <atomic>
#include <cerrno>
#include <chrono>
#include <cstdarg>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <sys/syscall.h>
#include <unistd.h>

using nlohmann::json;
namespace fs = std::filesystem;
static std::string fault;
static std::atomic<MiInstall*> operationForTest{nullptr};
extern "C" long __real_syscall(long, ...);
extern "C" int __real_rename(const char*, const char*);

extern "C" long __wrap_syscall(long number, ...) {
    // mi_install.cpp has a single syscall call site: atomic RENAME_NOREPLACE.
    assert(number == SYS_renameat2);
    va_list args;
    va_start(args, number);
    int fromFd = va_arg(args, int);
    const char* from = va_arg(args, const char*);
    int toFd = va_arg(args, int);
    const char* to = va_arg(args, const char*);
    int flags = va_arg(args, int);
    va_end(args);
    bool backup = fs::path(to).filename() == "backup";
    bool restoring = fs::path(from).filename() == "backup";
    bool rejecting = fs::path(to).filename() == "rejected";
    bool newDelivery = std::string(from).find("/update-") != std::string::npos &&
        std::string(to).find("/mods/") != std::string::npos && !restoring;
    if (fault == "crash-staged") _exit(73);
    if (backup && fault == "update-crash-before-backup") _exit(73);
    if (backup && fault == "update-cancel-commit") {
        while (!operationForTest.load()) std::this_thread::yield();
        mi_install_cancel(operationForTest.load());
    }
    if ((backup && fault == "update-backup-fail") ||
        (newDelivery && fault == "update-delivery-fail") ||
        (rejecting && fault == "update-rollback-fail") ||
        (restoring && fault == "update-restore-fail")) { errno = ENOSPC; return -1; }
    if (fault == "destination-race") {
        fs::create_directory(to);
        std::ofstream(fs::path(to) / "user.txt") << "preserve concurrent destination";
    }
    long result = __real_syscall(number, fromFd, from, toFd, to, flags);
    if (result == 0 && fault == "crash-delivered") _exit(73);
    if (result == 0 && backup && fault == "update-crash-backup") _exit(73);
    if (result == 0 && newDelivery && fault == "update-crash-delivered") _exit(73);
    if (result == 0 && backup && fault == "update-destination-race") {
        fs::path target = fs::path(from);
        fs::create_directory(target);
        std::ofstream(target / "user.txt") << "preserve concurrent destination";
    }
    return result;
}
extern "C" int __wrap_rename(const char* from, const char* to) {
    bool receipt = std::string(to).find("/mi/receipts/") != std::string::npos;
    bool journal = std::string(to).find("/mi/transactions/") != std::string::npos;
    std::string state;
    if (journal) { std::ifstream input(from); json value; input >> value; state = value.value("state", ""); }
    bool failReceipt = fault == "update-receipt-no-space" || fault == "update-rollback-fail" ||
        fault == "update-restore-fail" || fault == "update-crash-rolled-back" || fault == "update-rollback-journal-fail";
    if ((receipt && (fault == "receipt-no-space" || failReceipt)) || (journal && fault == "journal-no-space") ||
        (journal && state == "prepared" && fault == "update-journal-no-space") ||
        (journal && state == "committed" && fault == "update-commit-marker-fail") ||
        (journal && state == "rolled_back" && fault == "update-rollback-journal-fail")) {
        errno = ENOSPC;
        return -1;
    }
    int result = __real_rename(from, to);
    if (result == 0 && receipt && (fault == "crash-receipt" || fault == "update-crash-receipt")) _exit(73);
    if (result == 0 && journal && ((state == "committed" && fault == "update-crash-committed") ||
        (state == "cleaning" && fault == "update-crash-cleaning") ||
        (state == "rolled_back" && fault == "update-crash-rolled-back"))) _exit(73);
    return result;
}
static json load(const char* path) { std::ifstream input(path); json value; input >> value; return value; }
static void copy(char* target, size_t capacity, const std::string& value) {
    assert(value.size() < capacity);
    std::memcpy(target, value.c_str(), value.size() + 1);
}
static MiCatalogFile source(const char* path) {
    auto value = load(path);
    MiCatalogFile file{};
    copy(file.modId, sizeof(file.modId), value.at("mod_id"));
    copy(file.versionId, sizeof(file.versionId), value.at("version_id"));
    copy(file.fileId, sizeof(file.fileId), value.at("file_id"));
    copy(file.version, sizeof(file.version), value.at("version"));
    copy(file.filename, sizeof(file.filename), value.at("filename"));
    copy(file.url, sizeof(file.url), value.at("url"));
    copy(file.sha256, sizeof(file.sha256), value.at("sha256"));
    file.size = value.at("size");
    file.supported = true;
    return file;
}
static bool terminal(MiInstallState state) {
    return state == MI_INSTALL_COMPLETE || state == MI_INSTALL_FAILED || state == MI_INSTALL_CANCELLED;
}
static MiInstallInfo waitInstall(MiInstall* op) {
    MiInstallInfo result{};
    for (int i = 0; i < 10000; i++) {
        assert(mi_install_poll(op, &result));
        if (fault == "update-cancel" && result.state == MI_INSTALL_DOWNLOADING) mi_install_cancel(op);
        if (terminal(result.state)) return result;
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    assert(false && "installer did not reach terminal state");
    return result;
}
static json scan(const char* root, const char* catalogFile) {
    auto text = load(catalogFile).dump();
    std::unique_ptr<MiCatalog, decltype(&mi_catalog_free)> catalog(
        mi_catalog_parse(text.data(), text.size()), mi_catalog_free);
    assert(mi_catalog_status(catalog.get()) == MI_CATALOG_READY);
    MiUpdates* op = mi_updates_start(root, catalog.get());
    assert(op);
    MiUpdatesInfo info{};
    for (int i = 0; i < 10000; i++) {
        assert(mi_updates_poll(op, &info));
        if (info.state != MI_UPDATES_RUNNING) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    assert(info.state != MI_UPDATES_RUNNING);
    json candidates = json::array();
    for (size_t i = 0; i < mi_catalog_count(catalog.get()); i++) {
        const auto* entry = mi_catalog_entry(catalog.get(), i);
        MiUpdateCandidate found{};
        if (mi_updates_get(op, entry->id, &found))
            candidates.push_back({{"mod_id", found.file.modId}, {"version_id", found.file.versionId},
                {"file_id", found.file.fileId}, {"destination", found.destination},
                {"installed_version", found.installedVersion}});
    }
    mi_updates_close(op);
    return {{"state", info.state}, {"error", info.error}, {"checked", info.checked},
        {"available", info.available}, {"uncertain", info.uncertain}, {"missing", info.missing},
        {"candidates", candidates}};
}
int main(int argc, char** argv) {
    assert(argc >= 4);
    std::string action = argv[1];
    if (action == "scan") { std::cout << scan(argv[2], argv[3]).dump() << '\n'; return 0; }
    MiCatalogFile file{};
    if (action != "recover") file = source(argv[3]);
    if (argc > 4) fault = argv[4];
    uint64_t before = mi_install_revision();
    MiInstall* op = nullptr;
    if (action == "recover") op = mi_install_recover_start(argv[2]);
    else if (action == "update" || action == "update-busy-close") {
        auto value = load(argv[3]).at("previous");
        MiInstallPrevious previous{};
        copy(previous.versionId, sizeof(previous.versionId), value.at("version_id"));
        copy(previous.fileId, sizeof(previous.fileId), value.at("file_id"));
        copy(previous.destination, sizeof(previous.destination), value.at("destination"));
        op = mi_install_update_start(argv[2], &file, &previous);
    } else op = mi_install_start(argv[2], &file);
    operationForTest = op;
    if (fault == "update-cancel-pending") mi_install_cancel(op);
    assert(op);
    if (action == "busy-close" || action == "update-busy-close") {
        assert(argc == 6);
        MiInstallInfo info{};
        for (int i = 0; i < 5000; i++) {
            assert(mi_install_poll(op, &info));
            if (info.state == MI_INSTALL_DOWNLOADING) break;
            assert(!terminal(info.state));
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
        assert(info.state == MI_INSTALL_DOWNLOADING);
        assert(mi_install_is_busy() && mi_install_revision() > before);
        auto concurrent = scan(argv[2], argv[5]);
        assert(concurrent.at("state") == MI_UPDATES_FAILED && concurrent.at("error") == MI_UPDATES_BUSY);
        auto start = std::chrono::steady_clock::now();
        mi_install_close(op); // close must cancel without waiting for the HTTP server
        assert(std::chrono::steady_clock::now() - start < std::chrono::milliseconds(500));
        for (int i = 0; i < 10000 && mi_install_is_busy(); i++)
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        assert(!mi_install_is_busy() && mi_install_revision() >= before + 2);
        std::cout << json{{"closed", true}, {"revision_delta", mi_install_revision() - before},
            {"scan", concurrent}}.dump() << '\n';
        return 0;
    }
    auto info = waitInstall(op);
    assert(!mi_install_is_busy()); // terminal feedback must not leave the next action blocked
    assert(mi_install_revision() >= before + 2);
    mi_install_close(op);
    std::cout << json{{"state", info.state}, {"error", info.error}, {"destination", info.destination},
        {"revision_delta", mi_install_revision() - before}}.dump() << '\n';
}
