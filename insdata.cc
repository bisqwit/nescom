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
  { /* 13 .link page $FF */ 0, "page",  "",AddrMode::tByte, AddrMode::tNone }
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
  { ".text", "gt" }, // Select seG TEXT
  { ".zero", "gz" }, // Select seG ZERO
  
  // ins     0  1  2  3  4  5  6  7  8  9 10 11
  { "adc", "--'69'--'65'75'--'61'71'6D'7D'79'--"},
  { "and", "--'29'--'25'35'--'21'31'2D'3D'39'--"},
  { "asl", "0A'--'--'06'16'--'--'--'0E'1E'--'--"},
  { "bcc", "--'--'90'--'--'--'--'--'--'--'--'--"},
  { "bcs", "--'--'B0'--'--'--'--'--'--'--'--'--"},
  { "beq", "--'--'F0'--'--'--'--'--'--'--'--'--"},
  { "bit", "--'--'--'24'--'--'--'--'2C'--'--'--"},
  { "bmi", "--'--'30'--'--'--'--'--'--'--'--'--"},
  { "bne", "--'--'D0'--'--'--'--'--'--'--'--'--"},
  { "bpl", "--'--'10'--'--'--'--'--'--'--'--'--"},
  { "brk", "00'--'--'--'--'--'--'--'--'--'--'--"},
  { "bvc", "--'--'50'--'--'--'--'--'--'--'--'--"},
  { "bvs", "--'--'70'--'--'--'--'--'--'--'--'--"},
  { "clc", "18'--'--'--'--'--'--'--'--'--'--'--"},
  { "cld", "D8'--'--'--'--'--'--'--'--'--'--'--"},
  { "cli", "58'--'--'--'--'--'--'--'--'--'--'--"},
  { "clv", "B8'--'--'--'--'--'--'--'--'--'--'--"},
  { "cmp", "--'C9'--'C5'D5'--'C1'D1'CD'DD'D9'--"},
  { "cpx", "--'E0'--'E4'--'--'--'--'EC'--'--'--"},
  { "cpy", "--'C0'--'C4'--'--'--'--'CC'--'--'--"},
  { "dec", "--'--'--'C6'D6'--'--'--'CE'DE'--'--"},
  { "dex", "CA'--'--'--'--'--'--'--'--'--'--'--"},
  { "dey", "88'--'--'--'--'--'--'--'--'--'--'--"},
  { "eor", "--'49'--'45'55'--'41'51'4D'5D'59'--"},
  { "inc", "--'--'--'E6'F6'--'--'--'EE'FE'--'--"},
  { "inx", "E8'--'--'--'--'--'--'--'--'--'--'--"},
  { "iny", "C8'--'--'--'--'--'--'--'--'--'--'--"},
  { "jmp", "--'--'--'--'--'--'--'--'4C'--'--'6C"},
  { "jsr", "--'--'--'--'--'--'--'--'20'--'--'--"},
  { "lda", "--'A9'--'A5'B5'--'A1'B1'AD'BD'B9'--"},
  { "ldx", "--'A2'--'A6'--'B6'--'--'AE'--'BE'--"},
  { "ldy", "--'A0'--'A4'B4'--'--'--'AC'BC'--'--"},
  { "lsr", "4A'--'--'46'56'--'--'--'4E'5E'--'--"},
  { "nop", "EA'--'--'--'--'--'--'--'--'--'--'--"},
  { "ora", "--'09'--'05'15'--'01'11'0D'1D'19'--"},
  { "pha", "48'--'--'--'--'--'--'--'--'--'--'--"},
  { "php", "08'--'--'--'--'--'--'--'--'--'--'--"},
  { "pla", "68'--'--'--'--'--'--'--'--'--'--'--"},
  { "plp", "28'--'--'--'--'--'--'--'--'--'--'--"},
  { "rol", "2A'--'--'26'36'--'--'--'2E'3E'--'--"},
  { "ror", "6A'--'--'66'76'--'--'--'6E'7E'--'--"},
  { "rti", "40'--'--'--'--'--'--'--'--'--'--'--"},
  { "rts", "60'--'--'--'--'--'--'--'--'--'--'--"},
  { "sbc", "--'E9'--'E5'F5'--'E1'F1'ED'FD'F9'--"},
  { "sec", "38'--'--'--'--'--'--'--'--'--'--'--"},
  { "sed", "F8'--'--'--'--'--'--'--'--'--'--'--"},
  { "sei", "78'--'--'--'--'--'--'--'--'--'--'--"},
  { "sta", "--'--'--'85'95'--'81'91'8D'9D'99'--"},
  { "stx", "--'--'--'86'--'96'--'--'8E'--'--'--"},
  { "sty", "--'--'--'84'94'--'--'--'8C'--'--'--"},
  { "tax", "AA'--'--'--'--'--'--'--'--'--'--'--"},
  { "tay", "A8'--'--'--'--'--'--'--'--'--'--'--"},
  { "tsx", "BA'--'--'--'--'--'--'--'--'--'--'--"},
  { "txa", "8A'--'--'--'--'--'--'--'--'--'--'--"},
  { "txs", "9A'--'--'--'--'--'--'--'--'--'--'--"},
  { "tya", "98'--'--'--'--'--'--'--'--'--'--'--"}
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
