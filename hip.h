#pragma once

#include <stdio.h>
#include <stdint.h>

// https://heavyironmodding.org/wiki/EvilEngine/HIP_(File_Format)

#define HIP_MAX_STACK_DEPTH 8
#define HIP_MAX_PLATFORM_STRINGS 4
#define HIP_STRING_SIZE 32

class Hip
{
public:
    Hip();
    ~Hip();

    bool open(const char* path);
    void close();

    bool read();

    struct HIPA {} hipa;
    struct PACK {} pack;
    struct PVER {
        uint32_t subVersion;
        uint32_t clientVersion;
        uint32_t compatVersion;
    } pver;
    struct PFLG {
        uint32_t flags;
    } pflg;
    struct PCNT {
        uint32_t assetCount;
        uint32_t layerCount;
        uint32_t maxAssetSize;
        uint32_t maxLayerSize;
        uint32_t maxXformAssetSize;
    } pcnt;
    struct PCRT {
        uint32_t time;
        char string[HIP_STRING_SIZE];
    } pcrt;
    struct PMOD {
        uint32_t time;
    } pmod;
    struct PLAT {
        bool exists;
        uint32_t id;
        int stringCount;
        char strings[HIP_MAX_PLATFORM_STRINGS][HIP_STRING_SIZE];
    } plat;
    struct DICT {} dict;
    struct ATOC {} atoc;
    struct AINF {
        uint32_t ainf;
    } ainf;
    struct AHDR {
        uint32_t id;
        uint32_t type;
        uint32_t offset;
        uint32_t size;
        uint32_t plus;
        uint32_t flags;
        char* data;
    } *ahdr;
    struct ADBG {
        uint32_t align;
        char name[HIP_STRING_SIZE];
        char filename[HIP_STRING_SIZE];
        uint32_t checksum;
    } *adbg;
    struct LTOC {} ltoc;
    struct LINF {
        uint32_t linf;
    } linf;
    struct LHDR {
        uint32_t type;
        uint32_t assetCount;
        uint32_t* assetIDs;
    } *lhdr;
    struct LDBG {
        uint32_t ldbg;
    } *ldbg;
    struct STRM {} strm;
    struct DHDR {
        uint32_t dhdr;
    } dhdr;
    struct DPAK {
        uint32_t padAmount;
        char* data;
    } dpak;

private:
    struct Block
    {
        uint32_t id;
        uint32_t endpos;
    };

    FILE* file;
    Block stack[HIP_MAX_STACK_DEPTH];
    int stackDepth;
    uint32_t* layerAssetIDs;

    bool readHIPA();
    bool readPACK();
    bool readPVER();
    bool readPFLG();
    bool readPCNT();
    bool readPCRT();
    bool readPMOD();
    bool readPLAT();
    bool readDICT();
    bool readATOC();
    bool readAINF();
    bool readAHDR(int i);
    bool readADBG(int i);
    bool readLTOC();
    bool readLINF();
    bool readLHDR(int i, uint32_t* assetIDs);
    bool readLDBG(int i);
    bool readSTRM();
    bool readDHDR();
    bool readDPAK();

    uint32_t enterBlock();
    void exitBlock();
};