# Free NES (6502) assembler and linker

## Purpose

This program reads symbolic 6502/RP2A03/RP2A07 machine code
and compiles (assembles) it into a relocatable object file
or into an IPS patch.

The produced object file is binary-compatible with those
made with [XA65](http://www.google.fi/search?q=xa65).

## History

This program was born when Bisqwit needed to have something for
NES that is already accomplished for SNES by
[snescom](http://bisqwit.iki.fi/source/snescom.html).

## Linker

This package also contains a linker.

The linker can also be used to convert IPS patches into
binary files (an empty space is assumed to be the original
file), with the following command:

    neslink input.ips -o result.bin -f raw

## More detailed information

For more detailed information, see the HTML format documentation at:
http://bisqwit.iki.fi/source/nescom.html

## Copying

nescom has been written by Joel Yliluoma, a.k.a.
[Bisqwit](http://iki.fi/bisqwit/),    
and is distributed under the terms of the
[General Public License](http://www.gnu.org/licenses/licenses.html#GPL) (GPL).

If you happen to see this program useful for you, I'd
appreciate if you tell me :) Perhaps it would motivate
me to enhance the program.
