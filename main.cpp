#include "hip.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <assert.h>

#include <vector>
#include <map>
#include <unordered_map>
#include <unordered_set>

#ifdef _WIN32
#include <windows.h>
#endif

#define VERSION "v1.0"

#define DEFAULT_COLUMN_WIDTH 50

static int columnWidth = DEFAULT_COLUMN_WIDTH;

static int additionCount = 0;
static int deletionCount = 0;
static int modificationCount = 0;
static bool countsEnabled = true;

struct Diff
{
    enum class Type
    {
        Addition,
        Deletion,
        Modification
    } type;
    char left[64];
    char right[64];
};

static std::vector<Diff> pverDiffs;
static std::vector<Diff> pflgDiffs;
static std::vector<Diff> pcntDiffs;
static std::vector<Diff> pcrtDiffs;
static std::vector<Diff> pmodDiffs;
static std::vector<Diff> platDiffs;
static std::vector<Diff> ainfDiffs;
static std::vector<Diff> assetAdditions;
static std::vector<Diff> assetDeletions;
static std::vector<Diff> assetModifications;
static std::vector<Diff> layerAdditions;
static std::vector<Diff> layerDeletions;
static std::vector<Diff> layerModifications;

template <class T = std::nullptr_t>
static void ADDITION(std::vector<Diff>& diffs, const char* fmt, T val = T())
{
    Diff diff;
    diff.type = Diff::Type::Addition;
    diff.left[0] = '\0';
    sprintf_s(diff.right, sizeof(diff.right), fmt, val);
    diffs.push_back(diff);
    if (countsEnabled) additionCount++;
}

template <class T = std::nullptr_t>
static void DELETION(std::vector<Diff>& diffs, const char* fmt, T val = T())
{
    Diff diff;
    diff.type = Diff::Type::Deletion;
    sprintf_s(diff.left, sizeof(diff.left), fmt, val);
    diff.right[0] = '\0';
    diffs.push_back(diff);
    if (countsEnabled) deletionCount++;
}

template <class T = std::nullptr_t>
static void MODIFICATION(std::vector<Diff>& diffs, const char* fmt, T left = T(), T right = T())
{
    Diff diff;
    diff.type = Diff::Type::Modification;
    sprintf_s(diff.left, sizeof(diff.left), fmt, left);
    sprintf_s(diff.right, sizeof(diff.right), fmt, right);
    diffs.push_back(diff);
    if (countsEnabled) modificationCount++;
}

// https://stackoverflow.com/questions/3585846/color-text-in-terminal-applications-in-unix
#define RED   "\x1B[31m"
#define GRN   "\x1B[32m"
#define YEL   "\x1B[33m"
#define BLU   "\x1B[34m"
#define MAG   "\x1B[35m"
#define CYN   "\x1B[36m"
#define WHT   "\x1B[37m"
#define RESET "\x1B[0m"

static void printDiffLine(const char* left, const char* right)
{
    char bufFmt[16];
    sprintf_s(bufFmt, sizeof(bufFmt), "%%-%ds", columnWidth);
    printf(bufFmt, left);
    printf(bufFmt, right);
    printf("\n");
}

static void printDiffHeader(const char* left, const char* right)
{
    printDiffLine(left, right);
    for (int i = 0; i < columnWidth * 2; i++) printf("=");
    printf("\n");
}

static void printDiff(const Diff& diff)
{
    switch (diff.type) {
    case Diff::Type::Addition:
        printf(GRN);
        break;
    case Diff::Type::Deletion:
        printf(RED);
        break;
    case Diff::Type::Modification:
        printf(YEL);
        break;
    }
    printDiffLine(diff.left, diff.right);
    printf(RESET);
}

static void printDiffs(const std::vector<Diff>& diffs, const char* title, int count = -1) {
    if (!diffs.empty()) {
        if (title) {
            if (count == -1) {
                printDiffLine(title, title);
            } else {
                char buf[64];
                sprintf_s(buf, sizeof(buf), "%s (%d)", title, count);
                printDiffLine(buf, buf);
            }
        }
        for (const Diff& diff : diffs) {
            printDiff(diff);
        }
    }
}

static int Stricmp(const char* a, const char* b)
{
    assert(a);
    assert(b);
    while (*a && *b) {
        char ca = tolower(*a);
        char cb = tolower(*b);
        if (ca < cb) return -1;
        if (ca > cb) return 1;
        a++;
        b++;
    }
    if (*a) return 1;
    if (*b) return -1;
    return 0;
}

static const char* filenameFromPath(const char* path)
{
    assert(path);
    int n = (int)(strlen(path) - 1);
    while (n >= 0) {
        if (path[n] == '/' || path[n] == '\\') {
            return &path[n+1];
        }
        n--;
    }
    return path;
}

// Remove newline from end to make diff cleaner
static void hackPCRTString(char* str)
{
    size_t len = strlen(str);
    if (str[len-1] == '\n') str[len-1] = '\0';
}

static void printVersion()
{
    printf("HIPDiff " VERSION " by seilweiss\n");
    printf("Built on " __DATE__ " at " __TIME__ "\n");
}

static void printUsage()
{
    printf("Usage:\n");
    printf("    hipdiff [-h] [-v] [-a] [-d] [-c] [-o] [-p] [-w <width>] <original HIP file> <modified HIP file>\n");
    printf("\n");
    printf("Options:\n");
    printf("    -h: Show help\n");
    printf("    -v: Show version\n");
    printf("    -a: Only show asset diffs\n");
    printf("    -d: Detailed asset diffs (AHDR and ADBG chunks)\n");
    printf("    -c: Ignore asset data if checksum matches\n");
    printf("    -o: Diff asset offsets\n");
    printf("    -p: Diff asset pluses\n");
    printf("    -w <width>: Set column width (default: %d)\n", DEFAULT_COLUMN_WIDTH);
}

int main(int argc, char** argv)
{
#ifdef _WIN32
    // Enable text coloring in console
    // https://learn.microsoft.com/en-us/windows/console/console-virtual-terminal-sequences

    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut == INVALID_HANDLE_VALUE)
    {
        return GetLastError();
    }

    DWORD dwMode = 0;
    if (!GetConsoleMode(hOut, &dwMode))
    {
        return GetLastError();
    }

    dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    if (!SetConsoleMode(hOut, dwMode))
    {
        return GetLastError();
    }
#endif

    if (argc <= 1) {
        printVersion();
        printf("\n");
        printUsage();
        return 1;
    }

    bool showHelp = false;
    bool showVersion = false;
    bool assetDiffsOnly = false;
    bool detailedAssets = false;
    bool ignoreDataIfChksumMatch = false;
    bool diffOffsets = false;
    bool diffPluses = false;
    const char* paths[2] = {};
    int pathCount = 0;

    for (int i = 1; i < argc; i++) {
        char* arg = argv[i];
        if (arg[0] == '-') {
            if (!Stricmp(arg, "-h")) showHelp = true;
            else if (!Stricmp(arg, "-v")) showVersion = true;
            else if (!Stricmp(arg, "-a")) assetDiffsOnly = true;
            else if (!Stricmp(arg, "-d")) detailedAssets = true;
            else if (!Stricmp(arg, "-c")) ignoreDataIfChksumMatch = true;
            else if (!Stricmp(arg, "-o")) diffOffsets = true;
            else if (!Stricmp(arg, "-p")) diffPluses = true;
            else if (!Stricmp(arg, "-w")) {
                char* width = argv[i+1];
                columnWidth = atoi(width);
                if (columnWidth <= 0) columnWidth = DEFAULT_COLUMN_WIDTH;
                i++;
            }
            else {
                printf("Unknown option '%s'\n", arg);
                printf("\n");
                printUsage();
                return 1;
            }
        } else {
            if (pathCount < 2) {
                paths[pathCount++] = arg;
            } else {
                printf("Too many arguments: '%s'\n", arg);
                printf("\n");
                printUsage();
                return 1;
            }
        }
    }

    if (showHelp) {
        printUsage();
        return 0;
    }

    if (showVersion) {
        printVersion();
        return 0;
    }

    if (pathCount == 0) {
        printf("Original HIP file argument missing\n");
        printf("\n");
        printUsage();
        return 1;
    } else if (pathCount == 1) {
        printf("Modified HIP file argument missing\n");
        printf("\n");
        printUsage();
        return 1;
    }

    const char* opath = paths[0];
    const char* mpath = paths[1];
    assert(opath);
    assert(mpath);

    Hip ohip, mhip;

    if (!ohip.open(opath)) {
        printf("Could not open file '%s'\n", opath);
        return 1;
    }

    if (!mhip.open(mpath)) {
        printf("Could not open file '%s'\n", mpath);
        return 1;
    }

    //printf("Reading HIP file '%s'\n", opath);
    if (!ohip.read()) {
        printf("Could not read file '%s'\n", opath);
        return 1;
    }

    //printf("Reading HIP file '%s'\n", mpath);
    if (!mhip.read()) {
        printf("Could not read file '%s'\n", mpath);
        return 1;
    }

    hackPCRTString(ohip.pcrt.string);
    hackPCRTString(mhip.pcrt.string);

    struct Index
    {
        int oidx = -1;
        int midx = -1;
    };

    std::map<uint32_t, Index> ahdrIndices;
    std::unordered_map<uint32_t, std::vector<Index>> lhdrIndices;
    std::map<uint32_t, Index> ahdrLHDRIndices;

    for (uint32_t i = 0; i < ohip.pcnt.assetCount; i++) {
        ahdrIndices[ohip.ahdr[i].id].oidx = i;
    }
    for (uint32_t i = 0; i < mhip.pcnt.assetCount; i++) {
        ahdrIndices[mhip.ahdr[i].id].midx = i;
    }

    int numAssetsAdded = 0;
    int numAssetsDeleted = 0;
    int numAssetsModified = 0;
    int numLayersAdded = 0;
    int numLayersDeleted = 0;
    int numLayersModified = 0;

    std::unordered_set<uint32_t> addedAssets;
    std::unordered_set<uint32_t> deletedAssets;

    if (!assetDiffsOnly) {
        std::map<uint32_t, int> mLayerCounts;
        for (uint32_t i = 0; i < ohip.pcnt.layerCount; i++) {
            uint32_t type = ohip.lhdr[i].type;
            Index idx;
            idx.oidx = i;
            lhdrIndices[type].push_back(idx);
        }
        for (uint32_t i = 0; i < mhip.pcnt.layerCount; i++) {
            uint32_t type = mhip.lhdr[i].type;
            if (lhdrIndices[type].size() < mLayerCounts[type] + 1) {
                Index idx;
                idx.midx = i;
                lhdrIndices[type].push_back(idx);
            } else {
                lhdrIndices[type][mLayerCounts[type]].midx = i;
            }
            mLayerCounts[type]++;
        }
        for (uint32_t i = 0; i < ohip.pcnt.layerCount; i++) {
            for (uint32_t j = 0; j < ohip.lhdr[i].assetCount; j++) {
                ahdrLHDRIndices[ohip.lhdr[i].assetIDs[j]].oidx = i;
            }
        }
        for (uint32_t i = 0; i < mhip.pcnt.layerCount; i++) {
            for (uint32_t j = 0; j < mhip.lhdr[i].assetCount; j++) {
                ahdrLHDRIndices[mhip.lhdr[i].assetIDs[j]].midx = i;
            }
        }
    }

    // Perform diff
    if (!assetDiffsOnly) {
        if (ohip.pver.subVersion != mhip.pver.subVersion)
            MODIFICATION(pverDiffs, "  subVersion: 0x%X", ohip.pver.subVersion, mhip.pver.subVersion);
        if (ohip.pver.clientVersion != mhip.pver.clientVersion)
            MODIFICATION(pverDiffs, "  clientVersion: 0x%X", ohip.pver.clientVersion, mhip.pver.clientVersion);
        if (ohip.pver.compatVersion != mhip.pver.compatVersion)
            MODIFICATION(pverDiffs, "  compatVersion: 0x%X", ohip.pver.compatVersion, mhip.pver.compatVersion);
        if (ohip.pflg.flags != mhip.pflg.flags)
            MODIFICATION(pflgDiffs, "  flags: 0x%X", ohip.pflg.flags, mhip.pflg.flags);
        if (ohip.pcnt.assetCount != mhip.pcnt.assetCount)
            MODIFICATION(pcntDiffs, "  assetCount: %d", ohip.pcnt.assetCount, mhip.pcnt.assetCount);
        if (ohip.pcnt.layerCount != mhip.pcnt.layerCount)
            MODIFICATION(pcntDiffs, "  layerCount: %d", ohip.pcnt.layerCount, mhip.pcnt.layerCount);
        if (ohip.pcnt.maxAssetSize != mhip.pcnt.maxAssetSize)
            MODIFICATION(pcntDiffs, "  maxAssetSize: %d", ohip.pcnt.maxAssetSize, mhip.pcnt.maxAssetSize);
        if (ohip.pcnt.maxLayerSize != mhip.pcnt.maxLayerSize)
            MODIFICATION(pcntDiffs, "  maxLayerSize: %d", ohip.pcnt.maxLayerSize, mhip.pcnt.maxLayerSize);
        if (ohip.pcnt.maxXformAssetSize != mhip.pcnt.maxXformAssetSize)
            MODIFICATION(pcntDiffs, "  maxXformAssetSize: %d", ohip.pcnt.maxXformAssetSize, mhip.pcnt.maxXformAssetSize);
        if (ohip.pcrt.time != mhip.pcrt.time)
            MODIFICATION(pcrtDiffs, "  time: %d", ohip.pcrt.time, mhip.pcrt.time);
        if (strcmp(ohip.pcrt.string, mhip.pcrt.string))
            MODIFICATION(pcrtDiffs, "  \"%s\"", ohip.pcrt.string, mhip.pcrt.string);
        if (ohip.pmod.time != mhip.pmod.time)
            MODIFICATION(pmodDiffs, "  time: %d", ohip.pmod.time, mhip.pmod.time);

        if (ohip.plat.exists || mhip.plat.exists) {
            if (ohip.plat.exists != mhip.plat.exists) {
                if (ohip.plat.exists && !mhip.plat.exists) {
                    DELETION(platDiffs, "  id: 0x%08X", ohip.plat.id);
                    for (int i = 0; i < ohip.plat.stringCount; i++) {
                        DELETION(platDiffs, "  \"%s\"", ohip.plat.strings[i]);
                    }
                } else {
                    ADDITION(platDiffs, "  id: 0x%08X", mhip.plat.id);
                    for (int i = 0; i < mhip.plat.stringCount; i++) {
                        ADDITION(platDiffs, "  \"%s\"", mhip.plat.strings[i]);
                    }
                }
            } else {
                if (ohip.plat.id != mhip.plat.id)
                    MODIFICATION(platDiffs, "  id: 0x%08X", ohip.plat.id, mhip.plat.id);

                int platStringCount = ohip.plat.stringCount;
                if (platStringCount < mhip.plat.stringCount) platStringCount = mhip.plat.stringCount;
                for (int i = 0; i < platStringCount; i++) {
                    if (i >= ohip.plat.stringCount) {
                        ADDITION(platDiffs, "  \"%s\"", mhip.plat.strings[i]);
                    } else if (i >= mhip.plat.stringCount) {
                        DELETION(platDiffs, "  \"%s\"", ohip.plat.strings[i]);
                    } else if (strcmp(mhip.plat.strings[i], ohip.plat.strings[i])) {
                        MODIFICATION(platDiffs, "  \"%s\"", ohip.plat.strings[i], mhip.plat.strings[i]);
                    }
                }
            }
        }

        if (ohip.ainf.ainf != mhip.ainf.ainf)
            MODIFICATION(ainfDiffs, "  ainf: %d", ohip.ainf.ainf, mhip.ainf.ainf);
    }

    for (auto it = ahdrIndices.begin(); it != ahdrIndices.end(); it++) {
        Index& a = it->second;
        assert(a.oidx != -1 || a.midx != -1);
        if (a.oidx == -1) {
            Hip::AHDR& mahdr = mhip.ahdr[a.midx];
            Hip::ADBG& madbg = mhip.adbg[a.midx];
            if (detailedAssets) {
                countsEnabled = false;
                ADDITION(assetAdditions, "  AHDR (%s)", madbg.name);
                ADDITION(assetAdditions, "    id: 0x%08X", mahdr.id);
                ADDITION(assetAdditions, "    type: 0x%08X", mahdr.type);
                ADDITION(assetAdditions, "    offset: %d", mahdr.offset);
                ADDITION(assetAdditions, "    size: %d", mahdr.size);
                ADDITION(assetAdditions, "    plus: %d", mahdr.plus);
                ADDITION(assetAdditions, "    flags: 0x%08X", mahdr.flags);
                ADDITION(assetAdditions, "    ADBG");
                ADDITION(assetAdditions, "      align: %d", madbg.align);
                ADDITION(assetAdditions, "      name: %s", madbg.name);
                ADDITION(assetAdditions, "      filename: %s", madbg.filename);
                ADDITION(assetAdditions, "      checksum: 0x%08X", madbg.checksum);
                additionCount++;
                countsEnabled = true;
            } else {
                ADDITION(assetAdditions, "  %s", madbg.name);
            }
            numAssetsAdded++;
            addedAssets.insert(mahdr.id);
        } else if (a.midx == -1) {
            Hip::AHDR& oahdr = ohip.ahdr[a.oidx];
            Hip::ADBG& oadbg = ohip.adbg[a.oidx];
            if (detailedAssets) {
                countsEnabled = false;
                DELETION(assetDeletions, "  AHDR (%s)", oadbg.name);
                DELETION(assetDeletions, "    id: 0x%08X", oahdr.id);
                DELETION(assetDeletions, "    type: 0x%08X", oahdr.type);
                DELETION(assetDeletions, "    offset: %d", oahdr.offset);
                DELETION(assetDeletions, "    size: %d", oahdr.size);
                DELETION(assetDeletions, "    plus: %d", oahdr.plus);
                DELETION(assetDeletions, "    flags: 0x%08X", oahdr.flags);
                DELETION(assetDeletions, "    ADBG");
                DELETION(assetDeletions, "      align: %d", oadbg.align);
                DELETION(assetDeletions, "      name: %s", oadbg.name);
                DELETION(assetDeletions, "      filename: %s", oadbg.filename);
                DELETION(assetDeletions, "      checksum: 0x%08X", oadbg.checksum);
                deletionCount++;
                countsEnabled = true;
            } else {
                DELETION(assetDeletions, "  %s", oadbg.name);
            }
            numAssetsDeleted++;
            deletedAssets.insert(oahdr.id);
        } else {
            Hip::AHDR& oahdr = ohip.ahdr[a.oidx];
            Hip::AHDR& mahdr = mhip.ahdr[a.midx];
            Hip::ADBG& oadbg = ohip.adbg[a.oidx];
            Hip::ADBG& madbg = mhip.adbg[a.midx];
            assert(oahdr.id == mahdr.id);

            bool dataChanged = false;
            if (ignoreDataIfChksumMatch) {
                if (oadbg.checksum != madbg.checksum) {
                    dataChanged = true;
                }
            } else {
                if (oahdr.size == mahdr.size) {
                    if (memcmp(oahdr.data, mahdr.data, oahdr.size)) {
                        dataChanged = true;
                    }
                } else {
                    dataChanged = true;
                }
            }

            if (detailedAssets) {
                std::vector<Diff> ahdrMods;
                std::vector<Diff> adbgMods;

                countsEnabled = false;

                MODIFICATION(ahdrMods, "  AHDR (%s)", oadbg.name, madbg.name);
                if (oahdr.id != mahdr.id) {
                    assert(false && "How did we get here?");
                    MODIFICATION(ahdrMods, "    id: 0x%08X", oahdr.id, mahdr.id);
                }
                if (oahdr.type != mahdr.type)
                    MODIFICATION(ahdrMods, "    type: 0x%08X", oahdr.type, mahdr.type);
                if (oahdr.offset != mahdr.offset && diffOffsets)
                    MODIFICATION(ahdrMods, "    offset: %d", oahdr.offset, mahdr.offset);
                if (oahdr.size != mahdr.size)
                    MODIFICATION(ahdrMods, "    size: %d", oahdr.size, mahdr.size);
                if (oahdr.plus != mahdr.plus && diffPluses)
                    MODIFICATION(ahdrMods, "    plus: %d", oahdr.plus, mahdr.plus);
                if (oahdr.flags != mahdr.flags)
                    MODIFICATION(ahdrMods, "    flags: 0x%08X", oahdr.flags, mahdr.flags);
                if (dataChanged)
                    MODIFICATION(ahdrMods, "    data changed");

                MODIFICATION(adbgMods, "    ADBG");
                if (oadbg.align != madbg.align)
                    MODIFICATION(adbgMods, "      align: %d", oadbg.align, madbg.align);
                if (strcmp(oadbg.name, madbg.name))
                    MODIFICATION(adbgMods, "      name: %s", oadbg.name, madbg.name);
                if (strcmp(oadbg.filename, madbg.filename))
                    MODIFICATION(adbgMods, "      filename: %s", oadbg.filename, madbg.filename);
                if (oadbg.checksum != madbg.checksum)
                    MODIFICATION(adbgMods, "      checksum: 0x%08X", oadbg.checksum, madbg.checksum);

                if (ahdrMods.size() > 1 || adbgMods.size() > 1) {
                    assetModifications.insert(assetModifications.end(), ahdrMods.begin(), ahdrMods.end());
                    if (adbgMods.size() > 1)
                        assetModifications.insert(assetModifications.end(), adbgMods.begin(), adbgMods.end());
                    modificationCount++;
                    numAssetsModified++;
                }

                countsEnabled = true;
            } else {
                if (oahdr.id != mahdr.id
                 || oahdr.type != mahdr.type
                 || (oahdr.offset != mahdr.offset && diffOffsets)
                 || oahdr.size != mahdr.size
                 || (oahdr.plus != mahdr.plus && diffPluses)
                 || oahdr.flags != mahdr.flags
                 || oadbg.align != madbg.align
                 || strcmp(oadbg.name, madbg.name)
                 || strcmp(oadbg.filename, madbg.filename)
                 || oadbg.checksum != madbg.checksum
                 || dataChanged) {
                    MODIFICATION(assetModifications, "  %s", oadbg.name, madbg.name);
                    numAssetsModified++;
                }
            }
        }
    }

    if (!assetDiffsOnly) {
        for (auto it = lhdrIndices.begin(); it != lhdrIndices.end(); it++) {
            for (Index& l : it->second) {
                assert(l.oidx != -1 || l.midx != -1);
                if (l.oidx == -1) {
                    Hip::LHDR& mlhdr = mhip.lhdr[l.midx];
                    Hip::LDBG& mldbg = mhip.ldbg[l.midx];
                    countsEnabled = false;
                    ADDITION(layerAdditions, "  LHDR (%d)", mlhdr.type);
                    ADDITION(layerAdditions, "    type: %d", mlhdr.type);
                    for (uint32_t i = 0; i < mlhdr.assetCount; i++) {
                        uint32_t id = mlhdr.assetIDs[i];
                        if (addedAssets.find(id) == addedAssets.end()) {
                            ADDITION(layerAdditions, "    %s", mhip.adbg[ahdrIndices[id].midx].name);
                        }
                    }
                    ADDITION(layerAdditions, "    LDBG");
                    ADDITION(layerAdditions, "      ldbg: %d", mldbg.ldbg);
                    additionCount++;
                    countsEnabled = true;
                    numLayersAdded++;
                } else if (l.midx == -1) {
                    Hip::LHDR& olhdr = ohip.lhdr[l.oidx];
                    Hip::LDBG& oldbg = ohip.ldbg[l.oidx];
                    countsEnabled = false;
                    DELETION(layerDeletions, "  LHDR (%d)", olhdr.type);
                    DELETION(layerDeletions, "    type: %d", olhdr.type);
                    //DELETION(layerDeletions, "    assetCount: %d", olhdr.assetCount);
                    for (uint32_t i = 0; i < olhdr.assetCount; i++) {
                        uint32_t id = olhdr.assetIDs[i];
                        if (deletedAssets.find(id) == deletedAssets.end()) {
                            DELETION(layerDeletions, "    %s", ohip.adbg[ahdrIndices[id].oidx].name);
                        }
                    }
                    DELETION(layerDeletions, "    LDBG");
                    DELETION(layerDeletions, "      ldbg: %d", oldbg.ldbg);
                    deletionCount++;
                    countsEnabled = true;
                    numLayersDeleted++;
                } else {
                    Hip::LHDR& olhdr = ohip.lhdr[l.oidx];
                    Hip::LDBG& oldbg = ohip.ldbg[l.oidx];
                    Hip::LHDR& mlhdr = mhip.lhdr[l.midx];
                    Hip::LDBG& mldbg = mhip.ldbg[l.midx];
                    assert(olhdr.type == mlhdr.type);

                    std::vector<Diff> lhdrMods;
                    std::vector<Diff> ldbgMods;

                    countsEnabled = false;

                    MODIFICATION(lhdrMods, "  LHDR (%d)", olhdr.type, mlhdr.type);
                    if (olhdr.type != mlhdr.type) {
                        assert(false && "How did we get here?");
                        MODIFICATION(lhdrMods, "    type: %d", olhdr.type, mlhdr.type);
                    }

                    for (auto it = ahdrLHDRIndices.begin(); it != ahdrLHDRIndices.end(); it++) {
                        uint32_t id = it->first;
                        Index& a = it->second;
                        assert(a.oidx != -1 || a.midx != -1);
                        if (a.oidx == l.oidx || a.midx == l.midx) {
                            if (a.oidx != l.oidx) {
                                if (addedAssets.find(id) == addedAssets.end()) {
                                    ADDITION(lhdrMods, "    \"%s\"", mhip.adbg[ahdrIndices[id].midx].name);
                                    additionCount++;
                                }
                            } else if (a.midx != l.midx) {
                                if (deletedAssets.find(id) == deletedAssets.end()) {
                                    DELETION(lhdrMods, "    \"%s\"", ohip.adbg[ahdrIndices[id].oidx].name);
                                    deletionCount++;
                                }
                            }
                        }
                    }

                    MODIFICATION(ldbgMods, "    LDBG");
                    if (oldbg.ldbg != mldbg.ldbg)
                        MODIFICATION(ldbgMods, "      ldbg: %d", oldbg.ldbg, mldbg.ldbg);

                    if (lhdrMods.size() > 1 || ldbgMods.size() > 1) {
                        layerModifications.insert(layerModifications.end(), lhdrMods.begin(), lhdrMods.end());
                        if (ldbgMods.size() > 1)
                            layerModifications.insert(layerModifications.end(), ldbgMods.begin(), ldbgMods.end());
                        modificationCount++;
                        numLayersModified++;
                    }

                    countsEnabled = true;
                }
            }
        }
    }

    const char* oname = opath /*filenameFromPath(opath)*/;
    const char* mname = mpath /*filenameFromPath(mpath)*/;

    int onameWidth = (int)(strlen(oname) + 1);
    int mnameWidth = (int)(strlen(oname) + 1);
    if (onameWidth > columnWidth) columnWidth = onameWidth;
    if (mnameWidth > columnWidth) columnWidth = mnameWidth;

    printDiffHeader(oname, mname);
    if (!assetDiffsOnly) {
        printDiffs(pverDiffs, "PVER");
        printDiffs(pflgDiffs, "PFLG");
        printDiffs(pcntDiffs, "PCNT");
        printDiffs(pcrtDiffs, "PCRT");
        printDiffs(pmodDiffs, "PMOD");
        printDiffs(platDiffs, "PLAT");
        printDiffs(ainfDiffs, "AINF");
    }
    printDiffs(assetAdditions, "Added assets", numAssetsAdded);
    printDiffs(assetDeletions, "Deleted assets", numAssetsDeleted);
    printDiffs(assetModifications, "Modified assets", numAssetsModified);
    if (!assetDiffsOnly) {
        printDiffs(layerAdditions, "Added layers", numLayersAdded);
        printDiffs(layerDeletions, "Deleted layers", numLayersDeleted);
        printDiffs(layerModifications, "Modified layers", numLayersModified);
    }

    printf("\n");
    printf("%d addition(s), %d deletion(s), %d modification(s)\n",
           additionCount, deletionCount, modificationCount);

    return 0;
}