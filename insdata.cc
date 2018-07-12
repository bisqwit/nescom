#include "insdata.hh"
#include "assemble.hh"

const struct AddrMode AddrModes[] =
{
    /* Sorted in priority order - but the no-parameters-type must come first! */

  /* num  example      reject prereq postreq p1-type         p2-type */
  { /* 0  nop          */   0, "",   "",   AddrMode::tNone, AddrMode::tNone },//o      AC,IL
  { /* 1  rep #imm8    */   0, "#",  "",   AddrMode::tByte, AddrMode::tNone },//o #B   IM
  { /* 2  bcc rel8     */   0, "",   "",   AddrMode::tRel8, AddrMode::tNone },//o r    RL
  { /* 3  lda $10      */ '(', "",   "",   AddrMode::tByte, AddrMode::tNone },//o B    ZP
  { /* 4  lda $10,x    */ '(', "",   ",x", AddrMode::tByte, AddrMode::tNone },//o B,x  ZX
  { /* 5  lda $10,y    */ '(', "",   ",y", AddrMode::tByte, AddrMode::tNone },//o B,y  ZY
  { /* 6  lda ($10,x)  */   0, "(", ",x)", AddrMode::tByte, AddrMode::tNone },//o (B,x) IX
  { /* 7  lda ($10),y  */   0, "(", "),y", AddrMode::tByte, AddrMode::tNone },//o (B),y IY
  { /* 8  lda $1234    */ '(', "",   "",   AddrMode::tWord, AddrMode::tNone },//o W    AB
  { /* 9  lda $1234,x  */ '(', "",   ",x", AddrMode::tWord, AddrMode::tNone },//o W,x  AX
  { /* 10 lda $1234,y  */ '(', "",   ",y", AddrMode::tWord, AddrMode::tNone },//o W,y  AY
  { /* 11 lda ($1234)  */   0, "(",  ")",  AddrMode::tWord, AddrMode::tNone },//o (W)  IN
  { /* 12 .link group 1  */ 0, "group", "",AddrMode::tWord, AddrMode::tNone },
  { /* 13 .link page $FF */ 0, "page",  "",AddrMode::tByte, AddrMode::tNone },
  { /* 14 .nop imm16  */    0, "",   "",   AddrMode::tWord, AddrMode::tNone }
};
const unsigned AddrModeCount = sizeof(AddrModes) / sizeof(AddrModes[0]);

const struct ins ins[] =
{
    // IMPORTANT: Alphabetical sorting!

  { ".(",    "sb" }, // start block, no params
  { ".)",    "eb" }, // end block, no params
  { ".bss",  "gb" }, // Select seG BSS
  { ".data", "gd" }, // Select seG DATA
  { ".link",         // Select linkage (modes 12 and 13)
           "--'--'--'--'--'--'--'--'--'--'--'--'li'li" },
  { ".nop",          // Nop macro (mode 14)
           "--'--'--'--'--'--'--'--'--'--'--'--'--'--'np" },
  { ".text", "gt" }, // Select seG TEXT
  { ".zero", "gz" }, // Select seG ZERO

  // ins     0  1  2  3  4  5  6  7  8  9 10 11
  { "adc",  "--'69'--'65'75'--'61'71'6D'7D'79'--"},
  { "anc0B","--'0B'--'--'--'--'--'--'--'--'--'--"}, // Unofficial instruction
  { "anc2B","--'2B'--'--'--'--'--'--'--'--'--'--"}, // Unofficial instruction
  { "and",  "--'29'--'25'35'--'21'31'2D'3D'39'--"},
  { "ane",  "--'8B'--'--'--'--'--'--'--'--'--'--"}, // Unofficial instruction
  { "arr",  "--'6B'--'--'--'--'--'--'--'--'--'--"}, // Unofficial instruction
  { "asl",  "0A'--'--'06'16'--'--'--'0E'1E'--'--"},
  { "asr",  "--'4B'--'--'--'--'--'--'--'--'--'--"}, // Unofficial instruction
  { "bcc",  "--'--'90'--'--'--'--'--'--'--'--'--"},
  { "bcs",  "--'--'B0'--'--'--'--'--'--'--'--'--"},
  { "beq",  "--'--'F0'--'--'--'--'--'--'--'--'--"},
  { "bit",  "--'--'--'24'--'--'--'--'2C'--'--'--"},
  { "bmi",  "--'--'30'--'--'--'--'--'--'--'--'--"},
  { "bne",  "--'--'D0'--'--'--'--'--'--'--'--'--"},
  { "bpl",  "--'--'10'--'--'--'--'--'--'--'--'--"},
  { "brk",  "--'00'--'--'--'--'--'--'--'--'--'--"},
  { "bvc",  "--'--'50'--'--'--'--'--'--'--'--'--"},
  { "bvs",  "--'--'70'--'--'--'--'--'--'--'--'--"},
  { "clc",  "18'--'--'--'--'--'--'--'--'--'--'--"},
  { "cld",  "D8'--'--'--'--'--'--'--'--'--'--'--"},
  { "cli",  "58'--'--'--'--'--'--'--'--'--'--'--"},
  { "clv",  "B8'--'--'--'--'--'--'--'--'--'--'--"},
  { "cmp",  "--'C9'--'C5'D5'--'C1'D1'CD'DD'D9'--"},
  { "cpx",  "--'E0'--'E4'--'--'--'--'EC'--'--'--"},
  { "cpy",  "--'C0'--'C4'--'--'--'--'CC'--'--'--"},
  { "dcp",  "--'--'--'C7'D7'--'C3'D3'CF'DF'DB'--"}, // Unofficial instruction, combines DEC+CMP
  { "dec",  "--'--'--'C6'D6'--'--'--'CE'DE'--'--"},
  { "dex",  "CA'--'--'--'--'--'--'--'--'--'--'--"},
  { "dey",  "88'--'--'--'--'--'--'--'--'--'--'--"},
  { "eor",  "--'49'--'45'55'--'41'51'4D'5D'59'--"},
  { "inc",  "--'--'--'E6'F6'--'--'--'EE'FE'--'--"}, // no "inc addr,y", but has "inc addr,x"
  { "inx",  "E8'--'--'--'--'--'--'--'--'--'--'--"},
  { "iny",  "C8'--'--'--'--'--'--'--'--'--'--'--"},
  { "isb",  "--'--'--'E7'F7'--'E3'F3'EF'FF'FB'--"}, // Unofficial instruction, combines INC+SBC
  { "jmp",  "--'--'--'--'--'--'--'--'4C'--'--'6C"},
  { "jsr",  "--'--'--'--'--'--'--'--'20'--'--'--"},
  { "kil02","--'02'--'--'--'--'--'12'--'--'--'--"}, // Unofficial instruction
  { "kil22","--'22'--'--'--'--'--'32'--'--'--'--"}, // Unofficial instruction
  { "kil42","--'42'--'--'--'--'--'52'--'--'--'--"}, // Unofficial instruction
  { "kil62","--'62'--'--'--'--'--'72'--'--'--'--"}, // Unofficial instruction
  { "kil92","--'--'--'--'--'--'--'92'--'--'--'--"}, // Unofficial instruction
  { "kilB2","--'--'--'--'--'--'--'B2'--'--'--'--"}, // Unofficial instruction
  { "kilD2","--'--'--'--'--'--'--'D2'--'--'--'--"}, // Unofficial instruction
  { "kilF2","--'E2'--'--'--'--'--'F2'--'--'--'--"}, // Unofficial instruction
  { "las",  "--'--'--'--'--'--'--'--'--'--'BB'--"}, // Unofficial instruction, combines LDA+LDX+TXS
  { "lax",  "--'AB'--'A7'--'B7'A3'B3'AF'--'BF'--"}, // Unofficial instruction, combines LDA+LDX
  { "lda",  "--'A9'--'A5'B5'--'A1'B1'AD'BD'B9'--"},
  { "ldx",  "--'A2'--'A6'--'B6'--'--'AE'--'BE'--"}, // doesn't support "ldx addr,x"
  { "ldy",  "--'A0'--'A4'B4'--'--'--'AC'BC'--'--"}, // doesn't support "ldy addr,y"
  { "lsr",  "4A'--'--'46'56'--'--'--'4E'5E'--'--"},
  { "nop",  "EA'--'--'--'--'--'--'--'--'--'--'--"},
  { "nop04","--'--'--'04'--'--'--'--'--'--'--'--"}, // Unofficial instruction
  { "nop1A","1A'--'--'--'--'--'--'--'--'--'--'--"}, // Unofficial instruction
  { "nop14","--'--'--'--'14'--'--'--'0C'1C'--'--"}, // Unofficial instruction
  { "nop3A","3A'--'--'--'--'--'--'--'--'--'--'--"}, // Unofficial instruction
  { "nop34","--'--'--'--'34'--'--'--'--'3C'--'--"}, // Unofficial instruction
  { "nop44","--'--'--'44'54'--'--'--'--'--'--'--"}, // Unofficial instruction
  { "nop5A","5A'--'--'--'--'--'--'--'--'--'--'--"}, // Unofficial instruction
  { "nop5C","--'--'--'--'--'--'--'--'--'5C'--'--"}, // Unofficial instruction
  { "nop64","--'--'--'64'74'--'--'--'--'--'--'--"}, // Unofficial instruction
  { "nop7A","7A'--'--'--'--'--'--'--'--'--'--'--"}, // Unofficial instruction
  { "nop7C","--'--'--'--'--'--'--'--'--'7C'--'--"}, // Unofficial instruction
  { "nop80","--'80'--'--'--'--'--'--'--'--'--'--"}, // Unofficial instruction
  { "nop82","--'82'--'--'--'--'--'--'--'--'--'--"}, // Unofficial instruction
  { "nop89","--'89'--'--'--'--'--'--'--'--'--'--"}, // Unofficial instruction
  { "nopC2","--'C2'--'--'--'--'--'--'--'--'--'--"}, // Unofficial instruction
  { "nopDA","DA'--'--'--'--'--'--'--'--'--'--'--"}, // Unofficial instruction
  { "nopD4","--'--'--'--'D4'--'--'--'--'DC'--'--"}, // Unofficial instruction
  { "nopE2","--'E2'--'--'--'--'--'--'--'--'--'--"}, // Unofficial instruction
  { "nopFA","FA'--'--'--'--'--'--'--'--'--'--'--"}, // Unofficial instruction
  { "nopF4","--'--'--'--'F4'--'--'--'--'FC'--'--"}, // Unofficial instruction
  { "ora",  "--'09'--'05'15'--'01'11'0D'1D'19'--"},
  { "pha",  "48'--'--'--'--'--'--'--'--'--'--'--"},
  { "php",  "08'--'--'--'--'--'--'--'--'--'--'--"},
  { "pla",  "68'--'--'--'--'--'--'--'--'--'--'--"},
  { "plp",  "28'--'--'--'--'--'--'--'--'--'--'--"},
  { "rla",  "--'--'--'27'37'--'23'33'2F'3F'3B'--"}, // Unofficial instruction, combines ROL+AND
  { "rol",  "2A'--'--'26'36'--'--'--'2E'3E'--'--"},
  { "ror",  "6A'--'--'66'76'--'--'--'6E'7E'--'--"},
  { "rra",  "--'--'--'67'77'--'63'73'6F'7F'7B'--"}, // Unofficial instruction, combines ROR+ADC
  { "rti",  "40'--'--'--'--'--'--'--'--'--'--'--"},
  { "rts",  "60'--'--'--'--'--'--'--'--'--'--'--"},
  { "sax",  "--'--'--'87'--'97'83'--'8F'--'--'--"}, // Unofficial instruction, stores A&X to memory
  { "sbc",  "--'E9'--'E5'F5'--'E1'F1'ED'FD'F9'--"},
  { "sbcEB","--'EB'--'--'--'--'--'--'--'--'--'--"}, // Unofficial instruction
  { "sbx",  "--'CB'--'--'--'--'--'--'--'--'--'--"}, // Unofficial instruction
  { "sec",  "38'--'--'--'--'--'--'--'--'--'--'--"},
  { "sed",  "F8'--'--'--'--'--'--'--'--'--'--'--"},
  { "sei",  "78'--'--'--'--'--'--'--'--'--'--'--"},
  { "sha",  "--'--'--'--'--'--'--'93'--'--'9F'--"}, // Unofficial instruction
  { "shs",  "--'--'--'--'--'--'--'--'--'--'9B'--"}, // Unofficial instruction
  { "shx",  "--'--'--'--'--'--'--'--'--'--'9E'--"}, // Unofficial instruction
  { "shy",  "--'--'--'--'--'--'--'--'--'9C'--'--"}, // Unofficial instruction
  { "slo",  "--'--'--'07'17'--'03'13'0F'1F'1B'--"}, // Unofficial instruction, combines ASL+ORA
  { "sre",  "--'--'--'47'57'--'43'53'4F'5F'5B'--"}, // Unofficial instruction, combines LSR+EOR
  { "sta",  "--'--'--'85'95'--'81'91'8D'9D'99'--"},
  { "stx",  "--'--'--'86'--'96'--'92'8E'--'--'--"},
  { "sty",  "--'--'--'84'94'--'--'--'8C'--'--'--"},
  { "tax",  "AA'--'--'--'--'--'--'--'--'--'--'--"},
  { "tay",  "A8'--'--'--'--'--'--'--'--'--'--'--"},
  { "tsx",  "BA'--'--'--'--'--'--'--'--'--'--'--"},
  { "txa",  "8A'--'--'--'--'--'--'--'--'--'--'--"},
  { "txs",  "9A'--'--'--'--'--'--'--'--'--'--'--"},
  { "tya",  "98'--'--'--'--'--'--'--'--'--'--'--"},
};
const unsigned InsCount = sizeof(ins) / sizeof(ins[0]);

unsigned GetOperand1Size(unsigned modenum)
{
    if(modenum < AddrModeCount)
        switch(AddrModes[modenum].p1)
        {
            case AddrMode::tRel8:
            case AddrMode::tByte: return 1;
            case AddrMode::tWord: return 2;
            case AddrMode::tNone: ;
        }
    return 0;
}

unsigned GetOperand2Size(unsigned modenum)
{
    if(modenum < AddrModeCount)
        switch(AddrModes[modenum].p2)
        {
            case AddrMode::tRel8:
            case AddrMode::tByte: return 1;
            case AddrMode::tWord: return 2;
            case AddrMode::tNone: ;
        }
    return 0;
}

unsigned GetOperandSize(unsigned modenum)
{
    return GetOperand1Size(modenum) + GetOperand2Size(modenum);
}

bool IsReservedWord(const std::string& s)
{
    const struct ins *insdata = std::lower_bound(ins, ins+InsCount, s);
    return (insdata < ins+InsCount && s == insdata->token)
         || s == ".byt"
         || s == ".word";
}

#if 0
class Gruu
{
public:
    Gruu()
    {
        const char* names[256] = {0};
        unsigned modes[256]    = {0};
        for(const auto& i: ins)
        {
            const char* token = i.token;
            for(const char*s = i.opcodes; *s && s[2]; s += 3)
                if(*s != '-' && *s != '\0' && *s != '\'')
                {
                    unsigned o = std::strtol(s, nullptr, 16);
                    names[o] = token;
                    modes[o] = (s - i.opcodes) / 3;
                }
        }
        printf(" // addressing modes\n");
        for(unsigned n=0; n<256; ++n)
        {
            if(n%16 == 0) printf(" \"");
            putchar( modes[n] + 'A' );
            if(n%16 ==15) printf("\"");
            if(n%64 ==63) printf("\n");
        }
        printf(" // opcodes\n");
        for(unsigned n=0; n<256; ++n)
        {
            if(n%2 == 0) printf(" \"");
            if(names[n])
                printf("%.3s", names[n]);
            else
                printf("???");
            if(n%2 == 1) printf("\"");
            if(n==255) printf(";//%02X\n", n&0xF0);
            else if(n%16 ==15) printf(" //%02X\n", n&0xF0);
        }
    }
} gruu;
#endif
