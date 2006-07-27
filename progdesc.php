<?php
//TITLE=Free NES (6502) assembler and linker

$title = 'Free NES assembler';
$progname = 'nescom';

$text = array(
   'purpose:1. Purpose' => "

This program reads symbolic 6502/RP2A03/RP2A07 machine code
and compiles (assembles) it into a relocatable object file
or into an IPS patch.
 <p>
The produced object file is binary-compatible with those
made with <a href=\"http://www.google.fi/search?q=xa65\">XA65</a>.

", 'history:1.1. History' => "

This program was born when Bisqwit needed to have something for
NES that is already accomplished for SNES by
<a href=\"http://bisqwit.iki.fi/source/snescom.html\">snescom</a>.


", 'linker:1. Linker' => "

This package also contains a linker.<br>
See <a href=\"http://bisqwit.iki.fi/src/nescom-demo/\"
>http://bisqwit.iki.fi/src/nescom-demo/</a> - there's
a demo explaining how this all works.
 <p>
The linker can also be used to convert IPS patches into
binary files (an empty space is assumed to be the original
file), with the following command:
 <code>neslink input.ips -o result.bin -f raw</code><br>

", 'syntax:1. Supported syntax' => "

", 'ops:1.1. Mnemonics' => "

The following mnemonics are supported:
<p>
<code>adc</code>, <code>and</code>, <code>asl</code>, <code>bcc</code>, 
<code>bcs</code>, <code>beq</code>, <code>bit</code>, <code>bmi</code>, 
<code>bne</code>, <code>bpl</code>, <code>brk</code>, <code>bvc</code>, 
<code>bvs</code>, <code>clc</code>, <code>cld</code>, <code>cli</code>, 
<code>clv</code>, <code>cmp</code>, <code>cpx</code>, <code>cpy</code>, 
<code>dec</code>, <code>dex</code>, <code>dey</code>, <code>eor</code>, 
<code>inc</code>, <code>inx</code>, <code>iny</code>, <code>jmp</code>, 
<code>jsr</code>, <code>lda</code>, <code>ldx</code>, <code>ldy</code>, 
<code>lsr</code>, <code>nop</code>, <code>ora</code>, <code>pha</code>, 
<code>php</code>, <code>pla</code>, <code>plp</code>, <code>rol</code>, 
<code>ror</code>, <code>rti</code>, <code>rts</code>, <code>sbc</code>, 
<code>sec</code>, <code>sed</code>, <code>sei</code>, <code>sta</code>, 
<code>stx</code>, <code>sty</code>, <code>tax</code>, <code>tay</code>, 
<code>tsx</code>, <code>txa</code>, <code>txs</code>, <code>tya</code>
</p>

", 'addrmodes:1.1. Addressing modes' => "

All the standard addressing modes of the 6502 cpu are supported.
<p>
Syntax:

<table border=\"1\">
 <tr>
  <th>Name</th>
  <th>Examples</th>
  <th>Functionally equivalent to (not nescom syntax, provided for illustration only)</th>
 </tr>
 <tr>
  <td>Implied:</td>
   <td> <code>nop</code>; <code>clc</code> </td>
 </tr>
 <tr>
  <td>Immediate:</td>
   <td> <code>lda #value</code> etc </td>
   <td> <code>a = value</code> </td>
 </tr>
 <tr>
  <td>Short relative:</td>
   <td> <code>bra end</code></td>
 </tr>
 <tr>
  <td>Direct:</td>
   <td> <code>lda \$12</code></td>
   <td> <code>a = ram[\$0012]</code> </td>
 </tr>
 <tr>
  <td>Direct indexed:</td>
   <td> <code>lda \$12,x</code><br><code>lda \$12,y</code> </td>
   <td> <code>a = ram[\$0012 + x]</code><br><code>a = ram[\$0012 + y]</code> </td>
 </tr>
 <tr>
  <td>Direct indexed indirect:</td>
   <td> <code>lda (\$12,x)</code> </td>
   <td> <code>a = ram[ramw[\$0012 + x]]</code> </td>
 </tr>
 <tr>
  <td>Direct indirect indexed:</td>
   <td> <code>lda (\$12),y</code> </td>
   <td> <code>a = ram[ramw[\$0012] + y]</code> </td>
 </tr>
 <tr>
  <td>Absolute:</td>
   <td> <code>lda \$1234</code> </td>
   <td> <code>a = ram[\$1234]</code> </td>
 </tr>
 <tr>
  <td>Absolute indexed:</td>
   <td> <code>lda \$1234,x</code><br><code>lda \$1234,y</code> </td>
   <td> <code>a = ram[\$1234 + x]</code><br><code>a = ram[\$1234 + y]</code> </td>
 </tr>
 <tr>
  <td>Absolute indirect:</td>
   <td> <code>lda (\$1234)</code> </td>
   <td> <code>a = ram[ramw[\$1234]]</code> </td>
 </tr>
</table>
Note: \"ram\" and \"ramw\" in the table refer to generic
address space access (RAM, I/O ports, ROM etc).
\"ramw\" means 16-bit read access,
but the meaning of \"ram\" depends on the context.
It can be a read, write or a jump target, depending on instruction.

", 'opsize:1.1. Operand size control' => "

There are several operand prefixes
that can be used to force a certain operand size/type.

<ul>
 <li><code>lda !\$f0</code> would use 16-bit address instead of direct page.</li>
 <li><code>lda #&lt;var</code> can be used to load the lower 8 bits of an external variable.</li>
 <li><code>lda #&gt;var</code> can be used to load the upper 8 bits of an external variable.</li>
</ul>

", 'eval:1.1. Expression evaluation' => "

Expressions are supported. These are valid code:
<ul>
 <li><code>bra somewhere+1</code></li>
 <li><code>lda #!address + \$100</code></li>
 <li><code>ldy #\$1234 + (\$6C * 3)</code></li>
</ul>

", 'segs:1.1. Segments' => "

Code, labels and data can be generated to four segments:
<code>text</code>, <code>data</code>, <code>zero</code> and <code>bss</code>.<br>
Use <code>.text</code>, <code>.data</code>, <code>.zero</code> and <code>.bss</code>
respectively to select the segment.<br>
However, only the contents of <code>text</code>
and <code>data</code> segments are saved into
the o65 file. Labels are saved in all segments.

", 'comments:1.1. Comments' => "

Comments begin with a semicolon (;) and end with a newline.<br>
A colon is allowed to appear in comment.

", 'separation:1.1. Command separation' => "

Commands are separated by newlines and colons (:).

", 'reloc:1.1. Code pointer relocation' => "

You can use a command like <code>*= \$F200</code> to change
where the code goes by default.<br>
With IPS this is especially useful.<br>
You can change the code pointer as many times as you wish,
but unless you're generating an IPS file, all code must be
a continuous block.
 <p>
Note that in the IPS format, the offset denotes the file offset,
not NES offset.

", 'branchlabels:1.1. Branch labels' => "

The label <code>-</code> can be defined for local branches backward
and <code>+</code> for branches forward.

", 'cpp:1.1. Preprocessor' => "

nescom uses <a href=\"http://gcc.gnu.org/\">GCC</a> as a preprocessor.<br>
You can use <code>#ifdef</code>, <code>#ifndef</code>, <code>#define</code>,
<code>#if</code>, <code>#endif</code> and <code>#include</code> like
in any C program. (See <a href=\"#bugs\">bugs</a>)

", 'objfile:1.1. Object file format' => "

nescom produces relocatable object files (O65),
non-relocatable patch files (IPS), or raw files.<br>
The O65 file format has been
<a href=\"http://www.google.com/search?q=6502+binary+relocation+format\">documented</a>
by André Fachat
for the <a href=\"http://www.google.fi/search?q=xa65\">XA65</a> project.

", 'ips:1.1. IPS output support' => "

This version of nescom allows you to create IPS files.<br>
This IPS format has been extended to allow you to specify
global symbols and externs to be patched later.<br>
In the generated format:
<ul>
 <li>If the patch address is \$000001, it means this
     entry is a dynamic symbol. What follows is an asciiz
     string (the symbol name), the address (3-byte lsb-first integer)
     and the relocation size in bytes (1 byte).</li>
 <li>If the patch address is \$000002, it means this
     entry is a global symbol. What follows is an asciiz
     string (the symbol name) and the address (3-byte lsb-first integer)
     where this symbol is located in.
</ul>

", 'linkage:1.1. Linkage selection' => "

By default, O65 objects are linked to any free location in the ROM.<br>
IPS files are linked to predefined locations.
 <p>
With the <code>.link</code> statement, you can change that.
 <p>
<code>.link page \$3F</code> declares that this object should be
placed into page \$3F.<p>
<code>.link group 1</code> declares that this object should be
placed to the same page together with all other objects that
want to be linked in group 1. This is useful when you want to
ensure that certain tables or routines go to the same page,
even if they are not in the same compilation unit.<br>
The actual page is determined during link time, and you can
get the page by using 24-bit (@) or segment reference (^)
to a symbol from those modules.
<p>
<em>This is not completely ready for NES yet.</em>

", 'changelog:1. Changelog' => "

Nov 20 2005; 0.0.0 import from snescom-1.5.0.1.<br>
Nov 29 2005; 0.1.0 bugfixes, now it's functional.<br>
Dec 16 2005; 0.2.0 support for deeper \"++\" and \"--\" labels. Also support for <code>.nop</code> statement.<br>
Jun 20 2006; 1.0.0 raw output file format support in nescom. Also using boost::smart_ptr.<br>
Jul 26 2006; 1.1.0 has now a linker, and an example program.

", 'bugs:1. Known bugs' => "

<ul>
 <li><code>#include</code>d files aren't being properly preprocessed.</li>
 <li>memory mapping hasn't been quite designed yet.</li>
</ul>

", 'copying:1. Copying' => "

nescom has been written by Joel Yliluoma, a.k.a.
<a href=\"http://iki.fi/bisqwit/\">Bisqwit</a>,<br>
and is distributed under the terms of the
<a href=\"http://www.gnu.org/licenses/licenses.html#GPL\">General Public License</a> (GPL).
<p>
If you happen to see this program useful for you, I'd
appreciate if you tell me :) Perhaps it would motivate
me to enhance the program.

", 'requirements:1. Requirements' => "

nescom uses <a href=\"http://gcc.gnu.org/\">GCC</a> as a slave in
the preprocessing phase. Therefore, gcc must be installed and found
in the PATH when running nescom.

");
include '/WWW/progdesc.php';
