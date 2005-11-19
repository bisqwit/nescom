#include <cstdio>
#include <vector>
#include <string>

/* This program disassembles an IPS file. */

using namespace std;

#include "o65linker.hh"
#include "romaddr.hh"
#include "o65.hh"

static void DisAsm(unsigned origin, const unsigned char *data,
                   unsigned length);

static Relocdata<unsigned> Relocs;

static int HandleIPS()
{
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
        
        //fprintf(stderr, "%06X <- %-5u ", pos, len);
        //if(++col == 5) { fprintf(stderr, "\n"); col=0; }
        
        vector<unsigned char> Buf2(len);
        c = fread(&Buf2[0], 1, len, stdin);
        if(c < 0 && ferror(stdin)) { goto ipserr; }
        if(c != (wanted=(int)len)) { goto ipseof; }
        
        switch(pos)
        {
            case IPS_ADDRESS_GLOBAL:
            {
                std::string name((const char *)&Buf2[0], Buf2.size());
                name = name.c_str();
                unsigned addr = Buf2[name.size()+1]
                             | (Buf2[name.size()+2] << 8)
                             | (Buf2[name.size()+3] << 16);
                
                addr = ROM2SNESaddr(addr);
                
                printf("Global $%06X is '%s'\n", addr | 0xC00000, name.c_str());
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
                
                addr = ROM2SNESaddr(addr);
                
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
                        printf("Extern '%s' goes to $%06X (%u bytes)\n",
                            name.c_str(), addr | 0xC00000, size);
                }
                break;
            }
            default:
            {
                DisAsm(ROM2SNESaddr(pos), &Buf2[0], Buf2.size());
            }
        }
        printf("---\n");
    }
    Relocs.clear();
    return 0;
}
static int HandleO65()
{
    O65 o65;
    o65.Load(stdin);

    printf(".code\n");
    Relocs = o65.GetRelocData(CODE);
    DisAsm(o65.GetBase(CODE), &*o65.GetSeg(CODE).begin(), o65.GetSegSize(CODE));
    printf(".data\n");
    Relocs = o65.GetRelocData(DATA);
    DisAsm(o65.GetBase(DATA), &*o65.GetSeg(DATA).begin(), o65.GetSegSize(DATA));
    printf(".zero\n");
    Relocs = o65.GetRelocData(ZERO);
    DisAsm(o65.GetBase(ZERO), &*o65.GetSeg(ZERO).begin(), o65.GetSegSize(ZERO));
    printf(".bss\n");
    Relocs = o65.GetRelocData(BSS);
    DisAsm(o65.GetBase(BSS),  &*o65.GetSeg(BSS).begin(), o65.GetSegSize(BSS));
    Relocs.clear();
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
            "Usage: disasm [<filename.ips>]\n"
            "If you don't give filename, stdin is assumed.\n");
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
            if(!strncmp(Buf+2, "o65", 4))
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

static int FulFillable(const addrmode& mode, const unsigned char* data)
{
    
}
static unsigned DumpIns(unsigned address,const bool X,const bool Y,
                        const std::string& op,
                        const unsigned char* data)
{
    printf(" %06X\t", address);
    if(!op.empty()) printf("%s ", op.c_str());
    
    
    
    if('-'!=(c=info[size+26]))*s++=c;
    if('-'!=(c=info[size+52]))
    {
        if(c=='a')c='1'+A;if(c=='x')c='1'+X;
        if(c=='m')s+=sprintf(s,"$%02X $%02X",data[1],data[2]);
        else if(c=='r'){ signed char n=data[1];
                         s+=sprintf(s,"%+d (%06X)",n,address+n+2); }
        else if(c=='R'){ signed short n=data[1]+data[2]*256;
                         s+=sprintf(s,"%+d (%06X)",n,address+n+3); }
        else{*s++='$';for(c-='0';c;s+=sprintf(s,"%02X",data[c--]));}
    }
    if(info[size]=='x')s+=sprintf(s,",x");
    if('-'!=(c=info[size+78]))
        if(c=='s')s+=sprintf(s,",s");
        else if(c=='S')s+=sprintf(s,",s)");
        else *s++=c;
    if(info[size]=='y')s+=sprintf(s,",y");
    if(size==1)size=2+A;else if(size==2)size=2+X;
    else size=info[size+104]-'0';
    printf(" %06X\t", address);
    for(unsigned n=0; n<4; ++n)
        printf(n<size?"%02X ":"   ", data[n]);
    
    *s=0; printf("%s\n", Buf);
}

/* Bisqwit's humble little snes-disassembler. */
static void DisAsm(unsigned origin, const unsigned char *data,
                   unsigned length)
{
    bool A=true,X=true;
    const unsigned char *begin = data;
    for(unsigned size,address=origin; length>0;
        address+=size,length-=size,data+=size)
    {
        static const addrmode addrmodes[26] =
        {
            {""        ,""},
            {"#%s"     ,"a"},
            {"#%s"     ,"x"},
            {"#%s"     ,"1"},
            {"%s"      ,"r"},
            {"%s"      ,"R"},
            {"%s"      ,"1"},
            {"%s,x"    ,"1"},
            {"%s,y"    ,"1"},
            {"(%s)"    ,"1"},
            {"(%s,x)"  ,"1"},
            {"(%s),y"  ,"1"},
            {"[%s]"    ,"1"},
            {"[%s],y"  ,"1"},
            {"%s"      ,"2"},
            {"%s,x"    ,"2"},
            {"%s,y"    ,"2"},
            {"%s"      ,"3"},
            {"%s,x"    ,"3"},
            {"%s,s"    ,"1"},
            {"(%s,s),y","1"},
            {"(%s)"    ,"2"},
            {"[%s]"    ,"2"},
            {"(%s,x)"  ,"2"},
            {""        ,""},
            {"%s"      ,"11"}
        };
        static const addrmode bytemode = {".byt %s",  "1"};
        static const addrmode wordmode = {".word %s", "2"};
        static const addrmode longmode = {".long %s", "3"};
        static const char info[] =
 // addressing modes
 "DKDTGGGMABYAOOOR" "ELJUGHHNAQYAOPPS" "OKRTGGGMABYAOOOR" "ELJUHHHNAQYAPPPS"
 "AKDTZGGMABYAOOOR" "ELJUZHHNAQAARPPS" "AKFTGGGMABYAVOOR" "ELJUHHHNAQAAXPPS"
 "EKFTGGGMABAAOOOR" "ELJUHHINAQAAOPPS" "CKCTGGGMABAAOOOR" "ELJUHHINAQAAPPQS"
 "CKDTGGGMABAAOOOR" "ELJJJHHNAQAAWPPS" "CKDTGGGMABAAOOOR" "ELJUOHHNAQAAXPPS"
 // opcodes
 "BRKORA" "COPORA" "TSBORA" "ASLORA" "PHPORA" "ASLPHD" "TSBORA" "ASLORA"
 "BPLORA" "ORAORA" "TRBORA" "ASLORA" "CLCORA" "INCTCS" "TRBORA" "ASLORA"
 "JSRAND" "JSLAND" "BITAND" "ROLAND" "PLPAND" "ROLPLD" "BITAND" "ROLAND"
 "BMIAND" "ANDAND" "BITAND" "ROLAND" "SECAND" "DECTSC" "BITAND" "ROLAND"
 "RTIEOR" "DB EOR" "MVPEOR" "LSREOR" "PHAEOR" "LSRPHK" "JMPEOR" "LSREOR"
 "BVCEOR" "EOREOR" "MVNEOR" "LSREOR" "CLIEOR" "PHYTCD" "JMPEOR" "LSREOR"
 "RTSADC" "PERADC" "STZADC" "RORADC" "PLAADC" "RORRTL" "JMPADC" "RORADC"
 "BVSADC" "ADCADC" "STZADC" "RORADC" "SEIADC" "PLYTDC" "JMPADC" "RORADC"
 "BRASTA" "BRLSTA" "STYSTA" "STXSTA" "DEYBIT" "TXAPHB" "STYSTA" "STXSTA"
 "BCCSTA" "STASTA" "STYSTA" "STXSTA" "TYASTA" "TXSTXY" "STZSTA" "STZSTA"
 "LDYLDA" "LDXLDA" "LDYLDA" "LDXLDA" "TAYLDA" "TAXPLB" "LDYLDA" "LDXLDA"
 "BCSLDA" "LDALDA" "LDYLDA" "LDXLDA" "CLVLDA" "TSXTYX" "LDYLDA" "LDXLDA"
 "CPYCMP" "REPCMP" "CPYCMP" "DECCMP" "INYCMP" "DEXWAI" "CPYCMP" "DECCMP"
 "BNECMP" "CMPCMP" "PEICMP" "DECCMP" "CLDCMP" "PHXSTP" "JMLCMP" "DECCMP"
 "CPXSBC" "SEPSBC" "CPXSBC" "INCSBC" "INXSBC" "NOPXBA" "CPXSBC" "INCSBC"
 "BEQSBC" "SBCSBC" "PEASBC" "INCSBC" "SEDSBC" "PLXXCE" "JSRSBC" "INCSBC";
        const unsigned mode=info[*data]-'A'; // addressing mode
        switch(FulFillable(addrmodes[mode], data+1))
        {
            case 0: break;
            default:
            case 1: size = DumpIns(address,X,Y, ".byt",  bytemode, data); continue;
            case 2: size = DumpIns(address,X,Y, ".word", wordmode, data); continue;
            case 3: size = DumpIns(address,X,Y, ".long", longmode, data); continue;
        }
        if(*data==0xE2){c=data[1];if(c&0x20)A=false; if(c&0x10)X=false; }
        if(*data==0xC2){c=data[1];if(c&0x20)A=true;  if(c&0x10)X=true;  }
        size = DumpIns(address,X,Y,
                       std::string(info + 256 + *data*3, 3), addrmodes[mode], data+1)+1;
        if(length <= size)break;
    }
}
