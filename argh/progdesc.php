<?php
//TITLE=Command line argument handling library

$title = 'Command line argument handling library';
$progname = 'libargh';

$k1='';foreach(file('/usr/local/include/argh.hh') as $s)$k1.=$s;
$k2='';foreach(file('/usr/local/include/argh.h') as $s)$k2.=$s;

$text = array(
   '1. Purpose' => "

Handles commandline parameters.

", '1. Supported' => "

C, C++

", '1. Headers' => "

", '1.1. C++ header: argh.hh' => "

<pre class=smallerpre>".htmlspecialchars($k1)."</pre>

", '1.1. C header: argh.h' => "

<pre class=smallerpre>".htmlspecialchars($k2)."</pre>

", '1. Copying' => "

libargh has been written by Joel Yliluoma, a.k.a.
<a href=\"http://iki.fi/bisqwit/\">Bisqwit</a>,<br>
and is distributed under the terms of the
<a href=\"http://www.gnu.org/licenses/licenses.html#GPL\">General Public License</a> (GPL).

", '1. Requirements' => "

argh has been written in C++, utilizing the standard template library.<br>
GNU make is required.<br>
libargh compiles without warnings at least on g++ versions 3.0.1 and 3.0.3.

");
include '/WWW/progdesc.php';
