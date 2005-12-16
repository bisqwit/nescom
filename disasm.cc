#include <cstdio>
#include <vector>
#include <string>
#include <map>

/* This program disassembles an IPS file. */

using namespace std;

#include "miscfun.hh"
#include "o65linker.hh"
#include "romaddr.hh"
#include "o65.hh"

static void DisAsm(unsigned origin, const unsigned char *data,
                   unsigned length);

static std::multimap<unsigned, std::string> Globals;

typedef Relocdata<unsigned> Re;
static Re Relocs;
static std::vector<std::string> RelocVarList;

static bool IPSmode = false;

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
                
                addr = ROM2NESaddr(addr);
                
                printf(".global %s\t;$%06X\n", name.c_str(), addr);
                
                Globals.insert(make_pair(addr, name));
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
                
                addr = ROM2NESaddr(addr);
                
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
                IPSmode = true;
                DisAsm(pos, &Buf2[0], Buf2.size());
            }
        }
        printf("---\n");
    }
    return 0;
}
static void LoadO65globals(const O65& o, SegmentSelection seg)
{
    Globals.clear();
    std::vector<std::string> syms = o.GetSymbolList(seg);
    for(unsigned a=0; a<syms.size(); ++a)
    {
        const std::string& name = syms[a];
        unsigned addr = o.GetSymAddress(seg, name);
        Globals.insert(make_pair(addr, name));
        printf(".global %s\t;$%06X\n", name.c_str(), addr);
    }
}

static unsigned FindNextFixup(unsigned address, unsigned& length);
static void O65dumpSeg(const O65& o65, SegmentSelection seg, const char* asmheader)
{
    printf("%s\n", asmheader);
    Relocs = o65.GetRelocData(seg);
    Relocs.sort();

    LoadO65globals(o65, seg);
    DisAsm(o65.GetBase(seg), &*o65.GetSeg(seg).begin(), o65.GetSegSize(seg));
}

static int HandleO65()
{
    O65 o65;
    o65.Load(stdin);

    RelocVarList = o65.GetExternList();
    for(unsigned a=0; a<RelocVarList.size(); ++a)
        printf(".extern %s\n", RelocVarList[a].c_str());
    
    O65dumpSeg(o65, CODE, ".code");
    O65dumpSeg(o65, DATA, ".data");
    O65dumpSeg(o65, ZERO, ".zero");
    O65dumpSeg(o65, BSS, ".bss");

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
            "Copyright (C) 1992,2005 Bisqwit (http://iki.fi/bisqwit/)\n"
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
            first = false;
        }
        
        code.insert(code.end(), Buf, Buf+c);
    }
    unsigned origin = 0;
    
    printf("Code:\n");
    DisAsm(origin, &code[0], code.size());
    return 0;
}

struct addrmode
{
    const char* format;
    const char* params;
};

static std::string FindFixupAnchor(unsigned address, bool use_negative=true)
{
    typedef std::multimap<unsigned, std::string>::iterator git;
    git i = Globals.lower_bound(address);
    while(i != Globals.begin()
       && (i == Globals.end() || i->first > address)) --i;
    
    if(i == Globals.end()) return "";

    int diff = address - i->first;
    
    if(use_negative)
    {
        // Try next label, if it gives a shorter offset
        git j = i; ++j;
        if(j != Globals.end())
        {
            int diff2 = address - j->first;
            if(std::abs(diff2) < std::abs(diff)) { i = j; diff = diff2; }
        }
    }
    
    std::string fixup = i->second;
    if(diff)
        fixup += format("%+d", diff);
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
            { fix=FindFixupAnchor(param); goto End; }
    }
End:
    return format("@$%06X", param)+fix;
}

static std::string DumpInt2(unsigned address, const unsigned char* data)
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
            { fix=FindFixupAnchor(param); goto End; }
    }
End:
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
            return ext;
        }
    }
    for(unsigned a=0; a < R16lo.Fixups.size(); ++a)
    {
        const Re::R16lo_t::FixupType& re = R16lo.Fixups[a];
        if(re.second == address)
            return "<  " + FindFixupAnchor(param);
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
    for(unsigned a=0; a < R16hi.Fixups.size(); ++a)
    {
        const Re::R16hi_t::FixupType& re = R16hi.Fixups[a];
        if(re.second.first == address)
            return ">  " + FindFixupAnchor((param << 8) + re.second.second);
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
    for(unsigned a=0; a < R24seg.Fixups.size(); ++a)
    {
        const Re::R24seg_t::FixupType& re = R24seg.Fixups[a];
        if(re.second.first == address)
            return "^  " + FindFixupAnchor((param << 16) + re.second.second);
    }

    return format("$%02X", param) + fix;
}

static unsigned DumpIns(const unsigned address,
                        const std::string& op,
                        const addrmode& mode,
                        const unsigned char* data,
                        int opcodebytes)
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
                  std::string fix = FindFixupAnchor(address+n+2, false);
                  Buf += format("$%06X", address+n+2) + fix;
                  size+=1;
                  break;
                }
            case 'R':
                { signed short n=data[size]+data[size+1]*256;
                  std::string fix = FindFixupAnchor(address+n+3, false);
                  Buf += format("$%06X", address+n+3) + fix;
                  size+=2;
                  break;
                }
            case '3': Buf += DumpInt3(address+size, data+size); size += 3; break;
            case '2': Buf += DumpInt2(address+size, data+size); size += 2; break;
            case '1': Buf += DumpInt1(address+size, data+size); size += 1; break;
        }
        if(*p) Buf += ", ";
    }
    
    unsigned a = address;

    if(IPSmode) a = ROM2NESaddr(a);
    printf(" %06X\t", a);
    
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

static unsigned FindNextLabel(unsigned address)
{
    std::multimap<unsigned,std::string>::const_iterator
      gi = Globals.lower_bound(address);
    if(gi != Globals.end()) return gi->first;
    return address + 0x100;
}

static unsigned FindNextFixup(unsigned address, unsigned& length)
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
    
    for(unsigned a=0; a < R16.Fixups.size(); ++a) Try(2,R16.Fixups[a].second);
    for(unsigned a=0; a < R16lo.Fixups.size(); ++a) Try(1,R16lo.Fixups[a].second);
    for(unsigned a=0; a < R16hi.Fixups.size(); ++a) Try(1,R16hi.Fixups[a].second.first);
    for(unsigned a=0; a < R24seg.Fixups.size(); ++a) Try(1,R24seg.Fixups[a].second.first);
    for(unsigned a=0; a < R24.Fixups.size(); ++a) Try(3,R24.Fixups[a].second);
    
    #undef Try
    return candidate;
}

/* Bisqwit's humble little snes-disassembler. */
static void DisAsm(unsigned origin, const unsigned char *data,
                   unsigned length)
{
    if(Globals.find(origin) == Globals.end())
    {
        Globals.insert(make_pair(origin, format("Lbl_%06X", origin)));
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
        gi = Globals.lower_bound(address);
        for(; gi != Globals.end() && gi->first == address; ++gi)
        {
            printf("%s:\n", gi->second.c_str());
        }
        
        unsigned remain_until = remain;
        
        /* If there's a label in the middle of an instruction,
         * remain_until could be shorter.
         */
        unsigned next_label      = FindNextLabel(address+1);
        
        /* Avoid an opcode spanning over a label */
        if(next_label < opcode_end)
        {
            unsigned until_label = next_label - address;
            if(until_label < remain_until) remain_until = until_label;
        }

        /* Avoid an opcode spanning over a reloc/fixup */

        unsigned estimate_length = 1;
        unsigned next_fixup = FindNextFixup(address, estimate_length);
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
        
        /* If the instruction is too big for this position, create
         * a pseudo-instruction instead.
         */
        if(size > remain_until)
            DoRaw: switch(remain_until)
            {
                onebyte:
                case 1: size = DumpIns(address, ".byte", bytemode, data,0); continue;
                case 2: size = DumpIns(address, ".word", wordmode, data,0); continue;
                case 3: size = DumpIns(address, ".long", longmode, data,0); continue;
            }
        size = DumpIns(address,
                       std::string(info + 256 + *data*3, 3), addrmodes[mode], data,1);
        if(remain <= size)break;
    }
}
