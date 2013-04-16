#include <cstdio>
#include <vector>
#include <string>
#include <map>
#include <cstring>

/* This program disassembles an IPS file. */

using namespace std;

#include "miscfun.hh"
#include "o65linker.hh"
#include "romaddr.hh"
#include "o65.hh"

static void DisAsm(unsigned origin, const unsigned char *data,
                   unsigned length,
                   const SegmentSelection curseg);

static
    std::map<SegmentSelection,
     std::multimap<unsigned, std::string> > Globals;

typedef Relocdata<unsigned> Re;
static Re Relocs;
static std::vector<std::string> RelocVarList;

static bool ROMAddressing = false;

static int HandleIPS()
{
    std::map<std::string, unsigned> RelocVarMap;
    
    Globals.clear();
    for(;;)
    {
        unsigned char Buf[2048];
        int wanted,c = fread(Buf, 1, 3, stdin);
        if(c < 0 && ferror(stdin)) { ipserr: perror("fread"); arf: return -1; }
        if(c < (wanted=3)) { ipseof:
            fprintf(stderr, "Unexpected end of file - wanted %d, got %d\n", wanted, c);
            goto arf; }
        if(!strncmp((const char *)Buf, "EOF", 3))break;
        unsigned pos = (((unsigned)Buf[0]) << 16)
                      |(((unsigned)Buf[1]) << 8)
                      | ((unsigned)Buf[2]);
        c = fread(Buf, 1, 2, stdin);
        if(c < 0 && ferror(stdin)) { fprintf(stderr, "Got pos %X\n", pos); goto ipserr; }
        if(c < (wanted=2)) { goto ipseof; }
        unsigned len = (((unsigned)Buf[0]) << 8)
                     | ((unsigned)Buf[1]);
        
        bool rle=false;
        if(!len)
        {
            rle=true;
            c = fread(Buf, 1, 2, stdin);
            if(c < 0 && ferror(stdin)) { fprintf(stderr, "Got pos %X\n", pos); goto ipserr; }
            if(c < (wanted=2)) { goto ipseof; }
            len = (((unsigned)Buf[0]) << 8)
                 | ((unsigned)Buf[1]);
        }
        
        vector<unsigned char> Buf2(len);
        if(rle)
        {
            c = fread(&Buf2[0], 1, 1, stdin);
            if(c < 0 && ferror(stdin)) { goto ipserr; }
            if(c != (wanted=(int)1)) { goto ipseof; }
            for(unsigned c=1; c<len; ++c)
                Buf2[c] = Buf2[0];
        }
        else
        {
            c = fread(&Buf2[0], 1, len, stdin);
            if(c < 0 && ferror(stdin)) { goto ipserr; }
            if(c != (wanted=(int)len)) { goto ipseof; }
        }
        
        switch(pos)
        {
            case IPS_ADDRESS_GLOBAL:
            {
                std::string name((const char *)&Buf2[0], Buf2.size());
                name = name.c_str();
                unsigned addr = Buf2[name.size()+1]
                             | (Buf2[name.size()+2] << 8)
                             | (Buf2[name.size()+3] << 16);
                
                unsigned trans_addr = addr;
                
                printf(".global %s\t;$%06X\n", name.c_str(), trans_addr);
                
                Globals[CODE].insert(make_pair(addr, name));
                break;
            }
            case IPS_ADDRESS_EXTERN:
            {
                std::string name((const char *)&Buf2[0], Buf2.size());
                name = name.c_str();
                unsigned addr = Buf2[name.size()+1]
                             | (Buf2[name.size()+2] << 8)
                             | (Buf2[name.size()+3] << 16);
                unsigned size = Buf2[name.size()+4];
                
                unsigned var_id = 0;
                std::map<std::string,unsigned>::iterator i = RelocVarMap.find(name);
                if(i == RelocVarMap.end())
                {
                    RelocVarMap[name] = var_id = RelocVarList.size();
                    RelocVarList.push_back(name);
                    printf(".extern %s\n", name.c_str());
                }
                else
                    var_id = i->second;
                
                switch(size)
                {
                    case 1:
                        Relocs.R16lo.Relocs.push_back(std::make_pair(addr,var_id));
                        break;
                    case 2:
                        Relocs.R16.Relocs.push_back(std::make_pair(addr,var_id));
                        break;
                    case 3:
                        Relocs.R24.Relocs.push_back(std::make_pair(addr,var_id));
                        break;
                    default:
                        printf("; Extern '%s' goes to $%06X (%u bytes)\n",
                            name.c_str(), addr, size);
                }
                break;
            }
            default:
            {
                Relocs.sort();
                ROMAddressing = false;
                DisAsm(pos, &Buf2[0], Buf2.size(), CODE);
            }
        }
        printf("---\n");
    }
    return 0;
}
static int HandleFDS(int num_sides, bool headered)
{
    // Skip the remainder of FDS header
    if(headered)
        { char Buf[16-5]; fread(Buf, 1, sizeof(Buf), stdin); }
    else
        fseek(stdin, -5, SEEK_CUR);

    for(int side = 0; side < num_sides; ++side)
    {
        unsigned char Buf[65500] = { 0 };
        fread(Buf, 1, sizeof(Buf), stdin);

        unsigned length       = 1;
        unsigned base_address = 0;
        unsigned datatype     = 0;
        for(unsigned ptr = 0; ptr < sizeof(Buf); )
            switch(Buf[ptr])
            {
                case 1:
                    printf(".fds_diskinfo '%.14s',$%02X,'%.4s',$%02X,$%02X, $%02X,$%02X,$%02X, $%02X, $%02X,$%02X,$%02X, $%02X,$%02X,$%02X\n",
                        &Buf[ptr+1],
                        Buf[ptr+15],
                        &Buf[ptr+16],
                        Buf[ptr+20], Buf[ptr+21], // game version, side number
                        Buf[ptr+22], Buf[ptr+23], Buf[ptr+24], Buf[ptr+25],
                        Buf[ptr+31],Buf[ptr+32],Buf[ptr+33],
                        Buf[ptr+44],Buf[ptr+45],Buf[ptr+46]
                    );
                    ptr += 56;
                    break;
                case 2:
                    printf(".fds_numfiles %u\n", Buf[ptr+1]);
                    ptr += 2;
                    break;
                case 3:
                    base_address = Buf[ptr+11] + 0x100*Buf[ptr+12];
                    length       = Buf[ptr+13] + 0x100*Buf[ptr+14];
                    printf(".fds_file $%02X,$%02X,'%.8s', $%04X, %u, $%02X ;Ends at $%04X\n",
                        Buf[ptr+1], Buf[ptr+2],  &Buf[ptr+3],
                        base_address, length,   datatype = Buf[ptr+15],
                        base_address+length);
                    ptr += 16;
                    break;
                case 4:
                    if(datatype == 0)
                        DisAsm(base_address, &Buf[ptr+1], length, CODE);
                    else
                    {
                        printf(".data ... FIXME\n");
                    }
                    ptr += length+1;
                    break;
                case 0:
                    ++ptr;
                    break;
                default:
                    printf(".byte $%02X\n", Buf[ptr++]);
            }
        printf("---\n");
    }
    return 0;
}

static void LoadO65globals(const O65& o, SegmentSelection seg)
{
    std::multimap<unsigned, std::string>& glob = Globals[seg];
    
    std::vector<std::string> syms = o.GetSymbolList(seg);
    for(unsigned a=0; a<syms.size(); ++a)
    {
        const std::string& name = syms[a];
        unsigned addr = o.GetSymAddress(seg, name);
        glob.insert(make_pair(addr, name));
    }
}
static void DumpO65globals(const O65& o, SegmentSelection seg)
{
    const std::multimap<unsigned, std::string>& glob = Globals[seg];
        
    for(std::multimap<unsigned, std::string>::const_iterator
        i = glob.begin(); i != glob.end(); ++i)
    {
        const std::string& name = i->second;
        unsigned addr = i->first;
        printf(".global %s\t;$%06X\n", name.c_str(), addr);
    }
}

static unsigned FindNextFixup(unsigned address, unsigned& length);
static void O65dumpSeg(const O65& o65, SegmentSelection seg, const char* asmheader)
{
    printf("%s\n", asmheader);
    Relocs = o65.GetRelocData(seg);
    Relocs.sort();

    DumpO65globals(o65, seg);
    DisAsm(o65.GetBase(seg), &*o65.GetSeg(seg).begin(), o65.GetSegSize(seg), seg);
}

static int HandleO65()
{
    O65 o65;
    o65.Load(stdin);

    RelocVarList = o65.GetExternList();
    for(unsigned a=0; a<RelocVarList.size(); ++a)
        printf(".extern %s\n", RelocVarList[a].c_str());
    
    Globals.clear();
    LoadO65globals(o65, CODE);
    LoadO65globals(o65, DATA);
    LoadO65globals(o65, ZERO);
    LoadO65globals(o65, BSS);
    
    O65dumpSeg(o65, CODE, ".code");
    O65dumpSeg(o65, DATA, ".data");
    O65dumpSeg(o65, ZERO, ".zero");
    O65dumpSeg(o65, BSS, ".bss");
    
    ROMAddressing = false;

    return 0;
}


int main(int argc, const char *const *argv)
{
    if(argc == 2)
    {
        FILE *fp = freopen(argv[1], "rb", stdin);
        if(!fp) { perror(argv[1]); return -1; }
    }
    else
    {
        fprintf(stderr,
            "NES disassembler\n"
            "Copyright (C) 1992,2006 Bisqwit (http://iki.fi/bisqwit/)\n"
            "\n"
            "Usage: disasm [<filename>]\n"
            "If you don't give filename, stdin is assumed.\n"
            "IPS, O65 or raw formats are allowed.\n"
        );
    }
    vector<unsigned char> code;
    bool first=true;
    for(;;)
    {
        char Buf[2048];
        int c = first ? 5 : sizeof Buf;
        c = fread(Buf, 1, c, stdin);
        if(c <= 0) break;
        
        if(first)
        {
            if(!strncmp(Buf, "PATCH", 5))
                return HandleIPS();
            if(!strncmp(Buf+2, "o65", 3))
                return HandleO65();
            if(!strncmp(Buf, "FDS\x1A", 4))
                return HandleFDS(Buf[4], true);
            if(!strncmp(Buf, "\1*NIN", 5))
                return HandleFDS(2, false);
            first = false;
        }
        
        code.insert(code.end(), Buf, Buf+c);
    }
    unsigned origin = 0x0000;
    
    printf("Code:\n");
    ROMAddressing = true;
    DisAsm(origin, &code[0], code.size(), CODE);
    return 0;
}

struct addrmode
{
    const char* format;
    const char* params;
};


static unsigned FixCodeAddr(unsigned a)
{
    if(ROMAddressing) return ROM2NESaddr(a);
    return a;
}

static std::string FindFixupAnchor
    (const SegmentSelection seg, unsigned address, bool use_negative=true,
     bool suffixing=true)
{
    const std::multimap<unsigned, std::string>& glob = Globals[seg];
    
    typedef std::multimap<unsigned, std::string>::const_iterator git;
    
    git i = glob.lower_bound(address);
    while(i != glob.begin()
       && (i == glob.end() || i->first > address)) --i;
    
    if(i == glob.end()) return "";

    int diff = address - i->first;
    
    if(use_negative)
    {
        // Try next label, if it gives a shorter offset
        git j = i; ++j;
        if(j != glob.end())
        {
            int diff2 = address - j->first;
            if(std::abs(diff2) < std::abs(diff)) { i = j; diff = diff2; }
        }
    }
    
    std::string fixup = i->second;
    if(diff)
    {
        fixup += format("%+d", diff);
    }
    
    if(!suffixing) return fixup;
    return "\t<" + fixup + ">";
}

static std::string DumpInt3(unsigned address, const unsigned char* data)
{
    unsigned param = (data[2] << 16) | (data[1] << 8) | data[0];
    
    std::string fix = "";
    
    /* Could use binary searching here */

    const Re::R24_t& R24 = Relocs.R24;
    for(unsigned a=0; a < R24.Relocs.size(); ++a)
    {
        const Re::R24_t::RelocType& re = R24.Relocs[a];
        if(re.first == address)
        {
            std::string ext = RelocVarList[re.second];
            //printf("; RELOC %s USED\n", ext.c_str());
            if(param) ext += format("+$%06X", param);
            return "@" + ext;
        }
    }
    for(unsigned a=0; a < R24.Fixups.size(); ++a)
    {
        const Re::R24_t::FixupType& re = R24.Fixups[a];
        if(re.second == address)
            { fix=FindFixupAnchor(re.first, param); goto End; }
    }
End:
    return format("@$%06X", param)+fix;
}

static std::string DumpInt2(unsigned address, const unsigned char* data, bool is_code=false)
{
    unsigned param = (data[1] << 8) | data[0];
    
    std::string fix = "";
    
    /* Could use binary searching here */

    const Re::R16_t& R16 = Relocs.R16;
    for(unsigned a=0; a < R16.Relocs.size(); ++a)
    {
        const Re::R16_t::RelocType& re = R16.Relocs[a];
        if(re.first == address)
        {
            std::string ext = RelocVarList[re.second];
            //printf("; RELOC %s USED\n", ext.c_str());
            if(param) ext += format("%+d", (signed short)param);
            return "!" + ext;
        }
    }
    for(unsigned a=0; a < R16.Fixups.size(); ++a)
    {
        const Re::R16_t::FixupType& re = R16.Fixups[a];
        if(re.second == address)
            return "!" + FindFixupAnchor(re.first, param, true, false);
    }
End:
    if(is_code) return format("$%04X", FixCodeAddr(param))+fix;
    return format("$%04X", param)+fix;
}
static std::string DumpInt1(unsigned address, const unsigned char* data)
{
    unsigned param = *data;
    
    std::string fix = "";
    
    /* Could use binary searching here */

    const Re::R16lo_t& R16lo = Relocs.R16lo;
    for(unsigned a=0; a < R16lo.Relocs.size(); ++a)
    {
        const Re::R16lo_t::RelocType& re = R16lo.Relocs[a];
        if(re.first == address)
        {
            std::string ext = RelocVarList[re.second];
            //printf("; RELOC %s USED\n", ext.c_str());
            if(param) ext += format("+%u", param);
            return "<" + ext;
        }
    }

    const Re::R16hi_t& R16hi = Relocs.R16hi;
    for(unsigned a=0; a < R16hi.Relocs.size(); ++a)
    {
        const Re::R16hi_t::RelocType& re = R16hi.Relocs[a];
        if(re.first.first == address)
        {
            unsigned offspart = re.first.second;
            std::string ext = RelocVarList[re.second];
            //printf("; RELOC %s USED\n", ext.c_str());
            if(offspart) ext = "(" + ext + format("+%u)", offspart);
            if(param) ext = "(" + ext + format(" + $%02X)", param << 8);
            return ">" + ext;
        }
    }

    const Re::R24seg_t& R24seg = Relocs.R24seg;
    for(unsigned a=0; a < R24seg.Relocs.size(); ++a)
    {
        const Re::R24seg_t::RelocType& re = R24seg.Relocs[a];
        if(re.first.first == address)
        {
            unsigned offspart = re.first.second;
            std::string ext = RelocVarList[re.second];
            //printf("; RELOC %s USED\n", ext.c_str());
            if(offspart) ext = "(" + ext + format("+%u)", offspart);
            if(param) ext = "(" + ext + format(" + $%02X)", param << 16);
            return "^" + ext;
        }
    }
    for(unsigned a=0; a < R16hi.Fixups.size(); ++a)
    {
        const Re::R16hi_t::FixupType& re = R16hi.Fixups[a];
        if(re.second.first == address)
            return ">" + FindFixupAnchor(re.first, (param << 8) + re.second.second, true, false);
    }
    for(unsigned a=0; a < R16lo.Fixups.size(); ++a)
    {
        const Re::R16lo_t::FixupType& re = R16lo.Fixups[a];
        if(re.second == address)
            return "<" + FindFixupAnchor(re.first, param, true, false);
    }
    for(unsigned a=0; a < R24seg.Fixups.size(); ++a)
    {
        const Re::R24seg_t::FixupType& re = R24seg.Fixups[a];
        if(re.second.first == address)
            return "^" + FindFixupAnchor(re.first, (param << 16) + re.second.second, true, false);
    }

    return format("$%02X", param) + fix;
}

static unsigned DumpIns(const unsigned address,
                        const std::string& op,
                        const addrmode& mode,
                        const unsigned char* data,
                        int opcodebytes,
                        const SegmentSelection curseg)
{
    std::string Buf;
    
    const char*p = mode.params;
    unsigned size=opcodebytes;
    for(; p&&*p; )
    {
        char c = *p++;
        switch(c)
        {
            case 'r':
                { signed char n=data[size];
                  std::string fix = FindFixupAnchor(curseg, address+n+2, false);
                  Buf += format("$%06X", FixCodeAddr(address+n+2)) + fix;
                  size+=1;
                  break;
                }
            case 'R':
                { signed short n=data[size]+data[size+1]*256;
                  std::string fix = FindFixupAnchor(curseg, address+n+3, false);
                  Buf += format("$%06X", FixCodeAddr(address+n+3)) + fix;
                  size+=2;
                  break;
                }
            case '3': Buf += DumpInt3(address+size, data+size); size += 3; break;
            case '2':
            {
                bool is_code = op == "jmp" || op == "jsr"
                           || op == "bcc" || op == "bcs"
                           || op == "beq" || op == "bpl"
                           || op == "bvc" || op == "bvs"
                           || op ==" bmi" || op == "bne";
                Buf += DumpInt2(address+size, data+size, is_code);
                size += 2;
                break;
            }
            case '1': Buf += DumpInt1(address+size, data+size); size += 1; break;
        }
        if(*p) Buf += ", ";
    }
    
    printf(" %06X\t", FixCodeAddr(address));
    
    for(unsigned n=0; n<4; ++n)
        printf(n<size?"%02X ":"   ", data[n]);
    if(!op.empty()) printf("%s ", op.c_str());
    printf(mode.format, Buf.c_str());
    printf("\n");
    return size;
}

static unsigned CalcSize(const addrmode& mode)
{
    unsigned size=1;
    const char*p = mode.params;
    while(p&&*p)
        switch(*p++)
        {
            case 'r': size += 1; break;
            case 'R': size += 2; break;
            case '1': size += 1; break;
            case '2': size += 2; break;
            case '3': size += 3; break;
        }
    return size;    
}

static unsigned FindNextLabel(SegmentSelection seg, unsigned address)
{
    const std::multimap<unsigned, std::string>& glob = Globals[seg];
        
    std::multimap<unsigned,std::string>::const_iterator
      gi = glob.lower_bound(address);
    if(gi != glob.end()) return gi->first;
    return address + 0x100;
}

static unsigned FindNextFixup(unsigned address, unsigned& length,
                              const SegmentSelection& seg)
{
    unsigned candidate = (unsigned)(-1);
    length = 1;
    #define Try(l,u) \
        { if((u >= address && u < candidate) || (u==candidate && l >= length)) \
            { candidate=u; length=l; } }
    
    const Re::R16_t& R16 = Relocs.R16;
    const Re::R16lo_t& R16lo = Relocs.R16lo;
    const Re::R16hi_t& R16hi = Relocs.R16hi;
    const Re::R24seg_t& R24seg = Relocs.R24seg;
    const Re::R24_t& R24 = Relocs.R24;
    
    /* TODO: This could be made a lot faster by using binary search. */

    for(unsigned a=0; a < R16.Relocs.size(); ++a) Try(2,R16.Relocs[a].first);
    for(unsigned a=0; a < R16lo.Relocs.size(); ++a) Try(1,R16lo.Relocs[a].first);
    for(unsigned a=0; a < R16hi.Relocs.size(); ++a) Try(1,R16hi.Relocs[a].first.first);
    for(unsigned a=0; a < R24seg.Relocs.size(); ++a) Try(1,R24seg.Relocs[a].first.first);
    for(unsigned a=0; a < R24.Relocs.size(); ++a) Try(3,R24.Relocs[a].first);
    
    for(unsigned a=0; a < R16.Fixups.size(); ++a)    if(R16.Fixups[a].first==seg) Try(2,R16.Fixups[a].second);
    for(unsigned a=0; a < R16lo.Fixups.size(); ++a)  if(R16lo.Fixups[a].first==seg) Try(1,R16lo.Fixups[a].second);
    for(unsigned a=0; a < R16hi.Fixups.size(); ++a)  if(R16hi.Fixups[a].first==seg) Try(1,R16hi.Fixups[a].second.first);
    for(unsigned a=0; a < R24seg.Fixups.size(); ++a) if(R24seg.Fixups[a].first==seg) Try(1,R24seg.Fixups[a].second.first);
    for(unsigned a=0; a < R24.Fixups.size(); ++a)    if(R24.Fixups[a].first==seg) Try(3,R24.Fixups[a].second);
    
    #undef Try
    return candidate;
}

/* Bisqwit's humble little nes-disassembler. */
static void DisAsm(unsigned origin, const unsigned char *data,
                   unsigned length,
                   const SegmentSelection curseg)
{
    std::multimap<unsigned, std::string>& glob = Globals[curseg];
        
    if(glob.find(origin) == glob.end())
    {
        glob.insert(make_pair(origin, format("Lbl_%06X", origin)));
    }
    
    unsigned remain = length;
    for(unsigned size,address=origin; remain>0;
        address+=size,remain-=size,data+=size)
    {
        static const addrmode addrmodes[12] =
        {
            {""        ,""},
            {"#%s"     ,"1"},
            {"%s"      ,"r"},
            {"%s"      ,"1"},
            {"%s,x"    ,"1"},
            {"%s,y"    ,"1"},
            {"(%s,x)"  ,"1"},
            {"(%s),y"  ,"1"},
            {"%s"      ,"2"},
            {"%s,x"    ,"2"},
            {"%s,y"    ,"2"},
            {"(%s)"    ,"2"},
        };
        static const addrmode bytemode = {"%s", "1"};
        static const addrmode wordmode = {"%s", "2"};
        static const addrmode longmode = {"%s", "3"};
        static const char info[] =
 // addressing modes
 "AGZZZDDZABAZZIIZ" "CHZZZEEZAKZZZJJZ" "IGZZDDDZABAZIIIZ" "CHZZZEEZAKZZZJJZ"
 "AGZZZDDZABAZIIIZ" "CHZZZEEZAKZZZJJZ" "AGZZZDDZABAZLIIZ" "CHZZZEEZAKZZZJJZ"
 "ZGZZDDDZAZAZIIIZ" "CHZZEEFZAKAZZJZZ" "BGBZDDDZABAZIIIZ" "CHZZEEFZAKAZJJKZ"
 "BGZZDDDZABAZIIIZ" "CHZZZEEZAKZZZJJZ" "BGZZDDDZABAZIIIZ" "CHZZZEEZAKZZZJJZ"
 // opcodes
 "brkora" "??????" "???ora" "asl???" "phpora" "asl???" "???ora" "asl???"
 "bplora" "??????" "???ora" "asl???" "clcora" "??????" "???ora" "asl???"
 "jsrand" "??????" "bitand" "rol???" "plpand" "rol???" "bitand" "rol???"
 "bmiand" "??????" "???and" "rol???" "secand" "??????" "???and" "rol???"
 "rtieor" "??????" "???eor" "lsr???" "phaeor" "lsr???" "jmpeor" "lsr???"
 "bvceor" "??????" "???eor" "lsr???" "clieor" "??????" "???eor" "lsr???"
 "rtsadc" "??????" "???adc" "ror???" "plaadc" "ror???" "jmpadc" "ror???"
 "bvsadc" "??????" "???adc" "ror???" "seiadc" "??????" "???adc" "ror???"
 "???sta" "??????" "stysta" "stx???" "dey???" "txa???" "stysta" "stx???"
 "bccsta" "??????" "stysta" "stx???" "tyasta" "txs???" "???sta" "??????"
 "ldylda" "ldx???" "ldylda" "ldx???" "taylda" "tax???" "ldylda" "ldx???"
 "bcslda" "??????" "ldylda" "ldx???" "clvlda" "tsx???" "ldylda" "ldx???"
 "cpycmp" "??????" "cpycmp" "dec???" "inycmp" "dex???" "cpycmp" "dec???"
 "bnecmp" "??????" "???cmp" "dec???" "cldcmp" "??????" "???cmp" "dec???"
 "cpxsbc" "??????" "cpxsbc" "inc???" "inxsbc" "nop???" "cpxsbc" "inc???"
 "beqsbc" "??????" "???sbc" "inc???" "sedsbc" "??????" "???sbc" "inc???";
        unsigned mode=info[*data]-'A'; // addressing mode
        
        if(mode < 12)
            size = CalcSize(addrmodes[mode]);
        else
            size = 1;
        
        unsigned opcode_end = address+size;
        
        std::multimap<unsigned,std::string>::const_iterator gi;
        gi = glob.lower_bound(address);
        for(; gi != glob.end() && gi->first == address; ++gi)
        {
            printf("%s:\n", gi->second.c_str());
        }
        
        unsigned remain_until = remain;
        
        /* If there's a label in the middle of an instruction,
         * remain_until could be shorter.
         */
        unsigned next_label      = FindNextLabel(curseg, address+1);
        
        /* Avoid an opcode spanning over a label */
        if(next_label < opcode_end)
        {
            unsigned until_label = next_label - address;
            if(until_label < remain_until) remain_until = until_label;
        }

        /* Avoid an opcode spanning over a reloc/fixup */

        unsigned estimate_length = 1;
        unsigned next_fixup = FindNextFixup(address, estimate_length, curseg);
        //printf("Next fixup at %X (%u)\n", next_fixup, estimate_length);
        if(next_fixup == address)
        {
            /* Then opcode begins with a fixup. Can't be disassembled. */
            remain_until = estimate_length;
            goto DoRaw;
        }
        
        if(next_fixup < address
        && (next_fixup != address+1 || next_fixup+estimate_length != opcode_end)
          )
        {
            /* Then opcode contains a fixup that doesn't behave nicely
             * with the address. Can't be disassembled (would require math).
             */
            unsigned until_fixup = next_fixup - address;
            if(until_fixup < remain_until) remain_until = until_fixup;
            goto DoRaw;
        }
        if(mode >= 12) { goto onebyte; }
        
        if(data[0] == 0xA9 && data[1] == 0x3A
        && data[2] == 0x20 && data[3] == 0x51 && data[4] == 0xC0)
        {
            printf("MARK\n");
        }
        
        /* If the instruction is too big for this position, create
         * a pseudo-instruction instead.
         */
        if(size > remain_until)
            DoRaw: switch(remain_until)
            {
                onebyte:
                case 1: size = DumpIns(address, ".byte", bytemode, data,0, curseg); continue;
                case 2: size = DumpIns(address, ".word", wordmode, data,0, curseg); continue;
                case 3: size = DumpIns(address, ".long", longmode, data,0, curseg); continue;
            }
        size = DumpIns(address,
                       std::string(info + 256 + *data*3, 3), addrmodes[mode], data,1, curseg);
        if(remain <= size)break;
    }
}
