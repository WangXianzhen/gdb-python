#name: Valid v8-a
#objdump: -dr --prefix-addresses --show-raw-insn

.*: +file format .*arm.*

Disassembly of section .text:
0[0-9a-f]+ <[^>]+> e320f005 	sevl
0[0-9a-f]+ <[^>]+> bf50      	sevl
0[0-9a-f]+ <[^>]+> bf50      	sevl
0[0-9a-f]+ <[^>]+> f3af 8005 	sevl.w
