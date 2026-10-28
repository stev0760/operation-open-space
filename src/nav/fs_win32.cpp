// Win32Fs: the Navigation walk's filesystem through FindFirstFileW (ADR 0012). Win32 only.
#ifdef _WIN32
#include <windows.h>

#include "nav/fs.h"

namespace is::nav {

namespace {

constexpr UINT kCodePage = 1252;  // the port's strings are cp1252, as the original's

std::wstring widen(const std::string& s) {
    if (s.empty()) return {};
    const int n = MultiByteToWideChar(kCodePage, 0, s.data(), static_cast<int>(s.size()), nullptr, 0);
    std::wstring w(static_cast<std::size_t>(n), L'\0');
    MultiByteToWideChar(kCodePage, 0, s.data(), static_cast<int>(s.size()), w.data(), n);
    return w;
}

// cp1252; `lossy` when a character has no cp1252 form (it becomes '_').
std::string narrow(const wchar_t* w, bool* lossy) {
    const int len = static_cast<int>(wcslen(w));
    if (len == 0) return {};
    BOOL used = FALSE;
    const int n = WideCharToMultiByte(kCodePage, 0, w, len, nullptr, 0, "_", &used);
    std::string s(static_cast<std::size_t>(n), '\0');
    WideCharToMultiByte(kCodePage, 0, w, len, s.data(), n, "_", &used);
    if (lossy) *lossy = used != FALSE;
    return s;
}

// No "insert a disk" or "drive not ready" boxes while the walk touches a drive.
class QuietErrors {
public:
    QuietErrors() { SetThreadErrorMode(SEM_FAILCRITICALERRORS | SEM_NOOPENFILEERRORBOX, &old_); }
    ~QuietErrors() { SetThreadErrorMode(old_, nullptr); }
    QuietErrors(const QuietErrors&) = delete;
    QuietErrors& operator=(const QuietErrors&) = delete;

private:
    DWORD old_ = 0;
};

class Win32Search : public Search {
public:
    Win32Search(Win32Fs& fs, const std::string& pattern, unsigned attr)
        : fs_(fs), attr_(attr), dir_(pattern_dir(pattern)), mask_(pattern.substr(pattern_dir(pattern).size())) {
        native_dir_ = fs.native(dir_);
        if (!native_dir_.empty() && native_dir_.back() != L'\\' && native_dir_.back() != L':')
            native_dir_ += L'\\';
        QuietErrors quiet;
        h_ = FindFirstFileW((native_dir_ + widen(mask_)).c_str(), &d_);
        pending_ = h_ != INVALID_HANDLE_VALUE;
    }
    ~Win32Search() override {
        if (h_ != INVALID_HANDLE_VALUE) FindClose(h_);
    }
    Win32Search(const Win32Search&) = delete;
    Win32Search& operator=(const Win32Search&) = delete;

    bool next(FindEntry& out) override {
        if (h_ == INVALID_HANDLE_VALUE) return false;
        for (;;) {
            if (pending_) {
                pending_ = false;
            } else {
                QuietErrors quiet;
                if (!FindNextFileW(h_, &d_)) return false;
            }
            if (convert(out)) return true;
        }
    }

private:
    bool convert(FindEntry& out) {
        bool lossy = false;
        std::string name;
        if (d_.cAlternateFileName[0] != L'\0') {
            name = upper(narrow(d_.cAlternateFileName, nullptr));  // the volume's 8.3 alias
        } else {
            const std::string full = narrow(d_.cFileName, &lossy);
            name = short_name(full);  // [DESIGN ADR 0012 decision 4]
            if (lossy || name != upper(full)) {
                // a unique BASE~N name (M3 s1 fixer) [DESIGN ADR 0012 decision 4] [OPEN]
                name = fs_.alias(dir_, native_dir_, d_.cFileName);
                fs_.remember(upper(dir_ + name), native_dir_ + d_.cFileName);
            }
        }
        // the DOS attribute byte: read-only, hidden, system, directory, archive
        const unsigned attr = d_.dwFileAttributes & (kAttrReadOnly | kAttrHidden | kAttrSystem |
                                                     kAttrDirectory | kAttrArchive);
        if (!dos_attr_match(attr, attr_) || !dos_name_match(name, mask_)) return false;
        out.name = name;
        out.attr = attr;
        out.reparse = (d_.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0;
        return true;
    }

    Win32Fs& fs_;
    unsigned attr_;
    std::string dir_, mask_;
    std::wstring native_dir_;
    HANDLE h_ = INVALID_HANDLE_VALUE;
    WIN32_FIND_DATAW d_{};
    bool pending_ = false;
};

}  // namespace

Win32Fs::Win32Fs() = default;
Win32Fs::~Win32Fs() = default;

void Win32Fs::remember(const std::string& dos_path, const std::wstring& native_path) {
    long_names_[upper(dos_path)] = native_path;
}

std::wstring Win32Fs::native(const std::string& dos_path) const {
    // Longest remembered prefix that ends at a component boundary.
    const std::string up = upper(dos_path);
    std::size_t best = 0;
    std::wstring base;
    for (std::size_t i = 1; i <= up.size(); ++i) {
        if (i != up.size() && up[i] != '\\') continue;
        const auto it = long_names_.find(up.substr(0, i));
        if (it != long_names_.end()) {
            best = i;
            base = it->second;
        }
    }
    if (best == 0) return widen(dos_path);
    return base + widen(dos_path.substr(best));
}

namespace {
std::string utf8(const wchar_t* w) {
    const int len = static_cast<int>(wcslen(w));
    if (len == 0) return {};
    const int n = WideCharToMultiByte(CP_UTF8, 0, w, len, nullptr, 0, nullptr, nullptr);
    std::string s(static_cast<std::size_t>(n), '\0');
    WideCharToMultiByte(CP_UTF8, 0, w, len, s.data(), n, nullptr, nullptr);
    return s;
}
}  // namespace

std::string Win32Fs::alias(const std::string& dos_dir, const std::wstring& native_dir, const wchar_t* long_name) {
    const std::string key = upper(dos_dir);
    auto it = dirs_.find(key);
    if (it == dirs_.end()) {
        it = dirs_.emplace(key, ShortNames{}).first;
        ShortNames& t = it->second;
        std::vector<std::pair<std::string, std::string>> cut;  // (key, cut) in FindFirstFile order
        WIN32_FIND_DATAW d{};
        QuietErrors quiet;
        HANDLE h = FindFirstFileW((native_dir + L"*").c_str(), &d);
        if (h != INVALID_HANDLE_VALUE) {
            int n = 0;
            do {
                if (d.cAlternateFileName[0] != L'\0') {
                    t.reserve(narrow(d.cAlternateFileName, nullptr));
                    continue;
                }
                bool lossy = false;
                const std::string full = narrow(d.cFileName, &lossy);
                const std::string s = short_name(full);
                if (!lossy && s == upper(full)) t.reserve(s);
                else cut.emplace_back(utf8(d.cFileName), s);
            } while (++n < kMaxEntries && FindNextFileW(h, &d));
            FindClose(h);
        }
        for (const auto& c : cut) t.assign(c.first, c.second);
    }
    bool lossy = false;
    return it->second.assign(utf8(long_name), short_name(narrow(long_name, &lossy)));
}

std::unique_ptr<Search> Win32Fs::find(const std::string& pattern, unsigned attr) {
    return std::make_unique<Win32Search>(*this, pattern, attr);
}

bool Win32Fs::volume_label(char letter, std::string& label) {
    const wchar_t root[] = {static_cast<wchar_t>(letter), L':', L'\\', 0};
    wchar_t name[MAX_PATH + 1] = {};
    QuietErrors quiet;
    if (!GetVolumeInformationW(root, name, MAX_PATH + 1, nullptr, nullptr, nullptr, nullptr, 0))
        return false;
    label = narrow(name, nullptr);
    return !label.empty();
}

std::vector<Drive> Win32Fs::drives() {
    std::vector<Drive> v;
    const DWORD mask = GetLogicalDrives();
    for (char c = 'C'; c <= 'Z'; ++c) {
        if (!(mask & (1u << (c - 'A')))) continue;
        const wchar_t root[] = {static_cast<wchar_t>(c), L':', L'\\', 0};
        DriveType t = DriveType::kUnknown;
        switch (GetDriveTypeW(root)) {
        case DRIVE_NO_ROOT_DIR: t = DriveType::kNoRoot; break;
        case DRIVE_REMOVABLE: t = DriveType::kRemovable; break;
        case DRIVE_FIXED: t = DriveType::kFixed; break;
        case DRIVE_REMOTE: t = DriveType::kRemote; break;
        case DRIVE_CDROM: t = DriveType::kCdrom; break;
        case DRIVE_RAMDISK: t = DriveType::kRamDisk; break;
        default: t = DriveType::kUnknown; break;
        }
        v.push_back({c, t});
    }
    return v;
}

}  // namespace is::nav
#endif
