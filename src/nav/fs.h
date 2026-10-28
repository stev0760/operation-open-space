// The filesystem the Navigation walk reads (docs/re/nav.md "Filesystem calls", ADR 0012). The
// original enumerates with the DOS find calls of seg5:0x364 (int 21h AH = 4Eh, find-first) and
// seg5:0x352 (AH = 4Fh, find-next); Fs gives the same view: 8.3 names, the DOS attribute byte,
// the DOS attribute filter and the volume's enumeration order [BYTES seg5:0x364 seg6:0x3931].
//   - Fs: the interface; Win32Fs (fs_win32.cpp, Win32 only) and MemFs (in-memory, for tests).
//   - Walker: every find the Navigation code makes goes through it; it applies ADR 0012's bounds
//     (the 100-byte path, reparse points never entered, 4096 entries per search), records each
//     call and logs it in the form of nav.md's "Filesystem calls" table, so a run compares with
//     the oracle's int 21h trace.
//   - the probes seg6:0x4190, seg6:0x4748, seg6:0x4650 and seg6:0x45be.
// Portable: no Win32 calls in this header or fs.cpp.
#pragma once

#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace is::nav {

// ---- DOS attributes [REF MS-DOS int 21h AH=4Eh attribute semantics]
constexpr unsigned kAttrReadOnly = 0x01, kAttrHidden = 0x02, kAttrSystem = 0x04,
                   kAttrVolume = 0x08, kAttrDirectory = 0x10, kAttrArchive = 0x20;
// The tree's search attribute: directory, system and read-only, so hidden entries are left
// out [BYTES seg6:0x3931].
constexpr unsigned kTreeAttr = 0x15;
// The file searches (seg6:0x4190, seg6:0x420a) use attribute 0 [BYTES seg6:0x41bb seg6:0x4263].
constexpr unsigned kFileAttr = 0x00;

// A DOS search attribute admits normal files always and hidden, system and directory entries
// only when it names them [REF MS-DOS int 21h AH=4Eh attribute semantics].
bool dos_attr_match(unsigned entry_attr, unsigned search_attr);
// DOS wildcard matching of an 8.3 name against an 8.3 mask ("*.*", "*.EXE", "?"), case-
// insensitive, the name and extension matched separately as the find-first does.
bool dos_name_match(const std::string& name, const std::string& mask);
// The 8.3 form of a long name when the volume has no alias: upper case, spaces and dots
// dropped from the base (before the last '.') and the extension, + , ; = [ ] made '_', the
// base cut to 8 bytes and the extension to 3; "." and ".." stay as they are [DESIGN ADR 0012
// decision 4].
std::string short_name(const std::string& long_name);
// ASCII upper case (bytes from 0x80 unchanged).
std::string upper(std::string s);

// The unique 8.3 names of one directory whose volume gives no aliases (M3 s1 fixer) [DESIGN ADR
// 0012 decision 4] [OPEN: ADR 0012 line owed]. A name that fits 8.3 keeps it and is reserved;
// a long name that does not fit gets its cut (short_name) with a Windows-style tail, BASE~N.EXT,
// the base cut to leave room for "~N", N from 1 the first not yet used in the directory. The
// same long name (key) always gets the same name, so "Program Files" and "Program Files (x86)"
// become PROGRA~1 and PROGRA~2 and both stay reachable. [REF Win32 SDK "Naming Files", 8.3
// aliases]
class ShortNames {
public:
    void reserve(const std::string& fitting_name);
    // The name of the long name `key` (its exact text), whose cut is `cut`.
    std::string assign(const std::string& key, const std::string& cut);

private:
    std::map<std::string, std::string> given_;
    std::vector<std::string> used_;
};

// One find result: the find buffer's name (+0x1e) and attribute byte (+0x15).
struct FindEntry {
    std::string name;     // 8.3, as the DOS find buffer holds it
    unsigned attr = 0;    // the DOS attribute byte
    bool reparse = false; // FILE_ATTRIBUTE_REPARSE_POINT [DESIGN ADR 0012 decision 3]
};

// A tree entry: attribute bit 0x10 and a name not starting with '.' [BYTES seg6:0x3942
// seg6:0x394b seg6:0x479c].
bool is_subdir(const FindEntry& e);

// One search: the find-first, then find-next until it returns false.
class Search {
public:
    virtual ~Search() = default;
    virtual bool next(FindEntry& out) = 0;
};

enum class DriveType { kNoRoot, kUnknown, kRemovable, kFixed, kRemote, kCdrom, kRamDisk };
struct Drive {
    char letter = 'C';
    DriveType type = DriveType::kFixed;
};

class Fs {
public:
    virtual ~Fs() = default;
    // find-first over `pattern` ("D:\GAMES\*.exe") with the DOS attribute filter; the returned
    // search delivers its entries in the volume's order. The first next() is the find-first.
    virtual std::unique_ptr<Search> find(const std::string& pattern, unsigned attr) = 0;
    // The volume label (find-first "X:\*.*" attribute 0x08 in the original's drive list,
    // shell.md item 4); false when the drive gives none.
    virtual bool volume_label(char letter, std::string& label) = 0;
    // The drives C: to Z: with their types (GetLogicalDrives, GetDriveType) [DESIGN ADR 0012
    // decision 2].
    virtual std::vector<Drive> drives() = 0;
};

// Whether the drive list probes a drive's label when it lists it: fixed, removable, CD-ROM and
// RAM drives are probed; network, unknown and rootless drives are listed with Unknown_HD and
// probed only when the player selects them [DESIGN ADR 0012 decision 2].
bool probe_label_on_listing(DriveType type);

// One drive of the New Game drive list (ISPACE.DLL seg10:0x2104): the letter and, when the
// drive was probed and gave one, its volume label (null: "Unknown_HD").
struct ListedDrive {
    char letter = 'C';
    bool probed = false;
    bool has_label = false;
    std::string label;  // the label as Fs::volume_label gives it
};
// The drive list under ADR 0012 decision 2 (M3 s1 fixer): every drive C: to Z: of fs.drives()
// but a rootless one (the original skips a drive whose _chdrive fails [BYTES ISPACE.DLL
// seg10:0x2110 seg10:0x2118]); only the drives probe_label_on_listing names are probed for their
// label (the volume-label find-first [BYTES ISPACE.DLL seg10:0x211f-0x2138]), the others are
// listed unprobed. `log` gets one line per drive: `drive fs ISPACE.DLL seg10:0x2104 findfirst
// "X:\*.*" 0x08 -> "LABEL"`, `-> end`, or `-> not probed (ADR 0012)`.
std::vector<ListedDrive> list_drives(Fs& fs, const std::function<void(const std::string&)>& log = {});

// ---- the in-memory filesystem for tests
class MemFs : public Fs {
public:
    void add_drive(char letter, DriveType type = DriveType::kFixed, const std::string& label = {});
    // Paths are DOS paths ("D:\GAMES"); parents are created as plain directories; entries keep
    // the order they are added in. A non-root directory enumerates "." and ".." first, as DOS
    // does.
    void add_dir(const std::string& path, unsigned attr = kAttrDirectory, bool reparse = false);
    void add_file(const std::string& path, unsigned attr = kAttrArchive);

    std::unique_ptr<Search> find(const std::string& pattern, unsigned attr) override;
    bool volume_label(char letter, std::string& label) override;
    std::vector<Drive> drives() override;

    struct Node {
        std::string name;
        unsigned attr = kAttrDirectory;
        bool reparse = false;
        std::vector<std::unique_ptr<Node>> children;
    };

private:
    Node* lookup(const std::string& dir, bool create);
    struct DriveInfo {
        DriveType type = DriveType::kFixed;
        std::string label;
        Node root;
    };
    std::map<char, DriveInfo> drives_;
};

#ifdef _WIN32
// The Win32 filesystem (src/nav/fs_win32.cpp): FindFirstFileW/FindNextFileW where the original
// calls find-first/find-next, with the same pattern and the DOS attribute filter applied to
// the entry's attributes; the 8.3 alias (cAlternateFileName) when the volume gives one, else
// short_name() of the long name; FILE_ATTRIBUTE_REPARSE_POINT reported in FindEntry::reparse;
// critical-error boxes off (SEM_FAILCRITICALERRORS) [DESIGN ADR 0012 decisions 1, 2 and 4].
// It never opens, loads or runs a file.
class Win32Fs : public Fs {
public:
    Win32Fs();
    ~Win32Fs() override;
    std::unique_ptr<Search> find(const std::string& pattern, unsigned attr) override;
    bool volume_label(char letter, std::string& label) override;
    std::vector<Drive> drives() override;
    // The native path of a DOS path the walk built (cp1252): a component the port cut to 8.3
    // itself maps back to the long name it came from, so files on volumes without 8.3 aliases
    // stay reachable [DESIGN ADR 0012 decision 4]. Callers that read a listed file (the icon
    // reader) pass its path through this.
    std::wstring native(const std::string& dos_path) const;
    // Records a cut name: `dos_path` (upper case) stands for `native_path`.
    void remember(const std::string& dos_path, const std::wstring& native_path);
    // The unique 8.3 name of the long name `long_name` in the directory `dos_dir` (native
    // `native_dir`, ending in '\'): on the first cut name of a directory the port lists the
    // whole directory once (at most kMaxEntries entries, not one of the original's calls, so not
    // logged) to reserve its fitting names and number its long names in FindFirstFile order
    // (M3 s1 fixer) [DESIGN ADR 0012 decision 4] [OPEN].
    std::string alias(const std::string& dos_dir, const std::wstring& native_dir, const wchar_t* long_name);

private:
    std::map<std::string, std::wstring> long_names_;
    std::map<std::string, ShortNames> dirs_;
};
#endif

// ---- the walk's view of the filesystem

// The original's path buffer DGROUP:0x6990 is 100 bytes, followed by the record pointer
// DGROUP:0x69f4 [BYTES DGROUP:0x6990 DGROUP:0x69f4]: a directory fits when its path with the
// trailing '\' and the NUL fits in it [DESIGN ADR 0012 decision 3].
constexpr std::size_t kPathBuffer = 100;
bool path_fits(const std::string& dir);
// A search stops listing after this many entries [DESIGN ADR 0012 decision 3].
constexpr int kMaxEntries = 4096;

// The directory and mask of a pattern ("D:\GAMES\*.exe" -> "D:\GAMES\", "*.exe").
std::string pattern_dir(const std::string& pattern);
// dir + '\' when dir does not end in one (the lstrcat of DGROUP:0xeb8 and its kin).
std::string with_slash(const std::string& dir);

// One filesystem call of the walk, as nav.md's table names it.
struct FsCall {
    enum class Op { kFindFirst, kFindNext, kExtractIcon };
    std::string routine;   // "seg6:0x4748"
    Op op = Op::kFindFirst;
    std::string path;      // the find pattern or ExtractIcon's path; empty for find-next
    int arg = 0;           // the search attribute, or ExtractIcon's index (-1 or 0)
    bool ok = false;       // an entry was found (find), or the call ran (ExtractIcon)
    std::string name;      // the entry found
    int value = 0;         // ExtractIcon's result
    bool refused = false;  // not made: ADR 0012's bounds (path, reparse point, entry count)
    bool operator==(const FsCall& o) const;
};
// The log line: `seg6:0x4748 findfirst "D:\DEEP\*.*" 0x15 -> "."`, `seg6:0x4748 findnext ->
// end`, `seg6:0x4368 ExtractIcon "D:\GAMES\ALPHA.EXE" -1 -> 1`, `seg6:0x4368 ExtractIcon
// "D:\GAMES\ALPHA.EXE" 0 -> icon` (value 2 stands for a handle above 1, 0 for none); a call
// the bounds refuse ends in `-> refused (ADR 0012)`.
std::string describe(const FsCall& call);

class Walker {
public:
    using Log = std::function<void(const std::string&)>;
    explicit Walker(Fs& fs, Log log = {});

    // Every call is appended to `sink` while it is set (tests compare it with the oracle).
    void record(std::vector<FsCall>* sink) { sink_ = sink; }
    Fs& fs() { return fs_; }

    // One search of `routine`. The first next() is the find-first, later ones find-next.
    // A search is refused, without touching the filesystem, when its directory does not fit
    // kPathBuffer or lies under a reparse point the walk has seen; it ends after kMaxEntries.
    class Find {
    public:
        bool next(FindEntry& out);
        bool started() const { return started_; }

    private:
        friend class Walker;
        Find(Walker& w, std::string routine, std::string pattern, unsigned attr);
        Walker* w_;
        std::string routine_, pattern_;
        unsigned attr_;
        std::unique_ptr<Search> search_;
        bool started_ = false, refused_ = false, done_ = false;
        int delivered_ = 0;
    };
    Find find(const std::string& routine, const std::string& pattern, unsigned attr);

    // seg6:0x4368's ExtractIcon calls, recorded and logged by the caller's routine.
    void icon_call(const std::string& routine, const std::string& path, int index, int value,
                   bool refused = false);

    // Directories found with FILE_ATTRIBUTE_REPARSE_POINT: never entered or probed [DESIGN
    // ADR 0012 decision 3]. Find records them as it meets them.
    bool is_reparse(const std::string& dir) const;
    void note_reparse(const std::string& path);
    // Whether a directory can be entered: it fits the path buffer and is no reparse point.
    bool enterable(const std::string& dir) const;

    // ---- the probes (nav.md "Probes")
    // seg6:0x4190(dir): find-first `dir\*.exe` attribute 0 succeeds [BYTES seg6:0x4190
    // seg6:0x41bb seg6:0x41db seg6:0x41e4 seg6:0x41eb].
    bool has_exe(const std::string& dir, const char* routine = "seg6:0x4190");
    // seg6:0x4748(dir): `dir\*.*` attribute 0x15 holds a subdirectory whose name does not start
    // with '.'. A failed find-first still runs the find-next [BYTES seg6:0x4748 seg6:0x479c].
    bool has_subdir(const std::string& dir);
    // seg6:0x4650(dir): some such subdirectory `sub` has seg6:0x4190(dir\sub): the one-level-
    // ahead look [BYTES seg6:0x4650 seg6:0x46a1].
    bool subdir_has_exe(const std::string& dir);
    // seg6:0x45be(path): seg6:0x4190(path), or path differs from the drive root (lstrcmp
    // before the '\' is appended) and seg6:0x4748 and seg6:0x4650 both hold [BYTES seg6:0x45be].
    bool passes(const std::string& path, const std::string& root);

private:
    void push(const FsCall& c);
    Fs& fs_;
    Log log_;
    std::vector<FsCall>* sink_ = nullptr;
    std::vector<std::string> reparse_;  // upper-case paths with a trailing '\'
};

}  // namespace is::nav
