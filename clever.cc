#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <algorithm>
#include <string>
#include <vector>
#include <deque>
#include <set>
#include <map>
#include <stdint.h>

//#define TRACK_RAM_SIZE 0x800
#define TRACK_RAM_SIZE 0x100

struct BadAddressException { };

static unsigned LastPage;
static unsigned char* ROM;
static unsigned char* Pages[8] = {0,0,0,0};
inline unsigned char& Rd6502(unsigned addr)
{
    unsigned char page = addr >> 13;
    unsigned pageaddr  = addr & 0x1FFF;
    
    //printf("%X = page %u, addr %X\n", addr, page, pageaddr);
    
    return Pages[page][pageaddr];
}
static void SetPage(unsigned addrpage, unsigned rompage)
{
    unsigned char* ptr = ROM + (rompage << 13);
    //printf("SetPage(%u,%p)\n", addrpage,ptr);
    Pages[addrpage] = ptr;
}
static unsigned addr_to_rom(unsigned addrptr)
{
    if(addrptr < 0x8000)
    {
        throw BadAddressException();
    }
    /* returns the index to ROM according to current mapping */
    unsigned char* addr = &Rd6502(addrptr);
    unsigned romptr = addr - ROM;
    //printf("addr_to_rom(%X) = %X\n", addrptr, romptr);
    return romptr;
}

static unsigned Page0, Page1, Page2, Page3;

static unsigned rom_to_addr(unsigned romptr)
{
    unsigned rompage = romptr >> 13;
    unsigned romaddr = romptr & 0x1FFF;
    if(rompage == LastPage
    || rompage == LastPage-1)
    {
        SetPage(6, Page2=LastPage-1);
        SetPage(7, Page3=LastPage);
        return 0xC000 + romaddr + (rompage&1)*0x2000;
    }

    SetPage(4, Page0 = (rompage&~1));
    SetPage(5, Page1 = (rompage&~1)+1);
    return 0x8000 + romaddr + (rompage&1)*0x2000;
}

#define WORD(A) (Rd6502((A)+1)*256+Rd6502((A)))
#define BYTE(A) (Rd6502(A))

enum Addressing_Modes { Ac=0,Il,Im,Ab,Zp,Zx,Zy,Ax,Ay,Rl,Ix,Iy,In,Iw, No=127 };

static const char mn[][3+1]=
{
  "adc","and","asl","bcc","bcs", "beq","bit","bmi","bne","bpl", //0,5
  "brk","bvc","bvs","clc","cld", "cli","clv","cmp","cpx","cpy", //10,15
  "dec","dex","dey","inx","iny", "eor","inc","jmp","jsr","lda", //20,25
  "nop","ldx","ldy","lsr","ora", "pha","php","pla","plp","rol", //30,35
  "ror","rti","rts","sbc","sta", "stx","sty","sec","sed","sei", //40,45
  "tax","tay","txa","tya","tsx", "txs"                          //50,55
};

static unsigned char ad[512]=
{
  10,Il, 34,Ix, No,No, No,No, No,No, 34,Zp,  2,Zp, No,No, //00
  36,Il, 34,Im,  2,Ac, No,No, No,No, 34,Ab,  2,Ab, No,No,
   9,Rl, 34,Iy, No,No, No,No, No,No, 34,Zx,  2,Zx, No,No, //10
  13,Il, 34,Ay, No,No, No,No, No,No, 34,Ax,  2,Ax, No,No, 
  28,Iw,  1,Ix, No,No, No,No,  6,Zp,  1,Zp, 39,Zp, No,No, //20
  38,Il,  1,Im, 39,Ac, No,No,  6,Ab,  1,Ab, 39,Ab, No,No,
   7,Rl,  1,Iy, No,No, No,No, No,No,  1,Zx, 39,Zx, No,No, //30
  47,Il,  1,Ay, No,No, No,No, No,No,  1,Ax, 39,Ax, No,No,
  41,Il, 25,Ix, No,No, No,No, No,No, 25,Zp, 33,Zp, No,No, //40
  35,Il, 25,Im, 33,Ac, No,No, 27,Iw, 25,Ab, 33,Ab, No,No,
  11,Rl, 25,Iy, No,No, No,No, No,No, 25,Zx, 33,Zx, No,No, //50
  15,Il, 25,Ay, No,No, No,No, No,No, 25,Ax, 33,Ax, No,No,
  42,Il,  0,Ix, No,No, No,No, No,No,  0,Zp, 40,Zp, No,No, //60
  37,Il,  0,Im, 40,Ac, No,No, 27,In,  0,Ab, 40,Ab, No,No,
  12,Rl,  0,Iy, No,No, No,No, No,No,  0,Zx, 40,Zx, No,No, //70
  49,Il,  0,Ay, No,No, No,No, No,No,  0,Ax, 40,Ax, No,No,
  No,No, 44,Ix, No,No, No,No, 46,Zp, 44,Zp, 45,Zp, No,No, //80
  22,Il, No,No, 52,Il, No,No, 46,Ab, 44,Ab, 45,Ab, No,No,
   3,Rl, 44,Iy, No,No, No,No, 46,Zx, 44,Zx, 45,Zy, No,No, //90
  53,Il, 44,Ay, 55,Il, No,No, No,No, 44,Ax, No,No, No,No,
  32,Im, 29,Ix, 31,Im, No,No, 32,Zp, 29,Zp, 31,Zp, No,No, //A0
  51,Il, 29,Im, 50,Il, No,No, 32,Ab, 29,Ab, 31,Ab, No,No,
   4,Rl, 29,Iy, No,No, No,No, 32,Zx, 29,Zx, 31,Zy, No,No, //B0
  16,Il, 29,Ay, 54,Il, No,No, 32,Ax, 29,Ax, 31,Ay, No,No, 
  19,Im, 17,Ix, No,No, No,No, 19,Zp, 17,Zp, 20,Zp, No,No, //C0
  24,Il, 17,Im, 21,Il, No,No, 19,Ab, 17,Ab, 20,Ab, No,No,
   8,Rl, 17,Iy, No,No, No,No, No,No, 17,Zx, 20,Zx, No,No, //D0
  14,Il, 17,Ay, No,No, No,No, No,No, 17,Ax, 20,Ax, No,No,
  18,Im, 43,Ix, No,No, No,No, 18,Zp, 43,Zp, 26,Zp, No,No, //E0
  23,Il, 43,Im, 30,Il, No,No, 18,Ab, 43,Ab, 26,Ab, No,No,
   5,Rl, 43,Iy, No,No, No,No, No,No, 43,Zx, 26,Zx, No,No, //F0
  48,Il, 43,Ay, No,No, No,No, No,No, 43,Ax, 26,Ax, No,No
};

struct Disassembly
{
    const char*      Code;
    unsigned         Bytes;
    Addressing_Modes Mode;
    int              Param; // 6502 addr, not ROM addr */
    const char*      Prefix;
    const char*      Suffix;

    int OpCodeId;

    Disassembly(): Code(""),Bytes(0),Mode(No),Param(0),Prefix(""),Suffix("") {}
};

static Disassembly DAsm(const unsigned opaddr) /* 6502 addrs, not ROM addrs. */
{
    Disassembly result;
    
    unsigned addr = opaddr;
    const unsigned char op = BYTE(addr++);
    
    result.OpCodeId = ad[op*2];
    result.Mode = (Addressing_Modes)ad[op*2+1];
    result.Code = result.Mode == No ? ".db" : mn[result.OpCodeId];
    result.Prefix = "";
    result.Suffix = "";
    
    switch(result.Mode)
    {
        case No: result.Param = op; break;
        case Ac: result.Suffix = "a"; break;
        case Il: break;
        case Rl:
        {
            unsigned char J = BYTE(addr++);
            unsigned target = opaddr + 2 + ((J < 0x80) ? J : (J - 256));
            result.Param = target;
            break;
        }
        case Im:
            result.Prefix = "#";
            result.Param = BYTE(addr++); break;
        case Zp:
            result.Param = BYTE(addr++); break;
        case Zx:
            result.Suffix = ",x";
            result.Param = BYTE(addr++); break;
        case Zy:
            result.Suffix = ",y";
            result.Param = BYTE(addr++); break;
        case Ix:
            result.Prefix = "("; result.Suffix = ",x)";
            result.Param = BYTE(addr++); break;
        case Iy:
            result.Prefix = "("; result.Suffix = "),y";
            result.Param = BYTE(addr++); break;
        case Ab: // Used when addressing
            result.Param = WORD(addr); addr += 2; break;
        case Iw: // Used in jsr/jmp
            result.Param = WORD(addr); addr += 2; break;
        case Ax:
            result.Suffix = ",x";
            result.Param = WORD(addr); addr += 2; break;
        case Ay:
            result.Suffix = ",y";
            result.Param = WORD(addr); addr += 2; break;
        case In:
            result.Prefix = "(";
            result.Suffix = ")";
            result.Param = WORD(addr); addr += 2; break;
    }
    if(addr == opaddr) std::abort();
    result.Bytes = addr - opaddr;
    return result;
}


/* Usable functions:
 *   DAsm(char* textbuffer, unsigned addr); // uses Rd6502 to read.
 *   unsigned Rd6502(unsigned addr);
 */

struct ValueNotKnownException { };

static void PrintRomAddress(unsigned romptr)
{
    unsigned rompage = romptr >> 13;
    unsigned romaddr = romptr & 0x1FFF;
    unsigned res = 0;
    if(rompage == LastPage)   { res = 0xE000 + romaddr; goto done; }
    if(rompage == LastPage-1) { res = 0xC000 + romaddr; goto done; }
    if(rompage & 1)
        { res = 0xA000 + romaddr; goto done; }
    else
        { res = 0x8000 + romaddr; goto done; }
done:
    printf("$%04X", res);
}

enum CodeLikelihood
{
    CertainlyCode=100,
    MaybeCode    =59, // certainty=9 (just a little worse than MaybeData)
    Unknown      =50,
    
    MaybeData=40, // certainty=10
    PartialCode  =39, // it's data, but not something to be displayed.

    UsedJumpJumpPtr=23,
    UsedDataDataPtr=22,
    
    UsedJumpPtr  =21,
    UsedDataPtr  =20,
    
    UnusedJumpJumpPtr=13,
    UnusedDataDataPtr=12,

    UnusedJumpPtr=11,
    UnusedDataPtr=10,
    
    CertainlyData=0
    /* After a "beq" or "bcs" and such,
     * the next statement is not necessarily code,
     * although it usually is.
     */
};
enum SpecialTypes
{
    None,
    DataTableRoutineWithXY,
    JumpTableRoutineWithAppendix,
    MapperChangeRoutineWithReg,
    MapperChangeRoutineWithConst,
    MapperChangeRoutineWithRAM,
    TerminatedStringRoutine
};

static int CertaintyOf(CodeLikelihood type)
{
    int certainty = type-50; if(certainty<0) certainty=-certainty;
    return certainty;
}
static bool IsDataType(CodeLikelihood c)
{
    return c==MaybeData || c==CertainlyData || c==Unknown;
}
static bool IsVisitWorthy(CodeLikelihood c)
{
    switch(c)
    {
        case CertainlyCode:
        case MaybeCode:
        case MaybeData:
        case UnusedJumpJumpPtr:
        case UnusedDataDataPtr:
        case UnusedJumpPtr:
        case UnusedDataPtr:
        //case CertainlyData:
            return true;
        default:
            return false;
    }
}

class SimulCPU;
struct SimulReg
{
private:
    uint_least32_t romaddr;
    int_least32_t defined_at;
    int_least16_t value;
    char known; //0=false, 1=true, 2=uninitialized, 3=rom_xindex, 4=rom_yindex, 5=weak
    
    friend class SimulCPU;
public:    
    SimulReg(): defined_at(-1), value(0),known(2) { }
    void Invalidate() { known=0; defined_at = -1; }
    void MakeWeak() { if(known==1) known=5; else Invalidate(); }
    void Assign(const SimulReg& reg) { *this = reg; }
    void Assign(int v, int defloc=-1) { value=v; known=1; defined_at = defloc; }
    void AssignXindex(unsigned romptr, int defloc=-1) { romaddr=romptr; known=3; defined_at = defloc; }
    void AssignYindex(unsigned romptr, int defloc=-1) { romaddr=romptr; known=4; defined_at = defloc; }
    
    void Combine(const SimulReg& reg)
    {
        switch(known)
        {
            case 0:
            case 2:
                *this = reg; break;
            case 1:
            case 3:
            case 4:
                if(value != reg.value && reg.known != 5 && reg.known != 2)
                    Invalidate();
                break;
            case 5:
                if(reg.known == 0 || reg.known == 2) {} //Invalidate();
                else *this = reg;
                break;
        }
    }
    
    bool Known() const { return known == 1 || known == 5; }
    
    unsigned IsIndexed() const { switch(known) { case 3:case 4: return romaddr; } return 0; }
    char GetIndexType() const { switch(known) { case 3:return 'x';case 4:return 'y'; } return 0; }
    unsigned Value() const { return value; }
    int GetDefineLocation() const { return defined_at; }
    const SimulReg& GetRef() const { return *this; }
    
public:
    void Dump() const
    {
        switch(known)
        {
            case 0: printf("(?)"); break;
            case 1: printf("(%02X)", value); break;
            case 2: printf("(...)"); break;
            case 3: printf("$%04X,x", romaddr); break;
            case 4: printf("$%04X,y", romaddr); break;
            case 5: printf("(?%02X)", value); break;
        }
    }
};
struct SimulFlag
{
private:
    bool value;
    char known;  //0=false, 1=true, 2=uninitialized
    //int_least32_t defined_at;
public:
    SimulFlag() :value(false),known(2)/*, defined_at(-1)*/ { }
    void Invalidate() { known=0; /*defined_at = -1;*/ }
    void Assign(const SimulFlag& reg) { *this = reg; }
    void Assign(bool v, int defloc=-1) { value=v; known=1; /*defined_at=defloc;*/ }
    void Combine(const SimulFlag& reg)
    {
        if(known != 1) *this = reg;
        else if(reg.known == 1 && value != reg.value) Invalidate();
        else if(reg.known == 0) Invalidate();
    }
    bool Known() const { return known == 1; }
    bool Value() const { return value; }
    //int GetDefineLocation() const { return defined_at; }
    
    void Dump() const
    {
        switch(known)
        {
            case 0: printf("?"); break;
            case 2: printf("*"); break;
            case 1: printf("%u", (unsigned)value); break;
        }
    }
};

struct SimulRegPtr
{
    SimulRegPtr() : ptr(0) { }
    ~SimulRegPtr() { delete ptr; }
    
    void Invalidate() { delete ptr; ptr = 0; }
    void Combine(const SimulRegPtr& b)
        { if(!b.ptr) Invalidate(); else if(ptr) ptr->Combine(*b.ptr); else Assign(*b.ptr); }
    bool Known() const { return ptr && ptr->Known(); }
    unsigned char Value() const { return ptr ? ptr->Value() : 0; }
    const SimulReg& GetRef() const { static SimulReg idle; return ptr ? *ptr : idle; }
    
    template<typename T>
    void Assign(const T b) { if(!ptr) ptr = new SimulReg; ptr->Assign(b); }

    SimulRegPtr(const SimulRegPtr& b)
        { ptr=0; if(b.ptr) Assign(*b.ptr); }
    SimulRegPtr& operator=(const SimulRegPtr& b)
        { delete ptr; ptr=0; if(b.ptr)Assign(*b.ptr); return *this; }

    void Dump() const
    {
        if(ptr) ptr->Dump();
        else printf("(?)");
    }
private:
    SimulReg* ptr;
};

struct SimulCPU
{
    SimulReg A, X, Y;
    SimulFlag Zflag;
    SimulFlag Sflag;
    std::deque<SimulReg> Stack;
    SimulRegPtr RAM[TRACK_RAM_SIZE];
    
    SimulReg pagereg[4];
    
    void SetPageReg(unsigned page, SimulReg& v) { pagereg[page].Assign(v); }
    void SetPageReg(unsigned page, int v, bool weak = false)
    {
        if(weak)
            { SimulReg tmp; tmp.Assign(v); tmp.MakeWeak();
              pagereg[page].Combine(tmp); }
        else
            pagereg[page].Assign(v);
    }
    
    void LoadMap() const
    {
        if(pagereg[0].Known()) SetPage(4, Page0 = pagereg[0].Value());
        if(pagereg[1].Known()) SetPage(5, Page1 = pagereg[1].Value());
        if(pagereg[2].Known()) SetPage(6, Page2 = pagereg[2].Value());
        if(pagereg[3].Known()) SetPage(7, Page3 = pagereg[3].Value());
    }
    
    int GuessPage45() const
    {
        if(pagereg[0].Known()) return pagereg[0].Value() / 2;
        return -1;
    }
    
    void Invalidate()
    {
        A.Invalidate();
        X.Invalidate();
        Y.Invalidate();
        Zflag.Invalidate();
        Sflag.Invalidate();
        for(unsigned a=0; a<4; ++a) pagereg[a].MakeWeak();
        for(unsigned a=0; a<TRACK_RAM_SIZE; ++a) RAM[a].Invalidate();
        //RAM[0x24].Assign(6);
    }
    void Push(const SimulReg& reg) { Stack.push_back(reg); }
    void Pop(SimulReg& reg)
    {
        if(Stack.empty()) reg.Invalidate();
        else { reg.Assign(Stack.back()); Stack.pop_back(); }
    }
    void Pop()
    {
        SimulReg tmp; Pop(tmp);
    }
    void Push() { SimulReg tmp; Push(tmp); }
    
    void Combine(const SimulCPU& cpu2)
    {
        A.Combine(cpu2.A);
        X.Combine(cpu2.X);
        Y.Combine(cpu2.Y);
        Zflag.Combine(cpu2.Zflag);
        Sflag.Combine(cpu2.Sflag);
        for(unsigned a=0; a<4; ++a)
            pagereg[a].Combine(cpu2.pagereg[a]);
        for(unsigned a=0; a<TRACK_RAM_SIZE; ++a)
            RAM[a].Combine(cpu2.RAM[a]);
        
        if(Stack.empty())
            Stack = cpu2.Stack;
        else
            Stack.clear();
    }
    
    unsigned ReadRAM(unsigned addr) const
    {
        if(addr >= TRACK_RAM_SIZE || !RAM[addr].Known()) throw ValueNotKnownException();
        return RAM[addr].Value();
    }
    
    void Dump() const
    {
        printf("\t\t\t/* ");
        
        printf("A"); A.Dump();
        printf("X"); X.Dump();
        printf("Y"); Y.Dump();
        
        printf("MAP[");
        printf("%u:", Page0); pagereg[0].Dump();
        printf(",%u:", Page1); pagereg[1].Dump();
        printf(",%u:", Page2); pagereg[2].Dump();
        printf(",%u:", Page3); pagereg[3].Dump();
        printf("]");
        
        printf("s(%u)", (unsigned)Stack.size());
        printf("Z"); Zflag.Dump();
        printf("S"); Sflag.Dump();
        printf("*/");
    }

    SimulCPU()
    {
        ImportMap(true);
    }
    
    void ImportMap(bool weak)
    {
        ImportMap(0, weak);
        ImportMap(1, weak);
        ImportMap(2, weak);
        ImportMap(3, weak);
    }
    void ImportMap(unsigned page, bool weak)
    {
        //printf("Import map. Orig: "); Dump(); printf("\n");
        switch(page)
        {
            case 0: SetPageReg(0, Page0, weak); break;
            case 1: SetPageReg(1, Page1, weak); break;
            case 2: SetPageReg(2, Page2, weak); break;
            case 3: SetPageReg(3, Page3, weak); break;
        }
        //printf("-------> Changed: "); Dump(); printf("\n");
    }
    
    void MapperWrite(unsigned addr, const SimulReg& reg)
    {
#if 0 /* MAPPER 2 */
        if(reg.Known())
        {
            SetPageReg(0, reg.Value()*2+0, false);
            SetPageReg(1, reg.Value()*2+1, false);
            
        }
        else
        {
            pagereg[0].MakeWeak();
            pagereg[1].MakeWeak();
        }
#endif
#if 1 /* MAPPER 1 */
        if(!reg.Known())
        {
            pagereg[0].MakeWeak();
            pagereg[1].MakeWeak();
            pagereg[2].MakeWeak();
            pagereg[3].MakeWeak();
            return;
        }
        static unsigned regs[4], shift, buffer;
        unsigned n = (addr >> 13)-4;
        unsigned v = reg.Value();
        bool regnew[4] = {false,false,false,false};
        bool p0_weak=true, p2_weak=true;
        if(v & 0x80)
        {
            regs[0] |= 0x0C;
            shift=buffer=0;
            goto DoMMC1_PRG;
        }
        buffer |= (v&1) << shift++;
        if(shift == 5)
        {
            regs[n] = buffer;
            regnew[n] = true;
            shift=buffer=0;
            if(n != 2)
            {
            DoMMC1_PRG:
                unsigned offs = regs[1]&0x10;
                unsigned newp0=0, newp2=2;
                switch(regs[0] & 0x0C)
                {
                    case 0: case 4:
                        newp0=(regs[3]&~1)+offs; p0_weak = !regnew[3] || !regnew[1];
                        newp2=newp0+2;           p2_weak = p0_weak;
                        break;
                    case 8:
                        newp2=regs[3];           p2_weak = !regnew[3];
                        newp0=offs;              p0_weak = !regnew[1];
                        break;
                    case 12:
                        newp0=regs[3];           p0_weak = !regnew[3];
                        newp2=0xF+offs;          p2_weak = !regnew[1];
                        break;
                }
                SetPageReg(0, newp0*2  , p0_weak);
                SetPageReg(1, newp0*2+1, p0_weak);
                SetPageReg(2, newp2*2  , p2_weak);
                SetPageReg(3, newp2*2+1, p2_weak);
                LoadMap();
            }
        }
#endif
    }
    void MapperProgram(const SimulReg& reg)
    {
#if 1 /* MAPPER 1*/
        if(!reg.Known())
        {
            pagereg[0].MakeWeak();
            pagereg[1].MakeWeak();
            return;
        }
        SetPageReg(0, reg.Value()*2+0, false);
        SetPageReg(1, reg.Value()*2+1, false);
        LoadMap();
        
        printf("Mapper regs now(");
        printf("%u:", Page0); pagereg[0].Dump();
        printf(",%u:", Page1); pagereg[1].Dump();
        printf(",%u:", Page2); pagereg[2].Dump();
        printf(",%u:", Page3); pagereg[3].Dump();
        printf(")\n");
#endif
    }
    
private:
    //SimulCPU(const SimulCPU&);

};

struct PointerTableItem
{
    unsigned loptr;
    unsigned hiptr;
    unsigned stepping;
    bool final;
    int offset;
    
    int page45;
    
    std::string nametemplate;
    bool is_default_name;
    
    PointerTableItem()
        : stepping(0), final(true), offset(0),
          page45(-1),
          is_default_name(true) {}

    void LoadMemMaps() const
    {
        rom_to_addr(loptr); // Autoguess mappings
        
        if(page45 >= 0)
        {
            fprintf(stderr, "Reading pointer value at %X (page=%d)\n", 
                loptr, page45);
            
            SetPage(4, page45*2    );
            SetPage(5, page45*2 + 1);
        }
    }
};

struct State
{
    CodeLikelihood Type;
    
    SpecialTypes SpecialType;
    unsigned     SpecialTypeParam;
    
    // Code:
    std::set<unsigned/*romptr*/> CalledFrom;
    int FirstJumpFrom;
    int LastJumpFrom;
    int JumpsTo;
    
    std::set<std::string> Labels;
    SimulCPU cpu;
    Disassembly code;
    
    unsigned referred_address;
    int      referred_offset;  // for example, offs=1 when code says $CFDA but refaddr is $CFDB

    enum reftype { none=0, lo=1, hi=2, times64=3 };
    
    struct {
        reftype referred_byte    : 2;
        bool meaning_interpreted : 1;
        bool barrier             : 1;
        char is_referred         : 3; // 1=yes, 2=indexed, 4=data referrals
    };
    
    // Data:
    unsigned ArraySize; // size of 1 element
    unsigned ElemCount; // number of elements
    
    PointerTableItem PtrAddr;

public:    
    State():
        Type(Unknown),
        SpecialType(None),
        FirstJumpFrom(-1),
        LastJumpFrom(-1),
        JumpsTo(-1),
        referred_address(0), referred_offset(0),
        referred_byte(none),
        meaning_interpreted(false),
        barrier(false),
        is_referred(0),
        ArraySize(0), ElemCount(0)
        { }
    
    void JumpedFromInsert(unsigned romptr)
    {
        if(FirstJumpFrom == -1 || romptr < (unsigned)FirstJumpFrom) FirstJumpFrom = romptr;
        if(LastJumpFrom == -1  || romptr > (unsigned)LastJumpFrom) LastJumpFrom = romptr;
    }

private:
    //State(const State&);
};

struct CompareCodeLikelihood
{
    CompareCodeLikelihood(const std::vector<State>& s): results(s) { }
    
    static int VisitValue(CodeLikelihood l)
    {
        if(!IsVisitWorthy(l)) return 0;
        if(l >= 50) return 50+CertaintyOf(l);
        return CertaintyOf(l);
    }
    
    bool operator() (unsigned romptr1, unsigned romptr2) const
    {
        CodeLikelihood type1 = results[romptr1].Type;
        CodeLikelihood type2 = results[romptr2].Type;
        
        return VisitValue(type1) > VisitValue(type2);
    }
    const std::vector<State>& results;
};

class Disassembler
{
public:
    Disassembler(unsigned romsize): results(romsize) {}
private:
    //Disassembler(const Disassembler&);
    //void operator= (const Disassembler&);
    
public:
    void DiscoverRegions()
    {
    Retry:
        while(!VisitList.empty())
        {
            SortVisitList();
            
            /* Pick the most likely place to have code */
            unsigned romptr = VisitList[0];
            
            //printf("Try %05X (type %d)\n", romptr, results[romptr].Type);
            
            VisitList.erase(VisitList.begin());
            
            /* If the frontmost item is not code, break the loop. */
            if(results[romptr].Type <= 50)
            {
                /* However, process pointers if applicable... */
                switch(results[romptr].Type)
                {
                    case UnusedJumpJumpPtr:
                        InstallJumpJumpPointerTableEntry(results[romptr].PtrAddr);
                        results[romptr].Type = UsedJumpJumpPtr;
                        goto Retry;

                    case UnusedJumpPtr:
                        InstallJumpPointerTableEntry(results[romptr].PtrAddr);
                        results[romptr].Type = UsedJumpPtr;
                        goto Retry;

                    case UnusedDataDataPtr:
                        InstallDataDataPointerTableEntry(results[romptr].PtrAddr);
                        results[romptr].Type = UsedDataDataPtr;
                        goto Retry;

                    case UnusedDataPtr:
                        InstallDataPointerTableEntry(results[romptr].PtrAddr);
                        results[romptr].Type = UsedDataPtr;
                        goto Retry;

                    default: break;
                }
                break;
            }
            
            /* Visit if not visited yet. */
            if(Visited.find(romptr) == Visited.end())
            {
                Visited.insert(romptr);
                try
                {
                    ProcessCode(romptr);
                }
                catch(...)
                {
                    results[romptr].Type = CertainlyData;
                }
            }
        }
        
        if(DiscoverAddressReferences()) goto Retry;
        if(DiscoverIndexedAddresses()) goto Retry;
    }
    
    bool PossiblyMarkImmediatePointer(State& lo, State& hi, int refoffs=0, bool data=true)
    {
        unsigned address = lo.code.Param
                         + hi.code.Param*256
                         + refoffs;
        
        if(address >= 0x8000)
        {
            if(data)
                MarkAddressMaybeData(addr_to_rom(address), false, lo);
            else
                MarkAddressMaybeCode(addr_to_rom(address), lo);

            lo.meaning_interpreted = true;
            hi.meaning_interpreted = true;

            lo.referred_address = address;
            lo.referred_byte    = State::lo;
            lo.referred_offset  = refoffs;
            
            hi.referred_address = address;
            hi.referred_byte    = State::hi;
            hi.referred_offset  = refoffs;
            
            return true;
        }
        return false;
    }
    
    bool PossiblyMarkDPCMpointer(State& lo)
    {
        unsigned address = 0xC000 + lo.code.Param * 64;
        if(address >= 0x8000)
        {
            MarkAddressMaybeData(addr_to_rom(address), false, lo);

            lo.meaning_interpreted = true;

            lo.referred_address = address;
            lo.referred_byte    = State::times64;
            lo.referred_offset  = 0xC000;
            
            return true;
        }
        return false;
    }
    
    bool PossiblyMarkDataTable(State& lo, State& hi)
    {
      try {
        if(lo.code.Param == hi.code.Param-1)
        {
            lo.cpu.LoadMap();
            unsigned address = addr_to_rom(lo.code.Param);

            printf("; Possibly discovered a data table at %X ($%X) (page %d)\n",
                   address, lo.code.Param,
                   lo.cpu.GuessPage45());

            lo.meaning_interpreted = true;
            hi.meaning_interpreted = true;
            MarkDataTable(address+0, address+1, 2,
                          0,//extent
                          0,//offset
                          lo.cpu.GuessPage45());
            return true;
        }
        
        if(hi.code.Param > lo.code.Param
        && !((hi.code.Param - lo.code.Param)&1)
        && (hi.code.Param - lo.code.Param)/2 < 0x100)
        {
            lo.cpu.LoadMap();
            
            lo.meaning_interpreted = true;
            hi.meaning_interpreted = true;
            MarkDataTable(addr_to_rom(lo.code.Param),
                          addr_to_rom(hi.code.Param),
                          1,
                          (hi.code.Param - lo.code.Param)/2, // extent
                          0, //offset
                          lo.cpu.GuessPage45());
            return true;
        }
      }
      catch(BadAddressException)
      {
      }
        return false;
    }
    
    bool TestDPCMaddressPoke(State* States[4])
    {
        #define LDreg(code) \
            (code.Mode != Im && code.Mode != Ay \
                ? 0 \
                : code.OpCodeId == 29 ? 'A' \
                : code.OpCodeId == 31 ? 'X' \
                : code.OpCodeId == 32 ? 'Y'  \
                : 0 )
        #define STreg(code) \
            (  code.OpCodeId == 44 ? 'A' \
             : code.OpCodeId == 45 ? 'X' \
             : code.OpCodeId == 46 ? 'Y' \
             : 0 )
        
        char L0 = LDreg(States[0]->code), S0 = STreg(States[0]->code);
        char L1 = LDreg(States[1]->code), S1 = STreg(States[1]->code);
        
        #undef LDreg
        #undef STreg
        
        if(L0 != 0 && L0 == S1 && States[1]->code.Mode  == Ab
                               && States[1]->code.Param == 0x4012
                               && !States[0]->meaning_interpreted)
        {
            return PossiblyMarkDPCMpointer(*States[0]);
        }
        return false;
    }
    
    bool TestDataAddressPoke(State* States[4])
    {
        /*
           Possibilities (read up to down):
             ldx   ldy    ldx    ldy
             ldy   ldx    ldy    ldx
             stx   stx    sty    sty
             sty   sty    stx    stx

            note: xy denote _any_ registers, not just x and y
        */
        
        #define LDreg(code) \
            (code.Mode != Im && code.Mode != Ay \
                ? 0 \
                : code.OpCodeId == 29 ? 'A' \
                : code.OpCodeId == 31 ? 'X' \
                : code.OpCodeId == 32 ? 'Y'  \
                : 0 )
        #define STreg(code) \
            (  code.OpCodeId == 44 ? 'A' \
             : code.OpCodeId == 45 ? 'X' \
             : code.OpCodeId == 46 ? 'Y' \
             : 0 )
        
        char L0 = LDreg(States[0]->code), S0 = STreg(States[0]->code);
        char L1 = LDreg(States[1]->code), S1 = STreg(States[1]->code);
        char L2 = LDreg(States[2]->code), S2 = STreg(States[2]->code);
        char L3 = LDreg(States[3]->code), S3 = STreg(States[3]->code);
        
        #undef LDreg
        #undef STreg
        
        if(!L0           // first must be a load
        || (!L1 && !S1)  // second must be either
        || (!L2 && !S2)  // third must be either
        || !S3           // last must be a store
        || (!L0+!L1+!L2+!L3) != 2  // exactly two loads (or two non-loads) expected
        || (!S0+!S1+!S2+!S3) != 2  // exactly two stores (or two non-stores) expected
          )
        {
            return false;
        }
        
        int Load0, Load1;
        int Store0, Store1;
        
        // If both of the first ones are loads
        if(L0 && L1)
        {
            if(L0 == L1) return false; // They must load different regs
            if(L0 == S2 && L1 == S3)
            {
                // For example: ldx,ldy, stx,sty
                Load0 = 0; Store0 = 2;
                Load1 = 1; Store1 = 3;
            }
            else if(L0 == S3 && L1 == S2)
            {
                // For example: ldx,ldy, sty,stx
                Load0 = 0; Store0 = 3;
                Load1 = 1; Store1 = 2;
            }
            else return false; // Wrong types of stores
        }
        else if(L0 && L2)
        {
            if(L0 == S1 && L2 == S3)
            {
                Load0 = 0; Store0 = 1;
                Load1 = 2; Store1 = 3;
            }
            else return false; // Wrong types of stores
        }
        else return false; // Wrong types of loads
        
        if(States[Load0]->code.Mode == Ay && L0 == 'Y') return false;
        
        if(States[Load0]->code.Mode
        != States[Load1]->code.Mode) return false;
        
        if(States[Store0]->code.Param == States[Store1]->code.Param-1)
        {
            // yay, right order!
        }
        else if(States[Store0]->code.Param-1 == States[Store1]->code.Param)
        {
            // yay, reverse order!
            std::swap(Load0,Load1);
            std::swap(Store0,Store1);
        }
        else return false; // Not storing to consequential addresses
        
        if(States[Load0]->meaning_interpreted) return false;
        
        switch(States[Load0]->code.Mode)
        {
            case Im:
            {
                if(!PossiblyMarkImmediatePointer(*States[Load0], *States[Load1]))
                    return false;
                break;
            }
            case Ay:
            {
                if(!PossiblyMarkDataTable(*States[Load0], *States[Load1]))
                    return false;
                break;
            }
            default:;
        }
        return true;
    }
    
    bool TestCodeAddressPush(State* States[4])
    {
        /*
           Possibilities (read up to down):
             lda
             pha
             lda
             pha
           Not dealt with:
             txa    tya   pha    pha
             pha    pha   txa    tya
             tya    txa   pha    pha
             pha    pha
        */
        
        if(States[0]->code.Mode == Im && States[0]->code.OpCodeId == 29  // LDA
        && States[2]->code.Mode == Im && States[2]->code.OpCodeId == 29  // LDA
        && States[1]->code.OpCodeId == 35  // PHA
        && States[3]->code.OpCodeId == 35  // PHA
          )
        {
            if(States[0]->meaning_interpreted) return false;
            if(States[2]->meaning_interpreted) return false;
        
            return PossiblyMarkImmediatePointer(*States[2], *States[0],
                                                1, /* RTS pushes always have an offset of 1 */
                                                false);
        }
        return false;
    }
    
    bool TestLoadTableXY(State* States[4])
    {
        /*
           Possibilities:
               lda  ...,y     ldx ...,y
               tax            lda ...,y
               lda  ...,y     tay
               tay           
        */
        
        unsigned first  = 0; int first_op  = 29; // lda
        unsigned second = 2; int second_op = 29; // lda

        if(States[1]->code.OpCodeId == 50  // tax
        && States[3]->code.OpCodeId == 51) // tay
        {
            // ok possibility
        }
        else if(States[2]->code.OpCodeId == 51) // tay
        {
            first_op = 31; // ldx
            second   = 1;
            // ok possibility
        }
        else return false;

        if( States[first]->code.Mode == Ay && States[first]->code.OpCodeId == first_op
        && States[second]->code.Mode == Ay && States[second]->code.OpCodeId == second_op
        && !States[first]->meaning_interpreted
          )
        {
            if(PossiblyMarkDataTable(*States[first], *States[second])) return true;
        }

        return false;
    }

    bool DiscoverAddressReferences()
    {
        bool found_more_labels = false;
        
        for(unsigned romptr=0; romptr<results.size(); )
        {
            CodeLikelihood type = results[romptr].Type;
            if(type == PartialCode) { ++romptr; continue; }
            if(type <= 50) { ++romptr; continue; }
            
            State&      state0 = results[romptr];
            Disassembly& code0 = state0.code;
            unsigned bytes0 = code0.Bytes;
            unsigned romptr1 = romptr+bytes0;
            if(results[romptr1].Type < 50) { Ignore: romptr=romptr1; continue; }
            
            State&      state1 = results[romptr1];
            Disassembly& code1 = state1.code;
            unsigned bytes1 = code1.Bytes;
            unsigned romptr2 = romptr1+bytes1;
            if(results[romptr2].Type < 50) { goto Ignore; }
            
            State&      state2 = results[romptr2];
            Disassembly& code2 = state2.code;
            unsigned bytes2 = code2.Bytes;
            unsigned romptr3 = romptr2+bytes2;
            if(results[romptr3].Type < 50) { goto Ignore; }
            
            State&      state3 = results[romptr3];
            Disassembly& code3 = state3.code;
            unsigned bytes3 = code3.Bytes;
            unsigned romptr4 = romptr3+bytes3;
            
            State* States[4] = { &state0,&state1,&state2,&state3 };
            
            if(TestDPCMaddressPoke(States))
            {
                found_more_labels = true;
                romptr = romptr2;
                continue;
            }
            
            if(TestDataAddressPoke(States))
                { Success:
                    found_more_labels = true;
                    romptr=romptr4;
                    continue;
                }

            if(TestCodeAddressPush(States)) goto Success;

            if(TestLoadTableXY(States)) { goto Success; }
            
            goto Ignore;
        }
        return found_more_labels;
    }

    bool DiscoverIndexedAddresses()
    {
        bool found_more_labels = false;
        
        for(unsigned romptr=0; romptr<results.size(); )
        {
            CodeLikelihood type = results[romptr].Type;
            if(type == PartialCode) { ++romptr; continue; }
            if(type <= 50) { ++romptr; continue; }
            
            State&      state0 = results[romptr];
            Disassembly& code0 = state0.code;
            unsigned bytes0 = code0.Bytes;
            unsigned romptr1 = romptr+bytes0;
            if(results[romptr1].Type < 50) { Ignore: romptr=romptr1; continue; }
            
            if((code0.Mode == Ax || code0.Mode == Ay)
            && code0.Param >= 0x8000
            && code0.OpCodeId != 44
            && code0.OpCodeId != 45
            && code0.OpCodeId != 46
            && !state0.meaning_interpreted)
            {
                state0.meaning_interpreted = true;
                
                // Which mappings are active at this instruction?
                rom_to_addr(romptr); // Autoguess...
                
                if(MarkAddressMaybeData(addr_to_rom(code0.Param), true, state0))
                {
                    found_more_labels = true;
                    return true;
                }
            }
            
            goto Ignore;
        }
        return found_more_labels;
    }
    
    void PrintROMAddressName(unsigned romptr, bool is_jump, unsigned jump_from) const
    {
        if(romptr >= results.size())
        {
        Fallback:
            PrintRomAddress(romptr);
            return;
        }
        
        bool annotate = false;
        
        for(std::set<std::string>::const_iterator
            i = results[romptr].Labels.begin();
            i != results[romptr].Labels.end();
            ++i)
        {
            if(is_jump && (*i)[0] == '+') { if(jump_from >= romptr) continue; annotate = true; }
            if(is_jump && (*i)[0] == '-') { if(jump_from <  romptr) continue; annotate = true; }
            printf("%s", i->c_str());
            if(annotate)
            {
                printf("\t\t; ");
                PrintRomAddress(romptr);
                
                bool Thread_Jump = false;
                if(results[romptr].code.OpCodeId == 27 && results[romptr].code.Mode == Iw)
                    Thread_Jump = true;
                else if(results[romptr].code.OpCodeId == results[jump_from].code.OpCodeId
                     && results[romptr].code.Mode == Rl)
                {
                    Thread_Jump = true;
                }
                if(Thread_Jump)
                {
                    printf(" -> ");
                    unsigned target2 = results[romptr].code.Param;
                    unsigned romt2 = addr_to_rom(target2);
                    if(HasNonShortLabel(romt2))
                        PrintAddressName(target2, false);
                    else
                        PrintRomAddress(romt2); // avoiding printing "--" or "+" here.
                }
                return;
            }
            return;
        }
        goto Fallback;
    }
    
    void PrintAddressName(unsigned addr, bool is_jump = false, unsigned jump_from = 0) const
    {
        try
        {
            PrintROMAddressName(addr_to_rom(addr), is_jump, jump_from);
        }
        catch(const BadAddressException& )
        {
            printf("$%04X", addr);
        }
    }
    
    void DefineRAM(unsigned addr, const std::string& name)
    {
        RAMaddressNames[addr] = name;
    }
    
    void PrintRAMaddress(unsigned addr, unsigned bytes) const
    {
        std::map<unsigned, std::string>::const_iterator i = RAMaddressNames.find(addr);
        if(i != RAMaddressNames.end())
        {
            printf("%s", i->second.c_str());
            return;
        }
        printf("$%0*X", bytes*2, addr);
    }
    
    void DumpCode(unsigned romptr, const State& state, unsigned& code_indent) const
    {
        const Disassembly& code = state.code;
        
        unsigned bytes = code.Bytes;
        
        if(code.OpCodeId == 37 || code.OpCodeId == 38) // pla, plp
            if(code_indent > 0)--code_indent;
        
        for(unsigned a=0; a<bytes; ++a) printf(" %02X", ROM[romptr+a]);
        printf(": %*s", (4-bytes)*3 + code_indent, "");
        
        if(code.OpCodeId == 35 || code.OpCodeId == 36) // pha, php
            ++code_indent;
        
        if(code.OpCodeId == 42) // rts
            if(code_indent >= 2)code_indent -= 2;
        
        printf("%s %s", code.Code, code.Prefix);
        switch(code.Mode)
        {
            case Im:
            {
                switch(state.referred_byte)
                {
                    case State::none:
                        printf("$%02X", code.Param);
                        break;
                    case State::lo:
                    {
                        printf("<");
                        if(state.referred_offset != 0) printf("(");
                        PrintAddressName(state.referred_address);
                        if(state.referred_offset != 0) printf(" - %d)", state.referred_offset);
                        break;
                    }
                    case State::hi:
                    {
                        printf(">");
                        if(state.referred_offset != 0) printf("(");
                        PrintAddressName(state.referred_address);
                        if(state.referred_offset != 0) printf(" - %d)", state.referred_offset);
                        break;
                    }
                    case State::times64:
                    {
                        printf("((");
                        PrintAddressName(state.referred_address);
                        printf(" - $%X) / 64)", state.referred_offset);
                        break;
                    }
                }
                break;
            }
            
            case Zp: case Zx: case Zy:
            case Ix: case Iy:
            case No: PrintRAMaddress(code.Param, 1); break;
            case Ab: case Iw: case Ax: case Ay: case In:
            case Rl:
                if(code.Param >= 0x8000)
                {
                    bool is_jump = code.OpCodeId == 27 || code.Mode == Rl;
                    PrintAddressName(code.Param, is_jump, romptr);
                }
                else
                {
                    PrintRAMaddress(code.Param, 2);
                }
                break;
            case Ac: case Il: break;
        }
        printf("%s", code.Suffix);
        
        /*
        if(state.FirstJumpFrom >= 0)
            printf("; FirstJump=$%X", state.FirstJumpFrom);
        if(state.LastJumpFrom >= 0)
            printf("; LastJump=$%X", state.LastJumpFrom);
        */
    }

    void DumpNonCodeRanges() const
    {
        int begin=-1, last=0;
    
        for(unsigned romptr=0; romptr<results.size(); ++romptr)
        {
            int type = results[romptr].Type;
            if(type < 10 || type == 50)
            {
                if(begin>=0 && last != (int)(romptr-1))
                {
                    printf("|| (addr >= 0x%04X && addr <= 0x%04X)\n", begin|0x8000, last|0x8000); 
                    begin=-1;
                }
                if(begin<0) begin=romptr;
                last=romptr;
            }
        }
        if(begin>=0)
        {
            printf("|| (addr >= 0x%04X && addr <= 0x%04X)\n", begin|0x8000, last|0x8000); 
        }
    }
    
    void MakeNotShortCodeLabelCandidate(unsigned romptr)
    {
        for(std::set<std::string>::const_iterator
            j, i = results[romptr].Labels.begin();
            i != results[romptr].Labels.end();
            i=j)
        {
            j=i; ++j;
            if((*i)[0] == '-' || (*i)[0] == '+') results[romptr].Labels.erase(i);
        }
        results[romptr].LastJumpFrom = -1;
        results[romptr].FirstJumpFrom = -1;
    }
    
    bool IsBackwardShortCodeLabelCandidate(unsigned romptr) const
    {
        return (results[romptr].is_referred&5) == 1
            && results[romptr].Type >= 50
            && results[romptr].LastJumpFrom >= (int)romptr
            && results[romptr].CalledFrom.empty()
            && !HasShortLabel(romptr, '-')
            && !HasNonShortLabel(romptr, '+') // ignore forward labels
            ;
    }
    
    bool IsForwardShortCodeLabelCandidate(unsigned romptr) const
    {
        return (results[romptr].is_referred&5) == 1
            && results[romptr].Type >= 50
            && results[romptr].FirstJumpFrom >= 0
            && results[romptr].FirstJumpFrom < (int)romptr
            && results[romptr].CalledFrom.empty()
            && !HasShortLabel(romptr, '+')
            && !HasNonShortLabel(romptr, '-') // ignore backward labels
            ;
    }
    
    bool HasShortLabel(unsigned romptr, char type) const
    {
        for(std::set<std::string>::const_iterator
            i = results[romptr].Labels.begin();
            i != results[romptr].Labels.end();
            ++i)
        {
            if((*i)[0] == type) return true;
        }
        return false;
    }
    bool HasNonShortLabel(unsigned romptr, char ignore_type=0) const
    {
        for(std::set<std::string>::const_iterator
            i = results[romptr].Labels.begin();
            i != results[romptr].Labels.end();
            ++i)
        {
            char firstletter = (*i)[0];
            if(firstletter == ignore_type) continue;
            if(firstletter != '+' && firstletter != '-') return true;
        }
        return false;
    }
    
    void AssignMissingBackwardShortCodeLabels()
    {
        bool CandidatesRemain = true;
        for(unsigned length=1; CandidatesRemain && length<=4; ++length)
        {
            CandidatesRemain = false;
            
            const std::string backward_label(length, '-');
            
            std::vector<std::pair<unsigned, std::string> > give_labels;
            
            for(unsigned romptr=0; romptr < results.size(); ++romptr)
            {
                /* Check if this label may need a short forward label */
                if(!IsBackwardShortCodeLabelCandidate(romptr)) NextROMptr: continue;
                
                int LastJump  = results[romptr].LastJumpFrom;
                
                if(LastJump >= 0 && LastJump >= (int)romptr)
                {
                    for(int test=romptr; test<=LastJump; ++test)
                    {
                        /* If this point has a non-short label, don't cross it */
                        if(length == 1 && HasNonShortLabel(test))
                        {
                            MakeNotShortCodeLabelCandidate(romptr);
                            goto NextROMptr;
                        }
                        
                        /* If there exists a copy of this label
                         * between This and the LastJump, fail.
                         */
                        if(results[test].Labels.find(backward_label) != results[test].Labels.end())
                        {
                            CandidatesRemain = true;
                            goto NextROMptr;
                        }

                        if(test > (int)romptr && test <= LastJump
                        && IsBackwardShortCodeLabelCandidate(test))
                        {
                            CandidatesRemain = true;
                            goto NextROMptr;
                        }

                        /* If Test has no jump, it doesn't concern */
                        if(results[test].JumpsTo == -1) continue;
                        if(results[test].JumpsTo > test) continue; // forward jump
                        
                        // this jump may concern us!
                        
                        if(results[test].JumpsTo > (int)romptr
                        && IsBackwardShortCodeLabelCandidate(results[test].JumpsTo)
                          )
                        {
                            CandidatesRemain = true;
                            //fprintf(stderr, "%X: backward fail because of %X (len=%u)\n", romptr, test, length);
                            goto NextROMptr;
                        }
                        if(results[test].JumpsTo < (int)romptr
                        && HasShortLabel(results[test].JumpsTo, '-'))
                        {
                            CandidatesRemain = true;
                            //fprintf(stderr, "%X: backward fail because of %X (len=%u)\n", romptr, test, length);
                            goto NextROMptr;
                        }
                    }

                    //fprintf(stderr, "%X: created backward len=%u\n", romptr, length);
                    give_labels.push_back(std::make_pair(romptr, backward_label));
                }
            }
            
            for(unsigned a=0; a<give_labels.size(); ++a)
                results[give_labels[a].first].Labels.insert(give_labels[a].second);
            
            fflush(stderr);
        }
    }
    
    void AssignMissingForwardShortCodeLabels()
    {
        bool CandidatesRemain = true;
        for(unsigned length=1; CandidatesRemain && length<=4; ++length)
        {
            CandidatesRemain = false;
            
            const std::string forward_label(length, '+');
            
            std::vector<std::pair<unsigned, std::string> > give_labels;
            
            for(unsigned romptr=0; romptr < results.size(); ++romptr)
            {
                /* Check if this label may need a short forward label */
                if(!IsForwardShortCodeLabelCandidate(romptr)) NextROMptr: continue;
                
                int FirstJump  = results[romptr].FirstJumpFrom;
                
                if(FirstJump >= 0 && FirstJump < (int)romptr)
                {
                    for(int test=FirstJump; test < (int)romptr; ++test)
                    {
                        /* If this point has a non-short label, don't cross it */
                        if(length == 1 && HasNonShortLabel(test))
                        {
                            MakeNotShortCodeLabelCandidate(romptr);
                            goto NextROMptr;
                        }
                        
                        /* If there exists a copy of this label
                         * between This and the LastJump, fail.
                         */
                        if(results[test].Labels.find(forward_label) != results[test].Labels.end())
                        {
                            CandidatesRemain = true;
                            goto NextROMptr;
                        }
                        
                        if(test > FirstJump && test < (int)romptr
                        && IsForwardShortCodeLabelCandidate(test))
                        {
                            CandidatesRemain = true;
                            goto NextROMptr;
                        }

                        /* If Test has no jump, it doesn't concern */
                        if(results[test].JumpsTo == -1) continue;
                        if(results[test].JumpsTo <= test) continue; // backward jump
                        
                        // this jump may concern us!
                        
                        if(results[test].JumpsTo < (int)romptr
                        && IsForwardShortCodeLabelCandidate(results[test].JumpsTo)
                          )
                        {
                            CandidatesRemain = true;
                            //fprintf(stderr, "%X: forward fail because of %X (len=%u)\n", romptr, test, length);
                            goto NextROMptr;
                        }
                        if(results[test].JumpsTo > (int)romptr
                        && HasShortLabel(results[test].JumpsTo, '+'))
                        {
                            CandidatesRemain = true;
                            //fprintf(stderr, "%X: forward fail because of %X (len=%u)\n", romptr, test, length);
                            goto NextROMptr;
                        }
                    }

                    //fprintf(stderr, "%X: created forward len=%u\n", romptr, length);
                    give_labels.push_back(std::make_pair(romptr, forward_label));
                }
            }
            
            for(unsigned a=0; a<give_labels.size(); ++a)
                results[give_labels[a].first].Labels.insert(give_labels[a].second);
            
            fflush(stderr);
        }
    }
    
    void AssignMissingShortCodeLabels()
    {
        AssignMissingBackwardShortCodeLabels();
        AssignMissingForwardShortCodeLabels();
    }
    
    void AssignMissingLabels()
    {
        AssignMissingShortCodeLabels();
        
        /* Assign labels for locations that are missing them */
        for(unsigned romptr=0; romptr<results.size(); ++romptr)
        {
            if(results[romptr].is_referred)
            {
                if(results[romptr].Type < 50)
                {
                    // has already a label?
                    if(!results[romptr].Labels.empty()) continue;
                }
                else // it's code
                {
                    // if it already has a permanent label, skip
                    if(HasNonShortLabel(romptr)) continue;
                    
                    // It's not called from anywhere and has no data referrals,
                    // check if we can manage with a short label
                    if(results[romptr].CalledFrom.empty()
                    && !(results[romptr].is_referred & 4)
                      )
                    {
                        // Check if it has already short labels that satisfy all needs
                        if((results[romptr].LastJumpFrom < 0
                         || results[romptr].LastJumpFrom < (int)romptr
                         || HasShortLabel(romptr, '-'))
                        && (results[romptr].FirstJumpFrom < 0
                         || results[romptr].FirstJumpFrom >= (int)romptr
                         || HasShortLabel(romptr, '+'))
                           )
                        {
                            // No unsatisfied short labels...
                            // But if it's referred, it still needs a label.
                            if(!(results[romptr].is_referred
                             && results[romptr].Labels.empty()))
                            {
                                continue;
                            }
                        }
                        // It is unsatisfied! Remove short labels.
                    }
                    MakeNotShortCodeLabelCandidate(romptr);
                }
            
                std::string prefix = "_data_";
                std::string suffix = "";
                
                if(results[romptr].Type >= 50)
                {
                    if(!results[romptr].CalledFrom.empty())
                        prefix = "_func_";
                    else
                        prefix = "_loc_";
                }
                
                if(results[romptr].is_referred&2) suffix = "_indexed";

                char Buf[16];
                sprintf(Buf, "%04X", romptr);
                results[romptr].Labels.insert(prefix + Buf + suffix);
            }
        }
    }
    
    void Dump() const
    {
        unsigned code_indent = 0;
        
        for(unsigned romptr=0; romptr<results.size(); )
        {
            CodeLikelihood type = results[romptr].Type;
            
            unsigned need_nl = 0;
            
            for(std::set<std::string>::const_iterator
                i = results[romptr].Labels.begin();
                i != results[romptr].Labels.end();
                ++i)
            {
                const std::string& label = *i;
                
                // Non-local labels reset the indentation.
                if(label[0] != '+' && label[0] != '-') code_indent = 0;
                
                if(need_nl && (need_nl + 1 + label.size() > 7))
                    { need_nl=0; putchar('\n'); }
                
                if(need_nl) { ++need_nl; putchar(' '); }
                printf("%s", label.c_str());
                need_nl += label.size();
                
                if(need_nl > 7) { need_nl=0; putchar('\n'); }
            }

            if(type == PartialCode)
            {
            NextByte:
                ++romptr;
                if(need_nl) putchar('\n');
                continue;
            }
            
            //printf("(type%4d)", type);
            
            if(type > 50)
            {
                printf("\t");
                PrintRomAddress(romptr);
                printf(" ");
                need_nl = 0;
                
                const State& state = results[romptr];
                const Disassembly& code = state.code;
                
                unsigned bytes = code.Bytes;
                
                state.cpu.LoadMap();
                
                DumpCode(romptr, state, code_indent);
                //state.cpu.Dump();
                //if(state.barrier) printf("(barrier)");
                
                printf("\n");
                
                romptr += bytes;
                
                if(state.barrier)
                {
                    // The line after a barrier always has a label.
                    
                    if(HasNonShortLabel(romptr))
                        printf(";------------------------------------------\n");
                    else //if(results[romptr].Labels.empty())
                        printf("\n");
                }
                
                continue;
            }
            
            if(type == UnusedJumpPtr || type == UsedJumpPtr
            || type == UnusedDataPtr || type == UsedDataPtr
            || type == UnusedDataDataPtr || type == UsedDataDataPtr
            || type == UnusedJumpJumpPtr || type == UsedJumpJumpPtr
              )
            {
                const PointerTableItem& jmp = results[romptr].PtrAddr;
                
                if(romptr == jmp.hiptr)
                {
                    printf("\t");
                    PrintRomAddress(romptr);
                    unsigned targetptr = ReadPointerValueAt(jmp);
                    
                    printf("  %02X%*s.byte (", ROM[romptr], -11,":");
                    PrintAddressName(targetptr);
                    if(jmp.offset) printf(" %+d", -jmp.offset);
                    printf(").hi\n");
                    romptr += 1;
                    continue;
                }
                if(romptr == jmp.loptr)
                {
                    printf("\t");
                    PrintRomAddress(romptr);
                    unsigned targetptr = ReadPointerValueAt(jmp);
                    
                    if(jmp.hiptr == jmp.loptr+1)
                    {
                        printf("  %02X %02X%*s.word (", ROM[romptr], ROM[romptr+1], -8,":");
                        
                        if(targetptr < 0x800)
                            PrintRAMaddress(targetptr, 2);
                        else
                            PrintAddressName(targetptr);
                        
                        jmp.LoadMemMaps();
                        if(jmp.offset) printf(" %+d", -jmp.offset);
                        
                        unsigned jmpromptr=0;
                          try { jmpromptr=addr_to_rom(targetptr); }
                          catch(const BadAddressException& ) { }
                        
                        printf(") ;%X (%X) (%d)\n", targetptr, jmpromptr, jmp.page45);
                        
                        //printf(")\n");
                        romptr += 2;
                        continue;
                    }
                    printf("  %02X%*s.byte (", ROM[romptr], -11,":");
                    PrintAddressName(targetptr);
                    if(jmp.offset) printf(" %+d", -jmp.offset);
                    printf(").lo\n");
                    romptr += 1;
                    continue;
                }
            }
            
            if(IsDataType(type))
            {
                unsigned a=0;
                
                std::vector<unsigned char> ok_bytes;
                ok_bytes.reserve(32);
                
                unsigned stride = results[romptr].ArraySize;
                if(stride <= 1) stride = 16;
                
                while(/*a<linelen &&*/ (romptr+a) < results.size()
                    && IsDataType(results[romptr+a].Type)
                    && (a==0 || results[romptr+a].Labels.empty()))
                {
                    ok_bytes.push_back(ROM[romptr+a]);
                    ++a;
                }
                
                while(!ok_bytes.empty())
                {
                    printf("\t");
                    PrintRomAddress(romptr);
                    
                    unsigned linelen = stride;
                    while(linelen*2 <= 16) linelen *= 2;
                    unsigned remain = std::min((size_t)linelen, ok_bytes.size());
                    if(remain < stride) stride = 1;
                    
                    if(stride == 2)
                    {
                        printf("  %*s.word ", 13,"");
                        for(unsigned a=0; a<remain; a += 2)
                        {
                            if(a > 0) printf(",");
                            unsigned val = ok_bytes[a] + ok_bytes[a+1]*256;
                            printf("$%04X", val);
                        }
                        remain &= ~1;
                    }
                    else
                    {
                        printf("  %*s.byte ", 13,"");
                        for(unsigned a=0; a<remain; a += 1)
                        {
                            if(a > 0) printf(",");
                            if(a > 0 && stride > 1 && (a%stride)==0) printf(" ");
                            printf("$%02X", ok_bytes[a]);
                        }
                    }
                    printf("\n");
                    ok_bytes.erase(ok_bytes.begin(), ok_bytes.begin() + remain);
                    romptr += remain;
                }
                continue;
            }
            
            goto NextByte;
        }
    }
private:
    void MarkSomethingTable(unsigned loptr,unsigned hiptr,unsigned stepping,unsigned extent,
                            int offset,
                            const std::string& tabletype, bool is_default_name,
                            void (Disassembler::*Installer) (const PointerTableItem& ),
                            int page45 = -1)
    {
        bool first=true;
        while(first || extent > 0)
        {
            PointerTableItem i;
            i.loptr = loptr;
            i.hiptr = hiptr;
            i.stepping = stepping;
            i.final = extent != 0;
            i.offset=offset;
            i.page45 = page45;
            
            i.nametemplate    = tabletype;
            i.is_default_name = is_default_name;
            
            (this->*Installer)(i);
            
            if(first)
            {
                first=false;
                
                if(extent == 0 || hiptr==loptr+1)
                {
                    char Name[512];
                    
                    if(!HasNonShortLabel(i.loptr))
                    {
                        sprintf(Name, is_default_name ? "_%s_%04X"   : "%s",
                            tabletype.c_str(),
                            i.loptr);
                        results[i.loptr].Labels.insert(Name);
                    }
                    if(!HasNonShortLabel(i.hiptr))
                    {
                        sprintf(Name, is_default_name ? "_%s_%04X+1" : "%s+1",
                            tabletype.c_str(),
                            i.loptr);
                        results[i.hiptr].Labels.insert(Name);
                    }
                }
                else
                {
                    char Name[512];
                    if(!HasNonShortLabel(i.loptr))
                    {
                        sprintf(Name, is_default_name ? "_%sLo_%04X" : "%s_lo", tabletype.c_str(), i.loptr);
                        results[i.loptr].Labels.insert(Name);
                    }
                    if(!HasNonShortLabel(i.hiptr))
                    {
                        sprintf(Name, is_default_name ? "_%sHi_%04X" : "%s_hi", tabletype.c_str(), i.loptr);
                        results[i.hiptr].Labels.insert(Name);
                    }
                }
            }
            
            loptr += stepping;
            hiptr += stepping;
            if(extent > 0) --extent;
        }
    }
public:
    #define DeclareMarkingFuncs(funname, installername, defaultname) \
        void funname(unsigned loptr,unsigned hiptr,unsigned stepping,unsigned extent, int offset=0, int page45=-1) \
        { \
            if(CertaintyOf(results[loptr].Type) <= CertaintyOf(MaybeData)) \
                MarkSomethingTable(loptr,hiptr,stepping,extent,offset, defaultname,true, \
                                   &Disassembler::installername, page45); \
        } \
        void funname(unsigned loptr,unsigned hiptr,unsigned stepping,unsigned extent, const std::string& name, int offset=0, int page45=-1) \
        { \
            if(CertaintyOf(results[loptr].Type) <= CertaintyOf(MaybeData)) \
                MarkSomethingTable(loptr,hiptr,stepping,extent,offset, name,false, \
                                   &Disassembler::installername, page45); \
        } \
    
    DeclareMarkingFuncs(MarkJumpTable,     InstallJumpPointerTableEntry, "JumpPointerTable")
    DeclareMarkingFuncs(MarkDataTable,     InstallDataPointerTableEntry, "DataPointerTable")
    DeclareMarkingFuncs(MarkJumpJumpTable, InstallJumpJumpPointerTableEntry, "JumpJumpPointerTable")
    DeclareMarkingFuncs(MarkDataDataTable, InstallDataDataPointerTableEntry, "DataDataPointerTable")
    
    #undef DeclareMarkingFuncs

private:    

    bool IsSpecialType(const unsigned romptr, SpecialTypes specialtype) const
    {                                                    
        return results[romptr].SpecialType == specialtype;
    }                                                
    bool IsJumpTableRoutineWithAppendix(const unsigned romptr) const
    {                                                
        return IsSpecialType(romptr, JumpTableRoutineWithAppendix);
    }    
    bool IsTerminatedStringRoutine(const unsigned romptr, unsigned& param) const
    {
        bool res = IsSpecialType(romptr, TerminatedStringRoutine);
        if(res)
        {
            param = results[romptr].SpecialTypeParam;
        }
        return res;
    }    
    bool IsDataTableRoutineWithXY(const unsigned romptr) const
    {    
        return IsSpecialType(romptr, DataTableRoutineWithXY);
    }
    bool IsMapperChangeRoutine(const unsigned romptr, const SimulCPU& cpu,
                               SimulReg& result) const
    {
        if(IsSpecialType(romptr, MapperChangeRoutineWithReg))
        {
            switch(results[romptr].SpecialTypeParam)
            {
                case 0: result.Assign(cpu.A); break;
                case 1: result.Assign(cpu.X); break;
                case 2: result.Assign(cpu.Y); break;
            }
            return true;
        }
        if(IsSpecialType(romptr, MapperChangeRoutineWithConst))
        {
            result.Assign(results[romptr].SpecialTypeParam);
            return true;
        }
        if(IsSpecialType(romptr, MapperChangeRoutineWithRAM))
        {
            unsigned addr = results[romptr].SpecialTypeParam;
            try {
                result.Assign(cpu.ReadRAM(addr));
            } catch(ValueNotKnownException) {
            }
            return true;
        }
        return false;
    }
    
    bool MarkAddressMaybeData(unsigned rom_address, bool was_indexed, int page45 = -1)
    {
        bool retval = Mark(rom_address, MaybeData, false, page45);
        results[rom_address].is_referred |= was_indexed ? 2 : 4;
        return retval;
    }
    
    bool MarkAddressMaybeData(unsigned rom_address, bool was_indexed, const State& state)
    {
        int page45 = state.cpu.GuessPage45();
        bool result = MarkAddressMaybeData(rom_address, was_indexed, page45);
        results[rom_address].cpu.Combine(state.cpu);
        return result;
    }
    
    bool MarkAddressMaybeCode(unsigned rom_address, const State& state)
    {
        bool result = Mark(rom_address, MaybeCode, true, state.cpu.GuessPage45());
        results[rom_address].cpu.Combine(state.cpu);
        return result;
    }
    
    void ProcessCode(const unsigned romptr)
    {
        State& state = results[romptr];
        
        unsigned addrptr = rom_to_addr(romptr);
        
        state.cpu.ImportMap(((addrptr >> 13)&3)  , true);
        state.cpu.ImportMap(((addrptr >> 13)&3)^1, true);
        state.cpu.LoadMap();

        /*printf("Processing %05X (%04X) (", romptr,addrptr);
        printf("%u:", Page0); state.cpu.pagereg[0].Dump();
        printf(",%u:", Page1); state.cpu.pagereg[1].Dump();
        printf(",%u:", Page2); state.cpu.pagereg[2].Dump();
        printf(",%u:", Page3); state.cpu.pagereg[3].Dump();
        printf(")\n");*/
        
        Disassembly& code = state.code;
        code = DAsm(addrptr);
        
        const unsigned NoWhere = 0x7FFFFFFF;
        unsigned Next   = romptr + code.Bytes;
        unsigned Branch = NoWhere;

        switch(code.Mode)
        {
            case Ab:
            case Ax:
            case Ay:
            case In:
                /* Access of a memory address. Do not allow STA/STX/STY. */
                if(code.OpCodeId != 44 //sta
                && code.OpCodeId != 45 //stx
                && code.OpCodeId != 46)//sty
                {
                    /* If it's a read from a ROM address */
                    if((code.Param >= 0x8000 && code.Param < 0xA000 && state.cpu.pagereg[0].Known())
                    || (code.Param >= 0xA000 && code.Param < 0xC000 && state.cpu.pagereg[1].Known())
                    || (code.Param >= 0xC000 && code.Param < 0xE000 && state.cpu.pagereg[2].Known())
                    || (code.Param >= 0xE000 && code.Param <0x10000 && state.cpu.pagereg[3].Known())
                      )
                    {
                        /* It's almost certainly data */
                        unsigned param_romptr = addr_to_rom(code.Param);
                        
                        if(code.Mode != Ax && code.Mode != Ay)
                        MarkAddressMaybeData(param_romptr, code.Mode==Ax || code.Mode==Ay,
                            state);
                    }
                }
                break;
            default: ;
        }
        
        if(code.Mode == Iy) // y-indexed data access
        {
            unsigned Pointer = code.Param;
            
            if(Pointer+1 < TRACK_RAM_SIZE)
            {
                const SimulReg& lobyte = state.cpu.RAM[Pointer+0].GetRef();
                const SimulReg& hibyte = state.cpu.RAM[Pointer+1].GetRef();
                
                /* If there was a pointer that was poked into RAM */
                
                unsigned loptr = lobyte.IsIndexed();
                unsigned hiptr = hibyte.IsIndexed();

                if(loptr && hiptr && lobyte.GetIndexType() == hibyte.GetIndexType())
                {
                    unsigned extent=0, stepping=0;
                    if(hiptr == loptr+1)
                    {
                        extent = 0; stepping = 2;
                    }
                    else if(hiptr > loptr)
                    {
                        extent   = hiptr-loptr;
                        stepping = 1;
                    }
                    else if(loptr > hiptr)
                    {
                        extent   = loptr-hiptr;
                        stepping = 1;
                    }
                    if(stepping)
                    {
                        printf("; Discovered a data table at %X,%X (stepping %u, extent %u)\n",
                            loptr,hiptr, stepping,extent);
                        MarkDataTable(loptr,hiptr,stepping,extent);
                    }
                }
            }
        } // end y-indexed data access

        if(code.Mode == In && code.OpCodeId == 27) // Indirect jump
        {
            // Indirect jump
            Mark(Next, Unknown);
            Next = NoWhere;
            
            unsigned JumpPointer = code.Param;
            
            bool resolved = false;
            printf("; Indirect jump at romptr=$%X, JumpPointer=$%04X\n", romptr, JumpPointer);
            if(JumpPointer+1 < TRACK_RAM_SIZE)
            {
                const SimulReg& lobyte = state.cpu.RAM[JumpPointer+0].GetRef();
                const SimulReg& hibyte = state.cpu.RAM[JumpPointer+1].GetRef();
                
                unsigned loptr = lobyte.IsIndexed();
                unsigned hiptr = hibyte.IsIndexed();

                if(loptr && hiptr && lobyte.GetIndexType() == hibyte.GetIndexType())
                {
                    unsigned extent=0, stepping=0;
                    if(hiptr == loptr+1)
                    {
                        extent = 0; stepping = 2;
                    }
                    else if(hiptr > loptr)
                    {
                        extent   = hiptr-loptr;
                        stepping = 1;
                    }
                    else if(loptr > hiptr)
                    {
                        extent   = loptr-hiptr;
                        stepping = 1;
                    }
                    if(stepping)
                    {
                        printf("; Discovered a jump table at %X,%X (stepping %u, extent %u)\n",
                            loptr,hiptr, stepping,extent);
                        resolved = true;
                        
                        MarkJumpTable(loptr,hiptr,stepping,extent);
                    }
                }
            }
            if(!resolved) printf("; UNRESOLVED indirect jump!\n");
        } // end indirect jump
        
        #define Arithmetic_UpdateFlags(value) \
            do { \
                unsigned char vval = (value); \
                state.cpu.Zflag.Assign(vval == 0, romptr); \
                state.cpu.Sflag.Assign(vval & 0x80, romptr); \
            } while(0)
        #define Arithmetic_Invalidate(reg) \
            do { \
                state.cpu.Zflag.Invalidate(); \
                state.cpu.Sflag.Invalidate(); \
                state.cpu.reg.Invalidate(); \
            } while(0)
        #define Arithmetic_Assign(reg, value) \
            do { \
                unsigned char val = (value); \
                Arithmetic_UpdateFlags(val); \
                state.cpu.reg.Assign(val, romptr); \
            } while(0)
        #define Arithmetic_Copy(reg, source) \
            do { \
                if(state.cpu.source.Known()) \
                    Arithmetic_Assign(reg, state.cpu.source.Value()); \
                else \
                    state.cpu.reg.Assign(state.cpu.source.GetRef()); \
            } while(0)
        
        switch(code.OpCodeId)
        {
            case 127: // non-opcode
            case 10: //brk
            case 41: //rti
            case 42: //rts
                //printf("rts $%X (stack size %u)\n", romptr, state.cpu.Stack.size());
                Mark(Next, Unknown);
                Next = NoWhere;
                state.barrier = true;
                break;
            case 27: //jmp
            {
                if(code.Mode != Iw) break;
                
                if((code.Param >= 0x8000 && code.Param < 0xA000 && state.cpu.pagereg[0].Known())
                || (code.Param >= 0xA000 && code.Param < 0xC000 && state.cpu.pagereg[1].Known())
                || (code.Param >= 0xC000 && code.Param < 0xE000 && state.cpu.pagereg[2].Known())
                || (code.Param >= 0xE000 && code.Param <0x10000 && state.cpu.pagereg[3].Known())
                  )
                {
                    Branch = addr_to_rom(code.Param);
                    Mark(Branch, CertainlyCode, true);
                    results[Branch].cpu.Combine(state.cpu);
                    results[Branch].JumpedFromInsert(romptr);
                    state.JumpsTo = Branch;
                }
                //Next = Unknown;
                state.barrier = true;
                break;
            }
            case 28: //jsr
            {
            /*
                - attempted Solomon's Key hack
                if(code.Param == 0x9A4B)
                {
                    try
                    {
                        unsigned addr = state.cpu.ReadRAM(0x00) | (state.cpu.ReadRAM(0x01) << 8);
                        Mark(addr_to_rom(addr), MaybeCode, true);
                    }
                    catch(const ValueNotKnownException &)
                    {
                    }
                    catch(const BadAddressException &)
                    {
                    }
                }
            */
                if((code.Param >= 0x8000 && code.Param < 0xA000 && state.cpu.pagereg[0].Known())
                || (code.Param >= 0xA000 && code.Param < 0xC000 && state.cpu.pagereg[1].Known())
                || (code.Param >= 0xC000 && code.Param < 0xE000 && state.cpu.pagereg[2].Known())
                || (code.Param >= 0xE000 && code.Param <0x10000 && state.cpu.pagereg[3].Known())
                  )
                {
                    Branch = addr_to_rom(code.Param);
                    
                    results[Branch].CalledFrom.insert(romptr);
                    
                    if(results[Branch].Type == 50) // not visited, so just copy our state
                    {
                        results[Branch].cpu.Combine(state.cpu);
                    }

                    Mark(Branch, CertainlyCode, true);
                    results[Branch].cpu.Combine(state.cpu);
                    
                    if(IsJumpTableRoutineWithAppendix(Branch))
                    {
                        MarkJumpTable(Next+0, Next+1, 2, 0);
                        break;
                    }
                    
                    { unsigned param;
                    if(IsTerminatedStringRoutine(Branch, param))
                    {
                        int width      = param / 256;
                        int terminator = param % 256;
                        
                        Mark(Next, CertainlyData);
                        
                        unsigned c = Next;
                        while(ROM[c] != terminator) { results[c].ArraySize = width; c += width; }
                        c += 1; // skip terminator
                        Mark(c, CertainlyCode);
                    } }
                    
                    if(IsDataTableRoutineWithXY(Branch))
                    {
                        if(state.cpu.X.Known() && state.cpu.Y.Known())
                        {
                            unsigned addr = state.cpu.X.Value() + state.cpu.Y.Value()*256;
                            if(addr >= 0x8000)
                            {
                                unsigned jt = addr_to_rom(addr);
                                MarkAddressMaybeData(jt, true, state);
                                //printf("At jt %04X: jumptable=%04X\n", romptr, jt);
                                
                                int x_at = state.cpu.X.GetDefineLocation();
                                if(x_at >= 0)
                                    { results[x_at].referred_address = addr;
                                      results[x_at].referred_byte = State::lo;
                                      results[x_at].referred_offset = 0;
                                      results[x_at].meaning_interpreted=true;
                                    }
                                int y_at = state.cpu.Y.GetDefineLocation();
                                if(y_at >= 0)
                                    { results[y_at].referred_address = addr;
                                      results[y_at].referred_byte = State::hi;
                                      results[x_at].referred_offset = 0;
                                      results[y_at].meaning_interpreted=true;
                                    }
                           }
                        }
                    }
                    
                    SimulReg tmp;
                    if(IsMapperChangeRoutine(Branch, state.cpu, tmp))
                    {
                        printf("Call to $%X: Reprogramming mapper with ", Branch);
                        tmp.Dump();
                        printf("\n");
                        state.cpu.MapperProgram(tmp);
                    }
                }
                else
                {
                    printf("Call to $%X: Unknown target\n", code.Param);
                }
                
                Mark(Next, MaybeCode); // there's no guarantee that the jsr will return
                results[Next].cpu.Combine(state.cpu);
                state.cpu.Invalidate(); // a function may change any registers
                break;
            }
            
            case 3: case 5: case 7: case 11:
            case 4: case 8: case 9: case 12:
                // conditional jumps (bcc,beq,bmi,bvc,
                //                    bcs,bne,bpl,bvs)
            {
                Branch = addr_to_rom(code.Param);

                if(results[Branch].Type == 50) // not visited, so just copy our state
                {
                    results[Branch].cpu.Combine(state.cpu);
                }
                Mark(Branch, CertainlyCode, true);
                
                if((code.OpCodeId == 5 && state.cpu.Zflag.Known() &&  state.cpu.Zflag.Value()) // beq
                || (code.OpCodeId == 8 && state.cpu.Zflag.Known() && !state.cpu.Zflag.Value()) // bne
                || (code.OpCodeId == 7 && state.cpu.Sflag.Known() &&  state.cpu.Sflag.Value()) // bmi
                || (code.OpCodeId == 9 && state.cpu.Sflag.Known() && !state.cpu.Sflag.Value()) // bpl
                  )
                {
                    // flag is always true
                    //next = Unknown;
                    state.barrier = true;
                }
                else
                {
                    Mark(Next, MaybeCode);
                }

                results[Branch].JumpedFromInsert(romptr);
                state.JumpsTo = Branch;
                break;
            }
            case 0: // adc
            {
                /* Handle X,Y index in ADC because it's sometimes
                 * used to setup a calculated goto
                 */
                Arithmetic_Invalidate(A);
                switch(code.Mode)
                {
                    case Zx://passthru
                    case Ax:
                        if(code.Param >= 0x8000) state.cpu.A.AssignXindex(addr_to_rom(code.Param));
                        goto CodeContinues;
                    case Zy://passthru
                    case Ay:
                        if(code.Param >= 0x8000) state.cpu.A.AssignYindex(addr_to_rom(code.Param));
                        goto CodeContinues;
                    default: break;
                }
                goto CodeContinues;
            }
            case 1: // and
            case 2: // asl
            case 6: // bit (?)
            case 20: // dec
            case 25: // eor
            case 33: // lsr
            case 34: // ora
            case 39: // rol
            case 40: // ror
            case 43: // sbc
                Arithmetic_Invalidate(A);
                goto CodeContinues;
            case 21: // dex
            case 23: // inx
                /*if(state.cpu.X.Known())
                    Arithmetic_Assign(X, (state.cpu.X.Value() + (code.OpCodeId==23 ? 1 : -1)));
                else*/
                    Arithmetic_Invalidate(X);
                goto CodeContinues;
            case 22: // dey
            case 24: // iny
                /*if(state.cpu.Y.Known())
                    Arithmetic_Assign(Y, (state.cpu.Y.Value() + (code.OpCodeId==24 ? 1 : -1)));
                else*/
                    Arithmetic_Invalidate(Y);

            // REGISTER LOADS
            
            case 29: // lda
                Arithmetic_Invalidate(A);
                switch(code.Mode)
                {
                    case Im: Arithmetic_Assign(A, code.Param); break;
                    case Zx://passthru
                    case Ax:
                        if(code.Param >= 0x8000) state.cpu.A.AssignXindex(addr_to_rom(code.Param), romptr);
                        break;
                    case Zy://passthru
                    case Ay:
                        if(code.Param >= 0x8000) state.cpu.A.AssignYindex(addr_to_rom(code.Param), romptr);
                        break;
                    case Zp://passthru
                    case Ab:
                        if(code.Param >= 0x8000)
                        {
                            Arithmetic_Assign(A, BYTE(code.Param));
                            break;
                        }
                        else if(code.Param < TRACK_RAM_SIZE)
                        {
                            //Arithmetic_Copy(A, RAM[code.Param]);
                            break;
                        }
                        //passthru
                    default: state.cpu.A.Invalidate(); break;
                }
                goto CodeContinues;
            case 31: // ldx
                Arithmetic_Invalidate(X);
                switch(code.Mode)
                {
                    case Im: Arithmetic_Assign(X, code.Param); break;
                    case Zx://passthru
                    case Ax:
                        if(code.Param >= 0x8000) state.cpu.X.AssignXindex(addr_to_rom(code.Param), romptr);
                        break;
                    case Zy://passthru
                    case Ay:
                        if(code.Param >= 0x8000) state.cpu.X.AssignYindex(addr_to_rom(code.Param), romptr);
                        break;
                    case Zp://passthru
                    case Ab:
                        if(code.Param >= 0x8000)
                        {
                            Arithmetic_Assign(X, BYTE(code.Param));
                            break;
                        }
                        else if(code.Param < TRACK_RAM_SIZE)
                        {
                            //Arithmetic_Copy(X, RAM[code.Param]);
                            break;
                        }
                        //passthru
                    default: state.cpu.X.Invalidate(); break;
                }
                goto CodeContinues;
            case 32: // ldy
                Arithmetic_Invalidate(Y);
                switch(code.Mode)
                {
                    case Im: Arithmetic_Assign(Y, code.Param); break;
                    case Zx://passthru
                    case Ax:
                        if(code.Param >= 0x8000) state.cpu.Y.AssignXindex(addr_to_rom(code.Param), romptr);
                        break;
                    case Zy://passthru
                    case Ay:
                        if(code.Param >= 0x8000) state.cpu.Y.AssignYindex(addr_to_rom(code.Param), romptr);
                        break;
                    case Zp://passthru
                    case Ab:
                        if(code.Param >= 0x8000)
                        {
                            Arithmetic_Assign(Y, BYTE(code.Param));
                            break;
                        }
                        else if(code.Param < TRACK_RAM_SIZE)
                        {
                            //Arithmetic_Copy(Y, RAM[code.Param]);
                            break;
                        }
                        //passthru
                    default: state.cpu.Y.Invalidate(); break;
                }
                goto CodeContinues;

            // REGISTER STORES
            
            case 44: // sta
                switch(code.Mode)
                {
                    case Zp://passthru
                    case Ab:
                        if(code.Param < TRACK_RAM_SIZE)
                        {
                            state.cpu.RAM[code.Param].Assign(state.cpu.A);
                            break;
                        }
                        if(code.Param >= 0x8000)
                        {
                            state.cpu.MapperWrite(code.Param, state.cpu.A);
                            break;
                        }
                    default: ;
                }
                goto CodeContinues;
            case 45: // stx
                switch(code.Mode)
                {
                    case Zp://passthru
                    case Ab:
                        if(code.Param < TRACK_RAM_SIZE)
                        {
                            state.cpu.RAM[code.Param].Assign(state.cpu.X);
                            break;
                        }
                        if(code.Param >= 0x8000)
                        {
                            state.cpu.MapperWrite(code.Param, state.cpu.X);
                            break;
                        }
                    default: ;
                }
                goto CodeContinues;
            case 46: // sty
                switch(code.Mode)
                {
                    case Zp://passthru
                    case Ab:
                        if(code.Param < TRACK_RAM_SIZE)
                        {
                            state.cpu.RAM[code.Param].Assign(state.cpu.Y);
                            break;
                        }
                        if(code.Param >= 0x8000)
                        {
                            state.cpu.MapperWrite(code.Param, state.cpu.Y);
                            break;
                        }
                    default: ;
                }
                goto CodeContinues;

            case 35: // pha
                state.cpu.Push(state.cpu.A);
                goto CodeContinues;
            case 36: // php
                state.cpu.Push();
                goto CodeContinues;
            case 37: // pla
                Arithmetic_Invalidate(A);
                state.cpu.Pop(state.cpu.A);
                if(state.cpu.A.Known()) Arithmetic_UpdateFlags(state.cpu.A.Value());
                goto CodeContinues;
            case 38: // plp
                state.cpu.Pop();
                state.cpu.Zflag.Invalidate();
                state.cpu.Sflag.Invalidate();
                goto CodeContinues;
            case 50: // tax
                Arithmetic_Copy(X, A);
                goto CodeContinues;
            case 51: // tay
                Arithmetic_Copy(Y, A);
                goto CodeContinues;
            case 52: // txa
                Arithmetic_Copy(A, X);
                goto CodeContinues;
            case 53: // tya
                Arithmetic_Copy(A, Y);
                goto CodeContinues;
            case 54: // tsx
                state.cpu.X.Invalidate();
                state.cpu.Zflag.Invalidate();
                state.cpu.Sflag.Invalidate();
                goto CodeContinues;
            case 55: // txs
                state.cpu.Stack.clear();
                state.cpu.Zflag.Invalidate();
                state.cpu.Sflag.Invalidate();
                goto CodeContinues;
            case 17: case 18: case 19: // cmp, cpx, cpy
            case 26: //inc (no registers)
                state.cpu.Zflag.Invalidate();
                state.cpu.Sflag.Invalidate();
                goto CodeContinues;
            case 13: //clc
            case 14: //cld
            case 15: //cli
            case 16: //clv
            case 30: //nop
            case 47: //sec
            case 48: //sed
            case 49: //sei
                goto CodeContinues;
            
            default:
                // Everything else invalidates the whole CPU
                printf("Default: %u\n", code.OpCodeId);
                state.cpu.Invalidate();
                goto CodeContinues;
            CodeContinues:
                Mark(Next, CertainlyCode);
        }
        
        if(Next   != NoWhere)
        {
            //printf("Combining $%X (next) from $%X, Result: ", Next, romptr);
            results[Next].cpu.Combine(state.cpu);
            //results[Next].cpu.Dump(); printf("\n");
        }
        if(Branch != NoWhere)
        {
            //printf("Combining $%X (branch) from $%X, Result: ", Branch, romptr);
            results[Branch].cpu.Combine(state.cpu);
            //results[Branch].cpu.Dump(); printf("\n");
        }
        
        /* Mark the rest of the opcode as PartialCode, but don't
         * add it to the visit list
         */
        for(unsigned a=1; a<code.Bytes; ++a)
        {
            results[romptr+a].Type = PartialCode;
        }
    }
    void SortVisitList()
    {
        std::sort(VisitList.begin(), VisitList.end(), CompareCodeLikelihood(results));
    }

public:
    bool Mark(unsigned romptr, CodeLikelihood type,
              bool is_referred = false,
              int page45 = -1)
    {
        int certainty = CertaintyOf(type);
        
        //printf("Mark %05X, %d\n", romptr,type);
        CodeLikelihood oldtype = results[romptr].Type;
        int oldcertainty = CertaintyOf(oldtype);
        
        if(certainty >= oldcertainty)
        {
            /*
            if(type != oldtype && oldtype != 50)
            {
                printf("Replacing type %d with type %d at $%05X\n",
                    oldtype, type, romptr);
            }
            */
            results[romptr].Type = type;
            
            // if the referrer knows some mappings, copy them to the referred
            if(page45 >= 0)
            {
                results[romptr].cpu.SetPageReg(0, page45*2    , true);
                results[romptr].cpu.SetPageReg(1, page45*2 + 1, true);
            }
            
            if(is_referred) results[romptr].is_referred |= 1;
        }
        
        /* If not visited, put this to the visit list. */
        if(certainty != 0
        && IsVisitWorthy(type)
        && Visited.find(romptr) == Visited.end())
        {
            VisitList.push_front(romptr);
            //fprintf(stderr, "VisitList; pushing %X\n", romptr);
            return true;
        }
        return false;
    }
    void SetArraySize(unsigned romptr, unsigned size)
    {
        results[romptr].ArraySize = size;
    }
    
    bool Mark(unsigned romptr, const std::string& name, CodeLikelihood type,
              bool is_referred = false,
              int page45 = -1)
    {
        //printf("Mark %05X, %d, name %s\n", romptr,type, name.c_str());

        if(romptr >= results.size())
        {
            std::fprintf(stderr, "Error: romptr=%X, results.size()=%X\n",
                romptr, (unsigned)results.size());
        }
        
        if(!HasNonShortLabel(romptr)) /* FIXME: do this check only if the name was autogenerated */
        {
            results[romptr].Labels.insert(name);
        }
        
        return Mark(romptr, type, is_referred, page45);
    }

    unsigned ReadPointerValueAt(const PointerTableItem& i) const
    {
        i.LoadMemMaps();
        
        unsigned ptrlo = ROM[i.loptr];
        unsigned ptrhi = ROM[i.hiptr];
        unsigned addr = ptrlo | (ptrhi << 8);
        return addr + i.offset;
    }
    
    void InstallSomethingPointerTableEntry
        (const PointerTableItem& i,
         CodeLikelihood UsedPtrType,
         CodeLikelihood UnusedPtrType,
         const std::string& ptrtypename,
         void (Disassembler::*Installer)
               (unsigned romptr,
                const std::string& name,
                const PointerTableItem& i)
        )
    {
        /* i informs about the address that points into this table entry */
        
        if(i.loptr == 0x4128 && ptrtypename == "DataTableEntry")
        {
            abort();
        }
    
       /*
        printf("Installing %s %X,%X (%s)\n",
            ptrtypename.c_str(),
            i.loptr, i.hiptr, i.final ? "final" : "guessing");
       */
        if(i.loptr >= results.size()
        || i.hiptr >= results.size())
        {
            std::fprintf(stderr, "%s refers outside the ROM space (%X,%X)\n",
                ptrtypename.c_str(), i.loptr, i.hiptr);
            return;
        }
        
        unsigned pointertarget = ReadPointerValueAt(i);
        try
        {
            unsigned romptr = addr_to_rom(pointertarget);
            char Buf[512];
            sprintf(Buf, "_%04X", romptr);
            (this->*Installer)(romptr, ptrtypename + Buf, i);
        }
        catch(const BadAddressException& )
        {
        }
        
        results[i.loptr].Type = UsedPtrType; results[i.loptr].PtrAddr = i;
        results[i.hiptr].Type = UsedPtrType; results[i.hiptr].PtrAddr = i;

        if(!i.final)
        {
            // Go one step forward!
            PointerTableItem next_pointer = i;
            next_pointer.loptr += i.stepping;
            next_pointer.hiptr += i.stepping;
            
            if(results[next_pointer.loptr].Type == Unknown
            && results[next_pointer.hiptr].Type == Unknown)
            {
                Mark(next_pointer.loptr, UnusedPtrType);
                Mark(next_pointer.hiptr, UsedPtrType);
                
                results[next_pointer.loptr].PtrAddr = next_pointer;
                results[next_pointer.hiptr].PtrAddr = next_pointer;
            }
        }
    }
    
    void InstallJumpPointer(unsigned romptr) { Mark(romptr, CertainlyCode, true); }
    void InstallJumpPointer(unsigned romptr, const std::string& name, const PointerTableItem& i)
        { Mark(romptr, name, CertainlyCode, true, i.page45); }
    void InstallJumpPointerTableEntry(const PointerTableItem& i)
    {
        const std::string name = i.is_default_name ? "JumpTableEntry" : i.nametemplate;
        InstallSomethingPointerTableEntry(i, UsedJumpPtr,UnusedJumpPtr, name, &Disassembler::InstallJumpPointer);
    }
    
    void InstallDataPointer(unsigned romptr) { Mark(romptr, MaybeData, true); }
    void InstallDataPointer(unsigned romptr, const std::string& name, const PointerTableItem& i)
        { Mark(romptr, name, MaybeData, true, i.page45); }
    void InstallDataPointerTableEntry(const PointerTableItem& i)
    {
        const std::string name = i.is_default_name ? "DataTableEntry" : i.nametemplate;
        InstallSomethingPointerTableEntry(i, UsedDataPtr,UnusedDataPtr, name, &Disassembler::InstallDataPointer);
    }


    void InstallJumpJumpPointer(unsigned romptr) { MarkJumpTable(romptr+0,romptr+1,2,0, +1); }
    void InstallJumpJumpPointer(unsigned romptr, const std::string& name, const PointerTableItem& i)
        { MarkJumpTable(romptr+0,romptr+1,2,0, name, +1, i.page45); }
    void InstallJumpJumpPointerTableEntry(const PointerTableItem& i)
    {
        const std::string name = i.is_default_name ? "JumpJumpTableEntry" : i.nametemplate;
        InstallSomethingPointerTableEntry(i, UsedJumpJumpPtr,UnusedJumpJumpPtr, name, &Disassembler::InstallJumpJumpPointer);
    }
    
    void InstallDataDataPointer(unsigned romptr) { MarkDataTable(romptr+0,romptr+1,2,0); }
    void InstallDataDataPointer(unsigned romptr, const std::string& name, const PointerTableItem& i)
        { MarkDataTable(romptr+0,romptr+1,2,0, name, 0, i.page45); }
    void InstallDataDataPointerTableEntry(const PointerTableItem& i)
    {
        const std::string name = i.is_default_name ? "DataDataTableEntry" : i.nametemplate;
        InstallSomethingPointerTableEntry(i, UsedDataDataPtr,UnusedDataDataPtr, name, &Disassembler::InstallDataDataPointer);
    }
    
    void SetSpecialType(unsigned romptr, SpecialTypes type)
    {
        results[romptr].SpecialType = type;
    }
    void SetSpecialType(unsigned romptr, SpecialTypes type, unsigned param)
    {
        results[romptr].SpecialType      = type;
        results[romptr].SpecialTypeParam = param;
    }
    
private:
    std::vector<State> results; /* indexed by romptr */
    
    std::deque<unsigned> VisitList;
    std::set<unsigned> Visited;

    std::map<unsigned, std::string> RAMaddressNames;
};

static void DumpMappings()
{
    printf("Mappings:\n");
    for(unsigned a=4; a<8; ++a)
        printf(" Page %u: %X\n",
            a, (unsigned)(Pages[a]-ROM));
}
static void DumpVectors()
{
    printf("Vectors:\n");
    printf(" NMI:   %X\n", WORD(0xFFFA));
    printf(" Reset: %X\n", WORD(0xFFFC));
    printf(" IRQ:   %X\n", WORD(0xFFFE));
}

static const std::vector<std::string> Split
  (const std::string& text,
   char separator=' ',
   char quote='"',
   bool squish=true)
{
    std::vector<std::string> words;
    
    unsigned a=0, b=text.size(); 
    while(a<b)
    {
        if(text[a] == separator && squish) { ++a; continue; }
        if(text[a] == quote)
        {
            ++a;
            unsigned start = a;  
            while(a < b && text[a] != quote) ++a;
            words.push_back(text.substr(start, a-start));
            if(a < b) { ++a; /* skip quote */ }          
            continue;
        }            
        unsigned start = a;
        while(a<b && text[a] != separator) ++a;
        words.push_back(text.substr(start, a-start));
        if(!squish && a<b && text[a]==separator) ++a;
        continue;                                    
    }
    return words;
}

static void ParseINIfile(FILE* fp, Disassembler& dasm)
{
    if(!fp)return;
    while(fp)
    {
        char Buf[4096];
        if(!std::fgets(Buf,sizeof(Buf),fp)) break;
        std::strtok(Buf, "\r"); std::strtok(Buf, "\n");
        const char* ptr = Buf;
        while(*ptr == ' ') ++ptr;
        if(*ptr == '#' || !*ptr || *ptr == '\n') continue;
        std::vector<std::string> tokens = Split(ptr);
        
        #define ParseInt(str) \
            ({ __label__ IntErr; \
               if(str.empty()) \
               { IntErr: \
                 fprintf(stderr, "Invalid integer: %s\n", str.c_str()); goto SyntaxError; } \
               int result; \
               char* endptr = 0; \
               if(str[0] == '$') result = strtol(str.c_str()+1,&endptr,16); \
               else result = strtol(str.c_str(),&endptr,10); \
               if(*endptr) goto IntErr; \
               result; })
        
        CodeLikelihood type = Unknown;
        
        void (Disassembler::*MarkFun1)(unsigned lo,unsigned hi,unsigned step,unsigned len,int offset,int page45);
        void (Disassembler::*MarkFun2)(unsigned lo,unsigned hi,unsigned step,unsigned len,const std::string& name,int offset,int page45);
        
        if(tokens[0] == "CertainlyCode") { type = CertainlyCode; goto MarkCall; }
        if(tokens[0] == "MaybeCode")     { type = MaybeCode; goto MarkCall; }
        if(tokens[0] == "MaybeData")     { type = MaybeData; goto MarkCall; }
        if(tokens[0] == "CertainlyData") { type = CertainlyData; goto MarkCall; }
        
        if(tokens[0] == "RAM")
        {
            if(tokens.size() != 3) goto SyntaxError;
            int address = ParseInt(tokens[1]);
            dasm.DefineRAM(address, tokens[2]);
            continue;
        }
        if(tokens[0] == "DataTableRoutineWithXY")
        {
            if(tokens.size() != 2) goto SyntaxError;
            int address = ParseInt(tokens[1]);
            dasm.SetSpecialType(address, DataTableRoutineWithXY);
            continue;
        }
        if(tokens[0] == "TerminatedStringRoutine")
        {
            if(tokens.size() != 4) goto SyntaxError;
            int address    = ParseInt(tokens[1]);
            int width      = ParseInt(tokens[2]);
            int terminator = ParseInt(tokens[3]);
            
            int param = width*256 + terminator;
            
            dasm.SetSpecialType(address, TerminatedStringRoutine, param);
            continue;
        }
        if(tokens[0] == "JumpTableRoutineWithAppendix")
        {
            if(tokens.size() != 2) goto SyntaxError;
            int address = ParseInt(tokens[1]);
            dasm.SetSpecialType(address, JumpTableRoutineWithAppendix);
            continue;
        }
        if(tokens[0] == "MapperChangeRoutine")
        {
            if(tokens.size() != 4) goto SyntaxError;
            int address = ParseInt(tokens[1]);
            std::string type = tokens[2];
            if(type == "const")
            {
                int param   = ParseInt(tokens[3]);
                dasm.SetSpecialType(address, MapperChangeRoutineWithConst, param);
            }
            else if(type == "ram" || type == "RAM")
            {
                int param   = ParseInt(tokens[3]);
                dasm.SetSpecialType(address, MapperChangeRoutineWithRAM, param);
            }
            else if(type == "reg")
            {
                std::string reg = tokens[3];
                int regno = 0;
                if(reg == "A" || reg == "a") regno=0;
                else if(reg == "X" || reg == "x") regno=1;
                else if(reg == "Y" || reg == "y") regno=2;
                else fprintf(stderr, "Invalid register for MapperChangeRoutine: '%s'\n", reg.c_str());
                dasm.SetSpecialType(address, MapperChangeRoutineWithReg, regno);
            }
            else
                fprintf(stderr, "Invalid parameter type for MapperChangeRoutine: '%s'\n",
                    type.c_str());
            continue;
        }
        
        if(tokens[0] == "JumpTable") { MarkFun1 = &Disassembler::MarkJumpTable; MarkFun2 = &Disassembler::MarkJumpTable; goto MarkTable; }
        if(tokens[0] == "DataTable") { MarkFun1 = &Disassembler::MarkDataTable; MarkFun2 = &Disassembler::MarkDataTable; goto MarkTable; }
        if(tokens[0] == "JumpJumpTable") { MarkFun1 = &Disassembler::MarkJumpJumpTable; MarkFun2 = &Disassembler::MarkJumpJumpTable; goto MarkTable; }
        if(tokens[0] == "DataDataTable") { MarkFun1 = &Disassembler::MarkDataDataTable; MarkFun2 = &Disassembler::MarkDataDataTable; goto MarkTable; }
        
        goto SyntaxError;
        
        if(false)
        {
        MarkCall: ;
            // <type> <address> [name] [width]
            int address = ParseInt(tokens[1]);
            if(tokens.size() >= 3)
            {
                dasm.Mark(address, tokens[2], type);
                if(tokens.size() == 4)
                    dasm.SetArraySize(address, ParseInt(tokens[3]));
            }
            else if(tokens.size() == 2)
                dasm.Mark(address, type);
            else goto SyntaxError;
            continue;
        }
        
        if(false)
        {
            // name loptr hiptr step [length [name [offset [page45]]]]
            
        MarkTable:  
            if(tokens.size() < 5) goto SyntaxError;
            int lo = ParseInt(tokens[1]);
            int hi = ParseInt(tokens[2]);
            int step = ParseInt(tokens[3]);
            int ext  = ParseInt(tokens[4]);
            //fprintf(stderr, "lo=%X, hi=%X, step=%u, ext=%u\n",lo,hi,step,ext);
            if(tokens.size() == 5)
                (dasm.*MarkFun1)(lo,hi,step,ext, 0,-1);
            else if(tokens.size() == 6)
                (dasm.*MarkFun2)(lo,hi,step,ext, tokens[5], 0,-1);
            else if(tokens.size() == 7)
                (dasm.*MarkFun2)(lo,hi,step,ext, tokens[5], ParseInt(tokens[6]), -1);
            else if(tokens.size() == 8)
                (dasm.*MarkFun2)(lo,hi,step,ext, tokens[5], ParseInt(tokens[6]), ParseInt(tokens[7]));
            else goto SyntaxError;
            continue;
        }
        
        if(false)
        {
        SyntaxError:
            fprintf(stderr, "Syntax error in INI: %s\n", Buf);
        }
        
        #undef ParseInt
    }
}

static void DisAsm(unsigned char* Buf, unsigned size,
                   FILE* inifile = 0)
{
    unsigned NPages = size / 0x2000;

    printf("ROM is %p, %u bytes, %u 8k-pages\n", Buf, size, NPages);
    
    ROM      = Buf;
    LastPage = NPages-1;
    
    SetPage(4, Page0 = 0);
    SetPage(5, Page1 = 1);
    SetPage(6, Page2 = LastPage-1);
    SetPage(7, Page3 = LastPage);
    
    DumpMappings();
    DumpVectors();
    
    Disassembler dasm(size);
    
    if(inifile)
        ParseINIfile(inifile, dasm);
    
    try { dasm.Mark(addr_to_rom(WORD(0xFFFA)), "_NMI",   CertainlyCode); } catch(const BadAddressException&){}
    try { dasm.Mark(addr_to_rom(WORD(0xFFFC)), "_Reset", CertainlyCode); } catch(const BadAddressException&){}
    try { dasm.Mark(addr_to_rom(WORD(0xFFFE)), "_IRQ",   CertainlyCode); } catch(const BadAddressException&){}
    
    dasm.DiscoverRegions();
   // dasm.DumpNonCodeRanges();
   
    dasm.AssignMissingLabels();
    dasm.Dump();
}

int main(int argc, const char*const *argv)
{
    FILE* fp = fopen(argv[1], "rb");
    if(!fp)
    {
        perror(argv[1]);
    Usage:
        printf("Usage: clever_disasm <nesfile> [<inifile>]\n");
        return -1;
    }
    
    FILE* ini = argv[2] ? fopen(argv[2], "rb") : 0;
    if(argv[2] && !ini)
    {
        perror(argv[2]);
        goto Usage;
    }
    
    unsigned char hdr[16];
    fread(hdr, 1, sizeof(hdr), fp);
    
    if(hdr[6] & 4) fseek(fp, 512, SEEK_CUR);
    
    unsigned size = hdr[4] * 16384;
    
    unsigned char* Buf = new unsigned char[size];
    fread(Buf, 1, size, fp);
    fclose(fp);
    
    DisAsm(Buf, size, ini);
    
    delete[] Buf;
}
