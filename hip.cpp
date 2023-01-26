#include "hip.h"

#include <string.h>
#include <stdlib.h>
#include <assert.h>

#define PRINT_BLOCKS 0

#define BLKID(a,b,c,d) ((a<<24)|(b<<16)|(c<<8)|(d<<0))

static char* blockIDString(uint32_t id) {
    static char buf[5] = {};
    buf[0] = (id & 0xFF000000) >> 24;
    buf[1] = (id & 0x00FF0000) >> 16;
    buf[2] = (id & 0x0000FF00) >> 8;
    buf[3] = (id & 0x000000FF) >> 0;
    return buf;
}

static bool readLong(FILE* file, uint32_t* x)
{
    assert(x);
    assert(file);
    if (!x) return false;
    if (!file) return false;

    uint32_t val;

    size_t bytesRead = fread_s((void*)&val, sizeof(uint32_t), 1, sizeof(uint32_t), file);
    if (bytesRead != sizeof(uint32_t)) {
        return false;
    }

    // Swap endian
    val = ((val & 0xFF000000) >> 24)
        | ((val & 0x00FF0000) >> 8)
        | ((val & 0x0000FF00) << 8)
        | ((val & 0x000000FF) << 24);

    *x = val;
    return true;
}

static bool readString(FILE* file, char* buf, size_t bufsize)
{
    assert(buf);
    assert(file);
    if (!buf) return false;
    if (!file) return false;

    size_t len = 0;
    char c = '\0';

    // Read characters into buffer
    while (len < bufsize) {
        size_t bytesRead = fread_s((void*)&c, sizeof(char), 1, sizeof(char), file);
        if (bytesRead != sizeof(char)) {
            return false;
        }

        buf[len++] = c;

        if (c == '\0') break;
    }

    if (bufsize > 0) {
        buf[bufsize - 1] = '\0';
    }

    // If max size reached and there are still more characters left, read them (and throw em away)
    if (bufsize == 0 || (len == bufsize && c != '\0')) {
        while (true) {
            size_t bytesRead = fread_s((void*)&c, sizeof(char), sizeof(char), 1, file);
            if (bytesRead != sizeof(char)) {
                return false;
            }

            len++;

            if (c == '\0') break;
        }
    }

    // Skip padding byte
    if (len & 1) {
        fseek(file, 1, SEEK_CUR);
    }

    long pos = ftell(file);

    return true;
}

Hip::Hip()
{
    memset(this, 0, sizeof(*this));
}

Hip::~Hip()
{
    close();

    if (ahdr) free(ahdr);
    if (lhdr) free(lhdr);
    if (layerAssetIDs) free(layerAssetIDs);
    if (dpak.data) free(dpak.data);
}

bool Hip::open(const char* path)
{
    fopen_s(&file, path, "rb");
    return (file != nullptr);
}

void Hip::close()
{
    if (file) {
        fclose(file);
        file = nullptr;
    }
}

bool Hip::read()
{
    if (!file) {
        fprintf(stderr, "HIP: File not opened\n");
        return false;
    }

    bool valid = false;
    while (uint32_t cid = enterBlock()) {
        switch (cid) {
        case BLKID('H','I','P','A'):
            if (!readHIPA()) {
                fprintf(stderr, "HIP: Failed to read HIPA chunk\n");
                return false;
            }
            valid = true;
            break;
        case BLKID('P','A','C','K'):
            if (!readPACK()) {
                fprintf(stderr, "HIP: Failed to read PACK chunk\n");
                return false;
            }
            break;
        case BLKID('D','I','C','T'):
            if (!readDICT()) {
                fprintf(stderr, "HIP: Failed to read DICT chunk\n");
                return false;
            }
            break;
        case BLKID('S','T','R','M'):
            if (!readSTRM()) {
                fprintf(stderr, "HIP: Failed to read STRM chunk\n");
                return false;
            }
            break;
        }

        exitBlock();

        if (!valid) {
            fprintf(stderr, "HIP: Not a valid HIP file\n");
            return false;
        }
    }

    return true;
}

bool Hip::readHIPA()
{
    return true;
}

bool Hip::readPACK()
{
    while (uint32_t cid = enterBlock()) {
        switch (cid) {
        case BLKID('P','V','E','R'):
            if (!readPVER()) {
                fprintf(stderr, "HIP: Failed to read PVER chunk\n");
                return false;
            }
            break;
        case BLKID('P','F','L','G'):
            if (!readPFLG()) {
                fprintf(stderr, "HIP: Failed to read PFLG chunk\n");
                return false;
            }
            break;
        case BLKID('P','C','N','T'):
            if (!readPCNT()) {
                fprintf(stderr, "HIP: Failed to read PCNT chunk\n");
                return false;
            }
            break;
        case BLKID('P','C','R','T'):
            if (!readPCRT()) {
                fprintf(stderr, "HIP: Failed to read PCRT chunk\n");
                return false;
            }
            break;
        case BLKID('P','M','O','D'):
            if (!readPMOD()) {
                fprintf(stderr, "HIP: Failed to read PMOD chunk\n");
                return false;
            }
            break;
        case BLKID('P','L','A','T'):
            if (!readPLAT()) {
                fprintf(stderr, "HIP: Failed to read PLAT chunk\n");
                return false;
            }
            break;
        }

        exitBlock();
    }

    return true;
}

bool Hip::readPVER()
{
    if (!readLong(file, &pver.subVersion)) return false;
    if (!readLong(file, &pver.clientVersion)) return false;
    if (!readLong(file, &pver.compatVersion)) return false;

    return true;
}

bool Hip::readPFLG()
{
    if (!readLong(file, &pflg.flags)) return false;

    return true;
}

bool Hip::readPCNT()
{
    if (!readLong(file, &pcnt.assetCount)) return false;
    if (!readLong(file, &pcnt.layerCount)) return false;
    if (!readLong(file, &pcnt.maxAssetSize)) return false;
    if (!readLong(file, &pcnt.maxLayerSize)) return false;
    if (!readLong(file, &pcnt.maxXformAssetSize)) return false;

    return true;
}

bool Hip::readPCRT()
{
    if (!readLong(file, &pcrt.time)) return false;
    if (!readString(file, pcrt.string, HIP_STRING_SIZE)) return false;

    return true;
}

bool Hip::readPMOD()
{
    if (!readLong(file, &pmod.time)) return false;

    return true;
}

bool Hip::readPLAT()
{
    plat.exists = true;

    if (!readLong(file, &plat.id)) return false;

    Block& blk = stack[stackDepth-1];
    while ((uint32_t)ftell(file) < blk.endpos) {
        if (plat.stringCount >= HIP_MAX_PLATFORM_STRINGS) {
            printf("HIP: Warning: more strings than expected in PLAT chunk, skipping (max is %d)\n", HIP_MAX_PLATFORM_STRINGS);
            break;
        }

        if (!readString(file, plat.strings[plat.stringCount++], HIP_STRING_SIZE)) return false;
    }

    return true;
}

bool Hip::readDICT()
{
    layerAssetIDs = (uint32_t*)malloc(sizeof(uint32_t) * pcnt.assetCount);

    // Allocate assets
    {
        size_t size = (sizeof(AHDR) + sizeof(ADBG)) * pcnt.assetCount;
        void* buf = malloc(size);
        assert(buf);
        memset(buf, 0, size);

        ahdr = (AHDR*)buf;
        adbg = (ADBG*)(ahdr + pcnt.assetCount);
    }

    // Allocate layers
    {
        size_t size = (sizeof(LHDR) + sizeof(LDBG)) * pcnt.layerCount;
        void* buf = malloc(size);
        assert(buf);
        memset(buf, 0, size);

        lhdr = (LHDR*)buf;
        ldbg = (LDBG*)(lhdr + pcnt.layerCount);
    }

    while (uint32_t cid = enterBlock()) {
        switch (cid) {
        case BLKID('A','T','O','C'):
            if (!readATOC()) {
                fprintf(stderr, "HIP: Failed to read ATOC chunk\n");
                return false;
            }
            break;
        case BLKID('L','T','O','C'):
            if (!readLTOC()) {
                fprintf(stderr, "HIP: Failed to read LTOC chunk\n");
                return false;
            }
            break;
        }

        exitBlock();
    }

    return true;
}

bool Hip::readATOC()
{
    int i = 0;
    while (uint32_t cid = enterBlock()) {
        switch (cid) {
        case BLKID('A','I','N','F'):
            if (!readAINF()) {
                fprintf(stderr, "HIP: Failed to read AINF chunk\n");
                return false;
            }
            break;
        case BLKID('A','H','D','R'):
            if (!readAHDR(i)) {
                fprintf(stderr, "HIP: Failed to read AHDR chunk\n");
                return false;
            }
            i++;
            break;
        }

        exitBlock();
    }
    assert(i == pcnt.assetCount);

    return true;
}

bool Hip::readAINF()
{
    if (!readLong(file, &ainf.ainf)) return false;

    return true;
}

bool Hip::readAHDR(int i)
{
    if (!readLong(file, &ahdr[i].id)) return false;
    if (!readLong(file, &ahdr[i].type)) return false;
    if (!readLong(file, &ahdr[i].offset)) return false;
    if (!readLong(file, &ahdr[i].size)) return false;
    if (!readLong(file, &ahdr[i].plus)) return false;
    if (!readLong(file, &ahdr[i].flags)) return false;

    while (uint32_t cid = enterBlock()) {
        switch (cid) {
        case BLKID('A','D','B','G'):
            if (!readADBG(i)) {
                fprintf(stderr, "HIP: Failed to read ADBG chunk\n");
                return false;
            }
            break;
        }

        exitBlock();
    }

    return true;
}

bool Hip::readADBG(int i)
{
    if (!readLong(file, &adbg[i].align)) return false;
    if (!readString(file, adbg[i].name, HIP_STRING_SIZE)) return false;
    if (!readString(file, adbg[i].filename, HIP_STRING_SIZE)) return false;
    if (!readLong(file, &adbg[i].checksum)) return false;

    return true;
}

bool Hip::readLTOC()
{
    int i = 0;
    uint32_t* assetIDs = layerAssetIDs;
    while (uint32_t cid = enterBlock()) {
        switch (cid) {
        case BLKID('L','I','N','F'):
            if (!readLINF()) {
                fprintf(stderr, "HIP: Failed to read LINF chunk\n");
                return false;
            }
            break;
        case BLKID('L','H','D','R'):
            if (!readLHDR(i, assetIDs)) {
                fprintf(stderr, "HIP: Failed to read LHDR chunk\n");
                return false;
            }
            assetIDs += lhdr[i].assetCount;
            i++;
            break;
        }

        exitBlock();
    }
    assert((assetIDs - layerAssetIDs) == pcnt.assetCount);
    assert(i == pcnt.layerCount);

    return true;
}

bool Hip::readLINF()
{
    if (!readLong(file, &linf.linf)) return false;

    return true;
}

bool Hip::readLHDR(int i, uint32_t* assetIDs)
{
    if (!readLong(file, &lhdr[i].type)) return false;
    if (!readLong(file, &lhdr[i].assetCount)) return false;

    if (lhdr[i].assetCount) {
        lhdr[i].assetIDs = assetIDs;
        for (uint32_t j = 0; j < lhdr[i].assetCount; j++) {
            if (!readLong(file, &assetIDs[j])) return false;
        }
    }

    while (uint32_t cid = enterBlock()) {
        switch (cid) {
        case BLKID('L','D','B','G'):
            if (!readLDBG(i)) {
                fprintf(stderr, "HIP: Failed to read LDBG chunk\n");
                return false;
            }
            break;
        }

        exitBlock();
    }

    return true;
}

bool Hip::readLDBG(int i)
{
    if (!readLong(file, &ldbg[i].ldbg)) return false;

    return true;
}

bool Hip::readSTRM()
{
    while (uint32_t cid = enterBlock()) {
        switch (cid) {
        case BLKID('D','H','D','R'):
            if (!readDHDR()) {
                fprintf(stderr, "HIP: Failed to read DHDR chunk\n");
                return false;
            }
            break;
        case BLKID('D','P','A','K'):
            if (!readDPAK()) {
                fprintf(stderr, "HIP: Failed to read DPAK chunk\n");
                return false;
            }
            break;
        }

        exitBlock();
    }

    return true;
}

bool Hip::readDHDR()
{
    if (!readLong(file, &dhdr.dhdr)) return false;

    return true;
}

bool Hip::readDPAK()
{
    if (pcnt.assetCount == 0) return true;

    if (!readLong(file, &dpak.padAmount)) return false;
    fseek(file, dpak.padAmount, SEEK_CUR);

    uint32_t dataStart = ftell(file);
    uint32_t dataSize = stack[stackDepth-1].endpos - dataStart;

    dpak.data = (char*)malloc(dataSize);
    assert(dpak.data);

    if (fread(dpak.data, 1, dataSize, file) != dataSize) {
        fprintf(stderr, "HIP: Failed to read DPAK data\n");
        return false;
    }

    for (uint32_t i = 0; i < pcnt.assetCount; i++) {
        ahdr[i].data = dpak.data + ahdr[i].offset - dataStart;
    }

    return true;
}

uint32_t Hip::enterBlock()
{
    assert(stackDepth < HIP_MAX_STACK_DEPTH);
    if (stackDepth >= HIP_MAX_STACK_DEPTH) {
        fprintf(stderr, "HIP: Max block stack depth reached (%d)\n", HIP_MAX_STACK_DEPTH);
        return 0;
    }

    if (stackDepth > 0 && (uint32_t)ftell(file) >= stack[stackDepth-1].endpos) {
        // End of current block reached (not an error)
        return 0;
    }

    uint32_t id, len;
    if (!readLong(file, &id)) return 0;
    if (!readLong(file, &len)) return 0;

    Block& blk = stack[stackDepth++];
    blk.id = id;
    blk.endpos = ftell(file) + len;

#if PRINT_BLOCKS
    for (int i = 0; i < stackDepth-1; i++) printf("  ");
    printf("%s: %d\n", blockIDString(id), len);
#endif

    return id;
}

void Hip::exitBlock()
{
    assert(stackDepth > 0);
    if (stackDepth <= 0) {
        fprintf(stderr, "HIP: Stack depth underflow\n");
        return;
    }

    Block& blk = stack[--stackDepth];
    fseek(file, blk.endpos, SEEK_SET);
}