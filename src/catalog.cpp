// Reads building definitions from media_soviet/buildings_types and names from the language file.
#include "common.h"
#include <fstream>
#include <sstream>
#include <map>
#include <algorithm>

static std::vector<BuildingType> g_types;

static std::vector<unsigned char> ReadAll(const std::wstring& path) {
    std::vector<unsigned char> d;
    HANDLE h = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
    if (h == INVALID_HANDLE_VALUE) return d;
    DWORD sz = GetFileSize(h, nullptr), rd = 0;
    d.resize(sz);
    if (sz) ReadFile(h, d.data(), sz, &rd, nullptr);
    CloseHandle(h);
    d.resize(rd);
    return d;
}

// .btf: big-endian u32 count, u32 ?, u32 totalChars; then count * {u32 id, u32 offset, u16 len}; then UTF-16BE text.
static std::map<int, std::wstring> LoadNames(const std::wstring& path) {
    std::map<int, std::wstring> names;
    auto d = ReadAll(path);
    if (d.size() < 12) return names;
    auto be32 = [&](size_t o) { return (unsigned)(d[o] << 24 | d[o + 1] << 16 | d[o + 2] << 8 | d[o + 3]); };
    size_t count = be32(0);
    size_t text = 12 + count * 10;
    if (text > d.size()) return names;
    for (size_t i = 0; i < count; ++i) {
        size_t e = 12 + i * 10;
        int id = (int)be32(e);
        size_t off = be32(e + 4), len = (size_t)(d[e + 8] << 8 | d[e + 9]);
        size_t p = text + off * 2;
        if (p + len * 2 > d.size()) continue;
        std::wstring s;
        for (size_t k = 0; k < len; ++k) s.push_back((wchar_t)(d[p + k * 2] << 8 | d[p + k * 2 + 1]));
        names[id] = s;
    }
    return names;
}

namespace catalog {

const std::vector<BuildingType>& All() { return g_types; }

void Load() {
    g_types.clear();
    std::wstring media = GameDir() + L"media_soviet\\";
    auto names = LoadNames(media + L"sovietRussian.btf");
    if (names.empty()) names = LoadNames(media + L"sovietEnglish.btf");
    std::wstring dir = media + L"buildings_types\\";
    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileW((dir + L"*.ini").c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) { Log("catalog: buildings_types not found"); return; }
    do {
        std::wstring file = fd.cFileName;
        std::wstring stem = file.substr(0, file.size() - 4);
        auto data = ReadAll(dir + file);
        if (data.empty()) continue;
        BuildingType b;
        b.id = WideToUtf8(stem);
        int nameId = -1;
        std::istringstream ss(std::string(data.begin(), data.end()));
        std::string line;
        while (std::getline(ss, line)) {
            size_t s = line.find_first_not_of(" \t");
            if (s == std::string::npos || line[s] != '$') continue;
            std::istringstream ls(line.substr(s + 1));
            std::string key; ls >> key;
            if (key == "NAME") ls >> nameId;
            else if (key.rfind("TYPE_", 0) == 0) { if (b.category.empty()) b.category = key.substr(5); }
            else if (key == "STORAGE") {
                std::string res; int n = 0; ls >> res >> n;
                if (res == "RESOURCE_TRANSPORT_PASSANGER" && n > b.capacity) b.capacity = n;
            }
            else if (key == "QUALITY_OF_LIVING") ls >> b.quality;
            else if (key == "WORKERS_NEEDED") ls >> b.workers;
        }
        if (b.category.empty()) continue;
        auto bb = ReadAll(dir + stem + L".bbox");
        if (bb.size() < 24) continue;
        float f[6];
        memcpy(f, bb.data() + bb.size() - 24, 24);
        b.sx = f[3] - f[0]; b.sz = f[5] - f[2]; b.height = f[4];
        if (!(b.sx > 0.5f && b.sz > 0.5f && b.sx < 2000 && b.sz < 2000)) continue;
        auto it = names.find(nameId);
        b.name = (it != names.end() && !it->second.empty()) ? WideToUtf8(it->second) : b.id;
        g_types.push_back(b);
    } while (FindNextFileW(h, &fd));
    FindClose(h);
    std::sort(g_types.begin(), g_types.end(), [](const BuildingType& a, const BuildingType& b) {
        if (a.category != b.category) return a.category < b.category;
        if (a.capacity != b.capacity) return a.capacity > b.capacity;
        return a.id < b.id;
    });
    int living = 0;
    for (auto& t : g_types) if (t.category == "LIVING") ++living;
    Log("catalog: %d buildings, %d residential, %d names", (int)g_types.size(), living, (int)names.size());
}

} // namespace catalog
