<html>
 <head>
  <title>nescom demo</title>
 </head>
 <body>
  <h1>nescom demo</h1>
  
  Here's a demonstration how I use <a href="/source/nescom.html">nescom</a>,
  a free 6502 assembler + linker to create a simple NES ROM.
  <p>
  <table border=1>
   <tr>
    <th>Filename</th> <th>description</th>
   </tr>
   
   <tr>
    <td><a href="header.a65">header.a65</a></td>
    <td>The main assembly module. It contains the nes interrupt
        and reset vectors.
        It will be compiled to an IPS file
        and it will populate NES addresses $FFFA through $FFFF.</td>
   </tr>
   
   <tr>
    <td><a href="system.a65">system.a65</a></td>
    <td>Module: global functions and the interrupt and reset handlers.
        Initializes stuff, then calls less critical code.
        It's location in the ROM is decided at link time.</td>
   </tr>
   
   <tr>
    <td><a href="main.a65">main.a65</a></td>
    <td>The main module. It simply setups the music in this example.
        It's location in the ROM is decided at link time.</td>
   </tr>
   
   <tr>
    <td><a href="music.a65">music.a65</a></td>
    <td>This music player is actually ripped from Rockman 1 / Rockman 2.
        It is optimized a bit by Bisqwit, but that's it. It is used
        in <a href="http://bisqwit.iki.fi/jutut/rockmanbasics/">Rockman Basics</a>.
        It's location in the ROM is decided at link time,
        despite the <code>*=</code> statement in the code.</td>
   </tr>
   
   <tr>
    <td><a href="Makefile">Makefile</a></td>
    <td>Makefile contains the rom build instructions for the "make" program.</td>
   </tr>
   
   <tr>
    <td><a href="demo.nes">demo.nes</a></td>
    <td>This is the ROM that is created as a result. (64 kB, just for fun)</td>
   </tr>
   
  </table>
  
  <p>
  Copyright &copy; 1992, 2006 Bisqwit
  (<a href="http://iki.fi/bisqwit/">http://iki.fi/bisqwit/</a>)
  
  <hr>
  See also:
  <a href="../snescom-demo/">snescom demo for SNES compiling</a>
 </body>
</html>
