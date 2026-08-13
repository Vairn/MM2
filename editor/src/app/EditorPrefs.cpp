#include "app/EditorPrefs.h"

#include <algorithm>
#include <fstream>
#include <filesystem>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace fs = std::filesystem;

namespace mm2 {
namespace {

std::string ExeDir() {
#ifdef _WIN32
    char buf[MAX_PATH];
    const DWORD n = GetModuleFileNameA(nullptr, buf, MAX_PATH);
    if (n == 0 || n >= MAX_PATH) return {};
    return fs::path(buf).parent_path().string();
#else
    return {};
#endif
}

std::string PrefsPath() {
    const std::string exeDir = ExeDir();
    if (!exeDir.empty()) return (fs::path(exeDir) / "mm2ed.ini").string();
    return "mm2ed.ini";
}

std::string NormalizeFolder(const std::string& folderPath) {
    std::error_code ec;
    fs::path p = fs::absolute(folderPath, ec);
    if (ec) p = fs::path(folderPath);
    return p.lexically_normal().string();
}

bool PathsEqual(const std::string& a, const std::string& b) {
    std::error_code ec;
    if (fs::equivalent(a, b, ec)) return true;
    return NormalizeFolder(a) == NormalizeFolder(b);
}

void TrimInPlace(std::string& s) {
    while (!s.empty() && (s.back() == '\r' || s.back() == ' ' || s.back() == '\t')) s.pop_back();
    size_t i = 0;
    while (i < s.size() && (s[i] == ' ' || s[i] == '\t')) ++i;
    if (i > 0) s.erase(0, i);
}

bool FileExistsCI(const fs::path& dir, const char* name) {
    std::error_code ec;
    if (fs::exists(dir / name, ec)) return true;
    // Case-insensitive scan for hosts with mixed-case installs.
    if (!fs::is_directory(dir, ec)) return false;
    std::string want = name;
    std::transform(want.begin(), want.end(), want.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    for (auto& e : fs::directory_iterator(dir, ec)) {
        if (!e.is_regular_file()) continue;
        std::string fn = e.path().filename().string();
        std::transform(fn.begin(), fn.end(), fn.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        if (fn == want) return true;
    }
    return false;
}

}  // namespace

bool IsMm2DataFolder(const std::string& folderPath) {
    std::error_code ec;
    if (!fs::is_directory(folderPath, ec)) return false;
    const fs::path dir(folderPath);
    // Any of the core Amiga data files is enough to count as a project.
    return FileExistsCI(dir, "items.dat") || FileExistsCI(dir, "map.dat") ||
           FileExistsCI(dir, "event.dat") || FileExistsCI(dir, "roster.dat");
}

std::string RecentFolderLabel(const std::string& folderPath) {
    fs::path p(folderPath);
    std::string leaf = p.filename().string();
    if (leaf.empty()) leaf = p.parent_path().filename().string();
    return leaf.empty() ? folderPath : leaf;
}

std::vector<std::string> LoadRecentFolders() {
    std::vector<std::string> out;
    std::ifstream in(PrefsPath());
    if (!in) return out;
    std::string line;
    bool inRecent = true;
    while (std::getline(in, line)) {
        TrimInPlace(line);
        if (line.empty() || line[0] == '#' || line[0] == ';') continue;
        if (line.front() == '[' && line.back() == ']') {
            inRecent = (line == "[recent]");
            continue;
        }
        if (!inRecent) continue;
        const std::string norm = NormalizeFolder(line);
        if (!IsMm2DataFolder(norm)) continue;
        bool dup = false;
        for (const std::string& e : out) {
            if (PathsEqual(e, norm)) {
                dup = true;
                break;
            }
        }
        if (!dup) out.push_back(norm);
        if (static_cast<int>(out.size()) >= kRecentFoldersMax) break;
    }
    return out;
}

void SaveRecentFolders(const std::vector<std::string>& paths) {
    std::ofstream out(PrefsPath(), std::ios::trunc);
    if (!out) return;
    out << "# MM2ED prefs\n[recent]\n";
    int n = 0;
    for (const std::string& p : paths) {
        if (!IsMm2DataFolder(p)) continue;
        out << NormalizeFolder(p) << "\n";
        if (++n >= kRecentFoldersMax) break;
    }
}

void RememberRecentFolder(const std::string& folderPath) {
    if (!IsMm2DataFolder(folderPath)) return;
    const std::string norm = NormalizeFolder(folderPath);
    std::vector<std::string> list = LoadRecentFolders();
    list.erase(std::remove_if(list.begin(), list.end(),
                              [&](const std::string& e) { return PathsEqual(e, norm); }),
               list.end());
    list.insert(list.begin(), norm);
    if (static_cast<int>(list.size()) > kRecentFoldersMax)
        list.resize(static_cast<size_t>(kRecentFoldersMax));
    SaveRecentFolders(list);
}

void ClearRecentFolders() { SaveRecentFolders({}); }

}  // namespace mm2
