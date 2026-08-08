#pragma once
// ============================================================================
// scanner.h  —  Process finding + pattern scanning utilities
// ============================================================================
#include <windows.h>
#include <tlhelp32.h>
#include <psapi.h>
#include <cstdint>
#include <string>
#include <vector>
#include <cstdio>
#include <cctype>
#include <cstring>

namespace Scanner {

struct ProcessInfo {
    DWORD       pid = 0;
    std::wstring name;
};

struct CachedModule {
    bool        valid = false;
    uintptr_t   base  = 0;
    uint32_t    size  = 0;
    std::vector<uint8_t> bytes;
    HANDLE      hProcess = nullptr;

    CachedModule() = default;
    CachedModule(const CachedModule&) = delete;
    CachedModule& operator=(const CachedModule&) = delete;
    CachedModule(CachedModule&& o) noexcept { *this = std::move(o); }
    CachedModule& operator=(CachedModule&& o) noexcept {
        if (this != &o) {
            if (hProcess) CloseHandle(hProcess);
            valid = o.valid; o.valid = false;
            base  = o.base;  o.base = 0;
            size  = o.size;  o.size = 0;
            bytes = std::move(o.bytes);
            hProcess = o.hProcess; o.hProcess = nullptr;
        }
        return *this;
    }
    ~CachedModule() {
        if (hProcess) { CloseHandle(hProcess); hProcess = nullptr; }
    }
};

// ---------- Helpers ----------
static inline std::wstring to_lower(const std::wstring& s) {
    std::wstring r = s;
    for (auto& c : r) c = (wchar_t)towlower(c);
    return r;
}

// Find the FiveM GTA process (FiveM_bXXXX_GTAProcess.exe preferred).
inline bool FindFiveM(ProcessInfo& out, std::string& buildStr) {
    out = {0, L""};
    buildStr = "Unknown";

    PROCESSENTRY32W pe{};
    pe.dwSize = sizeof(pe);
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return false;

    DWORD bestPid = 0;
    std::wstring bestName;
    int bestScore = -1; // prefer GTAProcess over CitiLaunch over FiveM.exe

    if (Process32FirstW(snap, &pe)) {
        do {
            std::wstring lname = to_lower(pe.szExeFile);
            int score = -1;
            if (lname.find(L"fivem_b") != std::wstring::npos && lname.find(L"gtaprocess") != std::wstring::npos)
                score = 100;                 // FiveM_b3751_GTAProcess.exe
            else if (lname.find(L"fivem") != std::wstring::npos && lname.find(L".exe") != std::wstring::npos)
                score = 50;
            else if (lname.find(L"citilaunch") != std::wstring::npos)
                score = 10;

            if (score > bestScore) {
                bestScore = score;
                bestPid = pe.th32ProcessID;
                bestName = pe.szExeFile;
            }
        } while (Process32NextW(snap, &pe));
    }
    CloseHandle(snap);

    if (bestScore < 0) return false;

    out.pid = bestPid;
    out.name = bestName;

    // Extract build: FiveM_b<digits>_...
    size_t bpos = to_lower(bestName).find(L"_b");
    if (bpos != std::wstring::npos) {
        std::string num;
        for (size_t k = bpos + 2; k < bestName.size(); ++k) {
            if (iswdigit(bestName[k])) num.push_back((char)bestName[k]);
            else break;
        }
        if (!num.empty()) buildStr = num;
    }
    return true;
}

// Find a remote module by substring match (case-insensitive), returns base+size.
inline bool FindRemoteModule(HANDLE hProc, const std::wstring& containsLower,
                             uintptr_t& outBase, uint32_t& outSize) {
    outBase = 0; outSize = 0;

    // First try the main exe module
    HMODULE mainMod = nullptr;
    DWORD needed = 0;
    if (EnumProcessModulesEx(hProc, &mainMod, sizeof(mainMod), &needed, LIST_MODULES_64BIT)) {
        wchar_t modName[MAX_PATH] = {};
        if (GetModuleBaseNameW(hProc, mainMod, modName, MAX_PATH)) {
            std::wstring lnm = to_lower(modName);
            if (lnm.find(containsLower) != std::wstring::npos) {
                MODULEINFO mi{};
                if (GetModuleInformation(hProc, mainMod, &mi, sizeof(mi))) {
                    outBase = (uintptr_t)mi.lpBaseOfDll;
                    outSize = (uint32_t)mi.SizeOfImage;
                    return true;
                }
            }
        }
    }

    // Enumerate all modules
    std::vector<HMODULE> mods(1024);
    needed = 0;
    if (!EnumProcessModulesEx(hProc, mods.data(), (DWORD)(mods.size() * sizeof(HMODULE)), &needed, LIST_MODULES_ALL))
        return false;

    size_t count = needed / sizeof(HMODULE);
    if (count > mods.size()) {
        mods.resize(count);
        EnumProcessModulesEx(hProc, mods.data(), (DWORD)(mods.size() * sizeof(HMODULE)), &needed, LIST_MODULES_ALL);
        count = needed / sizeof(HMODULE);
    }

    for (size_t i = 0; i < count; ++i) {
        wchar_t modName[MAX_PATH] = {};
        if (!GetModuleBaseNameW(hProc, mods[i], modName, MAX_PATH)) continue;
        std::wstring lnm = to_lower(modName);
        if (lnm.find(containsLower) != std::wstring::npos) {
            MODULEINFO mi{};
            if (GetModuleInformation(hProc, mods[i], &mi, sizeof(mi))) {
                outBase = (uintptr_t)mi.lpBaseOfDll;
                outSize = (uint32_t)mi.SizeOfImage;
                return true;
            }
        }
    }
    return false;
}

// Parse "48 8B 05 ? ? ? ? 45 33" into pattern bytes + mask.
inline bool ParseSignature(const char* sig, std::vector<uint8_t>& outBytes, std::vector<bool>& outMask) {
    outBytes.clear(); outMask.clear();
    const char* p = sig;
    while (*p) {
        while (*p == ' ' || *p == '\t') ++p;
        if (!*p) break;
        if (*p == '?' || *p == '*') {
            outBytes.push_back(0);
            outMask.push_back(false);
            if (p[1] == '?') ++p;
            ++p;
            continue;
        }
        char hex[3] = { p[0], p[1], 0 };
        uint8_t b = (uint8_t)strtoul(hex, nullptr, 16);
        outBytes.push_back(b);
        outMask.push_back(true);
        p += 2;
    }
    return !outBytes.empty();
}

// Scan a byte buffer for the pattern. Returns offset into buffer, or -1.
inline int64_t ScanBuffer(const uint8_t* data, size_t dataLen,
                          const std::vector<uint8_t>& pat,
                          const std::vector<bool>& mask) {
    const size_t patLen = pat.size();
    if (!patLen || dataLen < patLen) return -1;
    for (size_t i = 0; i <= dataLen - patLen; ++i) {
        bool ok = true;
        for (size_t j = 0; j < patLen; ++j) {
            if (mask[j] && data[i + j] != pat[j]) { ok = false; break; }
        }
        if (ok) return (int64_t)i;
    }
    return -1;
}

// Read one remote pointer safely.
inline bool ReadRemotePtr(HANDLE h, uintptr_t addr, uintptr_t& out) {
    out = 0;
    SIZE_T r = 0;
    return ReadProcessMemory(h, (LPCVOID)addr, &out, sizeof(out), &r) && r == sizeof(out);
}

// Cache module bytes from remote process into memory.
inline CachedModule CacheModule(const ProcessInfo& pi, const std::wstring& modSubstr) {
    CachedModule c;
    c.hProcess = OpenProcess(PROCESS_VM_READ | PROCESS_QUERY_INFORMATION | PROCESS_QUERY_LIMITED_INFORMATION,
                             FALSE, pi.pid);
    if (!c.hProcess) {
        // try without LIMITED
        c.hProcess = OpenProcess(PROCESS_VM_READ | PROCESS_QUERY_INFORMATION, FALSE, pi.pid);
    }
    if (!c.hProcess) return c;

    std::wstring needle = to_lower(modSubstr);
    uintptr_t base = 0; uint32_t size = 0;
    if (!FindRemoteModule(c.hProcess, needle, base, size)) {
        // Fallback: main module
        if (!FindRemoteModule(c.hProcess, L"", base, size)) {
            CloseHandle(c.hProcess); c.hProcess = nullptr;
            return c;
        }
    }

    c.base = base;
    c.size = size;
    c.bytes.resize(size);

    // Read in chunks to be safe across page boundaries
    SIZE_T total = 0;
    uint8_t* dst = c.bytes.data();
    uintptr_t src = base;
    uint32_t left = size;
    while (left) {
        SIZE_T chunk = left > 0x10000 ? 0x10000 : left;
        SIZE_T got = 0;
        if (!ReadProcessMemory(c.hProcess, (LPCVOID)src, dst, chunk, &got) || got == 0) {
            // skip unreadable page
            DWORD old = 0;
            VirtualProtectEx(c.hProcess, (LPVOID)src, chunk, PAGE_EXECUTE_READWRITE, &old);
            ReadProcessMemory(c.hProcess, (LPCVOID)src, dst, chunk, &got);
            if (got == 0) { dst += chunk; src += chunk; left -= (uint32_t)chunk; memset(dst - chunk, 0, chunk); continue; }
        }
        dst += got; src += got; left -= (uint32_t)got; total += got;
    }

    c.valid = (total > 0);
    return c;
}

struct PatternDef {
    const char* name;
    const char* signature;
    int         offset; // bytes after match start where the displacement/pointer sits
    int         extra;  // number of remote pointer dereferences after resolution
    bool        rip;    // if true, treat (match+offset) as an x86_64 RIP-relative disp32
};

// Find a pattern in a cached module. Returns 0 on failure.
inline uintptr_t FindPattern(const CachedModule& mod, const PatternDef& pd) {
    if (!mod.valid || !mod.hProcess) return 0;

    std::vector<uint8_t> pat;
    std::vector<bool> mask;
    if (!ParseSignature(pd.signature, pat, mask)) return 0;

    int64_t hitOff = ScanBuffer(mod.bytes.data(), mod.bytes.size(), pat, mask);
    if (hitOff < 0) return 0;

    uintptr_t hit = mod.base + (uintptr_t)hitOff;
    uintptr_t cursor = hit + pd.offset;

    if (pd.rip) {
        // Read 32-bit signed displacement at cursor, target = cursor + 4 + disp
        if ((size_t)hitOff + pd.offset + 4 > mod.bytes.size()) return 0;
        int32_t disp = 0;
        memcpy(&disp, &mod.bytes[(size_t)hitOff + pd.offset], sizeof(disp));
        cursor = (cursor + 4) + (uintptr_t)(intptr_t)disp;
    }

    // Follow pointer chain
    for (int i = 0; i < pd.extra; ++i) {
        uintptr_t next = 0;
        if (!ReadRemotePtr(mod.hProcess, cursor, next) || next == 0) return 0;
        cursor = next;
    }
    return cursor;
}

// Convenience overload (string sig) for backward compat.
inline uintptr_t FindPattern(const CachedModule& mod,
                             const std::string& sig,
                             int offset, int extra, bool rip) {
    PatternDef pd{ "", sig.c_str(), offset, extra, rip };
    return FindPattern(mod, pd);
}

} // namespace Scanner
