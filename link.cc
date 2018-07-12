#include <cstdio>
#include <vector>
#include <cstring>

#include <unistd.h> // For ftruncate

using namespace std;

#include "o65linker.hh"
#include "msginsert.hh"
#include "dataarea.hh"
#include "romaddr.hh"
#include "space.hh"

#include "object.hh"

#include <getopt.h>

bool already_reprocessed = true; /* required by object.cc, not really used */
bool assembly_errors = false;

extern unsigned ROMmap_npages; // from romaddr.cc, number of 0x4000-byte pages
static unsigned MapperNo = 2;

namespace
{
    enum OutputFormat
    {
        IPSformat,
        O65format,
        RAWformat,
        NESformat
    } format = IPSformat;

    void SetOutputFormat(const std::string& s)
    {
        if(s == "ips") format = IPSformat;
        else if(s == "o65") format = O65format;
        else if(s == "raw") format = RAWformat;
        else if(s == "nes") format = NESformat;
        else
        {
            std::fprintf(stderr, "Error: Unknown output format %s'\n", s.c_str());
        }
    }
}

void MessageLinkingModules(unsigned count)
{
    //printf("Linking %u modules\n", count);
}

void MessageLoadingItem(const string& header)
{
    //printf("Loading %s\n", header.c_str());
}

void MessageWorking()
{
}

void MessageDone()
{
    //printf("- done\n");
}

void MessageModuleWithoutAddress(const string& name, const SegmentSelection seg)
{
    std::fprintf(stderr, "O65 linker: Module %s is still without address for seg %s\n",
        name.c_str(), GetSegmentName(seg).c_str());
}

void MessageUndefinedSymbol(const string& name)
{
    std::fprintf(stderr, "O65 linker: Symbol '%s' still undefined\n", name.c_str());
}

void MessageDuplicateDefinition(const string& name, unsigned nmods, unsigned ndefs)
{
    std::fprintf(stderr, "O65 linker: Symbol '%s' defined in %u module(s) and %u global(s)\n",
        name.c_str(), nmods, ndefs);
}

void MessageUndefinedSymbols(unsigned n)
{
    std::fprintf(stderr, "O65 linker: Still %u undefined symbol(s)\n", n);
}

static void MapNESintoROM(DataArea& area, const Object& obj)
{
    unsigned base = obj.GetSegmentBase();
    unsigned size = obj.GetSegmentSize();

    //fprintf(stderr, "base=%u, size=%u\n", base,size);
    std::multimap<unsigned, std::pair<unsigned,unsigned>> blobs; //For sorting
    while(size > 0)
    {
        unsigned gran       = 0x2000;
        unsigned base_begin = base - (base % gran);
        unsigned base_end   = base_begin + gran;

        unsigned write_to    = NES2ROMaddr(base);
        unsigned write_count = size;
        if(base + write_count > base_end) write_count = base_end - base;

        blobs.emplace(write_to, std::pair<unsigned,unsigned>(base, write_count));

        base += write_count;
        size -= write_count;
    }
    for(auto& b: blobs)
    {
        unsigned write_to    = b.first;
        unsigned base        = b.second.first;
        unsigned write_count = b.second.second;
        auto lump = obj.GetContent(base, write_count);
        if(std::any_of(lump.begin(), lump.end(), [&](unsigned n){return n!=0;}))
        {
            std::fprintf(stderr, "  base=$%X, size=$%X, write_to=$%X, write_count=$%X\n",
                base, size, write_to, write_count);

            area.WriteLump(write_to, lump);
        }
    }
}

static void FixupNES(Object& obj, std::FILE* stream)
{
    bool Mirroring  = true;
    bool Mirroring2 = false;
    bool Trainer = false;
    bool Battery = false;

    unsigned ROM_size  = ROMmap_npages;
    unsigned VROM_size = 0;
    unsigned ROM_type  = ((MapperNo & 0x0F) << 4)
                        | (Mirroring << 0)
                        | (Battery << 1)
                        | (Trainer << 2)
                        | (Mirroring2 << 3);
    unsigned ROM_type2 = (MapperNo & 0xF0);

    unsigned char NESheader[16] =
        {'N', 'E', 'S', 0x1A,
         (unsigned char)ROM_size,
         (unsigned char)VROM_size,
         (unsigned char)ROM_type,
         (unsigned char)ROM_type2,
         0,0,0,0,
         0,0,0,0};

    fseek(stream, 0, SEEK_SET);
    fwrite(NESheader, 1, 16, stream);

    DataArea rom_obj;
    obj.SelectTEXT();
    MapNESintoROM(rom_obj, obj);
    obj.SelectDATA();
    MapNESintoROM(rom_obj, obj);

    unsigned filesize = rom_obj.GetTop();
    unsigned RomSize = ROMmap_npages * GetPageSize();
    if(filesize < RomSize) filesize = RomSize;
    std::vector<unsigned char> ROMdata = rom_obj.GetContent(0, filesize);

    fseek(stream, 16, SEEK_SET);
    fwrite(&ROMdata[0], 1, ROMdata.size(), stream);

    ftruncate(fileno(stream), ROMdata.size()+16);
}

static void Import(O65linker& linker, Object& obj, SegmentSelection seg)
{
    std::vector<unsigned> o65addrs = linker.GetAddrList(seg);
    for(unsigned a=0; a<o65addrs.size(); ++a)
    {
        const vector<unsigned char>& code = linker.GetSeg(seg, a);
        if(code.empty()) continue;

        char Buf[64];
        std::sprintf(Buf, "object_%u_%s", a+1, GetSegmentName(seg).c_str());

        obj.DefineLabel(Buf, o65addrs[a]);

        obj.SetPos(o65addrs[a]);
        obj.AddLump(code);
    }
}

static void WriteOut(O65linker& linker, std::FILE* stream)
{
    Object obj;

    obj.StartScope();
     obj.SelectTEXT(); Import(linker, obj, CODE);
     obj.SelectDATA(); Import(linker, obj, DATA);
     obj.SelectZERO(); Import(linker, obj, ZERO);
     obj.SelectBSS(); Import(linker, obj, BSS);
     obj.SelectTEXT();
    obj.EndScope();

    switch(format)
    {
        case IPSformat:
            obj.WriteIPS(stream);
            break;
        case O65format:
            obj.WriteO65(stream);
            break;
        case RAWformat:
            obj.WriteRAW(stream, ROMmap_npages*GetPageSize());
            break;
        case NESformat:
            obj.WriteRAW(stream, ROMmap_npages*GetPageSize(), 16);
            FixupNES(obj, stream);
            break;
    }
    obj.Dump();
}

int main(int argc, char** argv)
{
    std::vector<std::string> files;

    std::FILE *output = NULL;
    std::string outfn;

    for(;;)
    {
        int option_index = 0;
        static struct option long_options[] =
        {
            {"help",     0,0,'h'},
            {"version",  0,0,'V'},
            {"output",   0,0,'o'},
            {"outformat", 0,0,'f'},
            {"romsize",  0,0,'s'},
            {0,0,0,0}
        };
        int c = getopt_long(argc,argv, "hVo:f:s:", long_options, &option_index);
        if(c==-1) break;
        switch(c)
        {
            case 'V': // version
            {
                std::printf(
                    "%s %s\n"
                    "Copyright (C) 1992,2006 Bisqwit (http://iki.fi/bisqwit/)\n"
                    "This is free software; see the source for copying conditions. There is NO\n"
                    "warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.\n",
                    argv[0], VERSION
                      );
                return 0;
            }

            case 'h':
            {
                std::printf(
                    "O65 linker (tuned for NES)\n"
                    "Copyright (C) 1992,2006 Bisqwit (http://iki.fi/bisqwit/)\n"
                    "\nUsage: %s [<option> [<...>]] <file> [<...>]\n"
                    "\nLinks O65 and IPS files together and produces a file.\n"
                    "\nOptions:\n"
                    " --help, -h            This help\n"
                    " --version, -V         Displays version information\n"
                    " -f, --outformat <fmt> Select output format: ips,raw,o65,nes (default: ips)\n"
                    " -o <file>             Places the output into <file>\n"
                    " -s <size>             Desired size of the ROM (must be a multiple of 16384)\n"
                    "\n"
                    "For the NES output format, currently only mapper-%u ROMs are supported with no VROM.\n"
                    "\nNo warranty whatsoever.\n"
                    ,
                    argv[0],
                    MapperNo);
                return 0;
            }

            case 'o':
            {
                outfn = optarg;
                if(outfn != "-")
                {
                    if(output)
                    {
                        std::fclose(output);
                    }
                    output = std::fopen(outfn.c_str(), "wb");
                    if(!output)
                    {
                        std::perror(outfn.c_str());
                        goto ErrorExit;
                    }
                }
                break;
            }
            case 'f':
            {
                SetOutputFormat(optarg);
                break;
            }
            case 's':
            {
                unsigned outsize = strtol(optarg, 0, 10);
                if(outsize % GetPageSize())
                {
                    std::fprintf(stderr, "Warning: The given ROMsize (%u, 0x%X) is not a multiple of 0x%X.\n"
                        "         Using %u (0x%X) instead.\n",
                        outsize, outsize,
                        GetPageSize(),
                        (outsize / GetPageSize())*GetPageSize(),
                        (outsize / GetPageSize())*GetPageSize()
                       );
                }

                ROMmap_npages = outsize / GetPageSize();
                std::fprintf(stderr, "%u pages.\n", ROMmap_npages);
                break;
            }
        }
    }

    while(optind < argc)
        files.push_back(argv[optind++]);

    if(files.empty())
    {
        std::fprintf(stderr, "Error: Link what? See %s --help\n", argv[0]);
    ErrorExit:
        if(output)
        {
            std::fclose(output);
            std::remove(outfn.c_str());
        }
        return -1;
    }


    O65linker linker;

    for(std::size_t a=0; a<files.size(); ++a)
    {
        char Buf[5];
        FILE *fp = std::fopen(files[a].c_str(), "rb");
        if(!fp)
        {
            std::perror(files[a].c_str());
            continue;
        }

        std::fread(Buf, 1, 5, fp);
        if(!std::strncmp(Buf, "PATCH", 5))
        {
            linker.LoadIPSfile(fp, files[a]);
        }
        else
        {
            O65 tmp;
            tmp.Load(fp);

            const vector<pair<unsigned char, string> >&
                customheaders = tmp.GetCustomHeaders();

            std::map<SegmentSelection,LinkageWish> Linkage;

            for(unsigned b=0; b<customheaders.size(); ++b)
            {
                unsigned char type = customheaders[b].first;
                const string& data = customheaders[b].second;
                switch(type)
                {
                    case 10: // linkage type
                    {
                        unsigned param = 0;
                        if(data.size() >= 5)
                        {
                            param = (data[1] & 0xFF)
                                  | ((data[2] & 0xFF) << 8)
                                  | ((data[3] & 0xFF) << 16)
                                  | ((data[4] & 0xFF) << 24);
                        }
                        unsigned seg = data[0] / 8, mode = data[0] & 7;
                        switch(mode)
                        {
                            case 0:
                                Linkage[SegmentSelection(seg)] = LinkageWish();
                                break;
                            case 1:
                                Linkage[SegmentSelection(seg)].SetLinkageGroup(param);
                                std::fprintf(stderr, "%s of %s will be linked in group %u\n",
                                     GetSegmentName(SegmentSelection(seg)).c_str(),
                                     files[a].c_str(), param);
                                break;
                            case 2:
                                unsigned addr = ROM2NESaddr(param*GetPageSize());
                                param = addr/GetPageSize();
                                Linkage[SegmentSelection(seg)].SetLinkagePage(param);
                                std::fprintf(stderr, "%s of %s will be linked in page starting at address $%05X\n",
                                    GetSegmentName(SegmentSelection(seg)).c_str(),
                                    files[a].c_str(), param*GetPageSize());
                                break;
                        }
                        break;
                    }
                    case 0: // filename
                    case 1: // operating system header
                    case 2: // assembler name
                    case 3: // author
                    case 4: // creation date
                        break;
                }
            }

            linker.AddObject(tmp, files[a], Linkage);
        }
        std::fclose(fp);
    }

    freespacemap freespace_code;
    // Assume everything is free space!
    /* FIXME: Make this configurable. */

    for(unsigned a=0; a<ROMmap_npages; ++a)
    {
        unsigned addr = ROM2NESaddr(a*GetPageSize());
        freespace_code.Add(addr/GetPageSize(), addr%GetPageSize(), GetPageSize());
    }
    for(unsigned a=0; a<ROMmap_npages; ++a)
    {
        unsigned addr = ROM2NESaddr(a*GetPageSize());
        freespace_code.DumpPageMap(addr/GetPageSize());
    }

    /* Organize the code blobs */
    freespace_code.OrganizeO65linker(linker, CODE);
    freespace_code.OrganizeO65linker(linker, DATA);

    for(unsigned a=0; a<ROMmap_npages; ++a)
    {
        unsigned addr = ROM2NESaddr(a*GetPageSize());
        freespace_code.DumpPageMap(addr/GetPageSize());
    }

    /* ZERO & BSS all refer to the RAM. */

    freespacemap freespace_data;

    /* First link the zeropage. It may only use 8-bit addresses. */
    freespace_data.Add(0x00, 0x0000, 0x100);
    freespace_data.OrganizeO65linker(linker, ZERO);

    /* 0x100..0x1FF is stack. Don't mark it as free space. */

    /* Then link BSS.
     * If 8-bit addresses remained free from the zeropage segment,
     * they may be used for data addresses.
     */

    freespace_data.Add(0x00, 0x0200, 0x800 - 0x200);
    freespace_data.OrganizeO65linker(linker, BSS);
    freespace_data.DumpPageMap(0);

    linker.Link();

    WriteOut(linker, output ? output : stdout);
    if(output) std::fclose(output);

    return 0;
}
