// isview part 3 of 6: --dump, which writes dump_json.h's summary of NE modules and exits,
// and the messages of a command-line run, which go to standard error instead of a message
// box so that a script never waits on one. Included once, by main.cpp.

namespace {

// A standard handle to write to. A GUI program started from a console has none unless it is
// redirected; it then attaches to the parent's console. INVALID_HANDLE_VALUE when there is
// no console either. `close` says whether the caller owns the handle.
HANDLE std_handle(DWORD which, bool& close) {
    close = false;
    HANDLE h = GetStdHandle(which);
    if (h != nullptr && h != INVALID_HANDLE_VALUE) return h;
    // ERROR_ACCESS_DENIED: attached already (an earlier call for the other handle)
    if (!AttachConsole(ATTACH_PARENT_PROCESS) && GetLastError() != ERROR_ACCESS_DENIED)
        return INVALID_HANDLE_VALUE;
    h = CreateFileW(L"CONOUT$", GENERIC_WRITE, FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
    close = h != INVALID_HANDLE_VALUE;
    return h;
}

bool write_all(HANDLE h, const std::string& data) {
    bool ok = true;
    std::size_t done = 0;
    while (ok && done < data.size()) {
        DWORD wrote = 0;
        const DWORD chunk = static_cast<DWORD>(std::min<std::size_t>(data.size() - done, 1u << 20));
        ok = WriteFile(h, data.data() + done, chunk, &wrote, nullptr) && wrote > 0;
        done += wrote;
    }
    return ok;
}

// Writes `data` to the file `out`, or to standard output when `out` is empty.
bool write_output(const std::string& data, const std::wstring& out) {
    bool close = false;
    HANDLE h = INVALID_HANDLE_VALUE;
    if (!out.empty()) {
        h = CreateFileW(out.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL,
                        nullptr);
        close = true;
    } else {
        h = std_handle(STD_OUTPUT_HANDLE, close);
    }
    if (h == nullptr || h == INVALID_HANDLE_VALUE) return false;
    const bool ok = write_all(h, data);
    if (close) CloseHandle(h);
    return ok;
}

// "isview: MSG" on standard error, in UTF-8; a message box only when the process has no
// standard error and no console to attach to.
void write_error(const std::wstring& msg) {
    bool close = false;
    HANDLE h = std_handle(STD_ERROR_HANDLE, close);
    if (h == nullptr || h == INVALID_HANDLE_VALUE) {
        MessageBoxW(nullptr, msg.c_str(), L"isview", MB_ICONERROR);
        return;
    }
    write_all(h, utf8(L"isview: " + msg + L"\n"));
    if (close) CloseHandle(h);
}

// --dump MODULE: one module's summary, or with MODULE "*" an array of every module's.
int run_dump(const std::wstring& which, const std::wstring& out) {
    g.modules = isv::scan_modules(*g.store);
    std::string json;
    if (which == L"*") {
        json = isv::modules_json(g.modules);
    } else {
        const auto name = narrow(which);
        const ModuleInfo* m = name ? find_module(*name) : nullptr;
        if (!m) {
            std::wstring known;
            for (const ModuleInfo& mi : g.modules) known += L" " + widen(mi.name);
            write_error(L"no NE module " + which + L" in " + g.dir.wstring() + L"; modules:" + known);
            return 1;
        }
        json = isv::module_json(*m) + "\n";
    }
    if (!write_output(json, out)) {
        write_error(L"cannot write the summary" + (out.empty() ? L"" : L" to " + out));
        return 1;
    }
    return 0;
}

}  // namespace
