# fake6502 Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).



## [2.2.0] - 22-06-2022

Updated the documentation, created this CHANGELOG.md file.


### Added

 - re-instate the counter: instructions

 - sectioning comments/dividers for .c and .h files

 - checks for invalid options settings/combinations

 - DECIMALMODE documentation


### Changed

 - created structs: state_6502_cpu, state_6502_emu

 - renamed struct: context_t -> context_6502

 - mem[65536] -> void *state_host,
for the host code to have it's own data struct/state area

 - renamed struct: opcode_t -> opcode_6502

 - move defines/macros into the header file

 - removed `static` from some fns,
so the host code can use them to extend the library



## [2.1.0] - 18-06-2022

Source code tidy up, for better inclusion as a source code library.

### Changed

 - void step(context_t *c) -> step6502(context_t *c)



## [2.0.0] - 2020 - 2022

```

commit 3848235cb2f38da8d392d3074bd1b9d28e1f79c2
Author: John Walsh <john.walsh@mini-net.co.uk>
Date:   Sun Jun 19 09:14:06 2022 +0100

    simple code/build bug fixes

commit bc124c83be13f1d95473d57713132c0632ad55d8
Author: John Walsh <john.walsh@mini-net.co.uk>
Date:   Sat Jun 18 22:35:45 2022 +0100

    basic source code tidy up, step() -> step6502()

commit 42fd56a539a120b5973bff9906135b24df09ef45
Author: Sam M W <you@example.com>
Date:   Fri Aug 6 07:39:58 2021 +0100

    Fix stupid SBC borrow bug

commit 7279e186f410abb912b71609326f156079d9afd7
Author: Sam M W <you@example.com>
Date:   Tue Jun 29 16:40:30 2021 +0100

    Extract arithmetic shift left

commit 1318e8b58482180ad279caa3475c4b40e8462ffb
Merge: b352e33 57a1785
Author: Sam M W <you@example.com>
Date:   Tue Jun 29 16:19:00 2021 +0100

    Merge branch 'master' of github.com:omarandlorraine/fake6502

commit b352e33e3ab6f428f5e505422420acc456b7b761
Author: Sam M W <you@example.com>
Date:   Tue Jun 29 16:18:49 2021 +0100

    add test for RLA opcode

commit 57a1785c25ba6a85ada51551a3a093a2712d859e
Author: Sam M W <you@example.com>
Date:   Fri Jun 25 21:37:05 2021 +0100

    expose stack operations

commit 03ce7b24f603c1269e738a25d50fafe0bc19dce0
Author: Sam M W <you@example.com>
Date:   Thu Jun 3 09:03:23 2021 +0100

    static compare for compat. with stoc

commit 641cfa02bf477a0a05ed377755d465cff7f399ce
Author: Sam M W <you@example.com>
Date:   Mon May 31 08:22:30 2021 +0100

    Refactor for SAX and LAX

commit e521525a3472600803645c28e798b1e16b3183b1
Author: Sam M W <you@example.com>
Date:   Mon May 31 07:43:40 2021 +0100

    Remove extraneous writes of A and of X during SAX

commit 98f41af228354fb1dfd6cb08df03ee24667ad9e0
Author: Sam M W <you@example.com>
Date:   Sun May 30 13:45:46 2021 +0100

    Removed unused variable

commit a8aea1c1528d1d0f3e3a6121184b57d878b95329
Author: Sam M W <you@example.com>
Date:   Thu May 27 16:43:24 2021 +0100

    The NMI vector is 16 bits of course

commit 7714a387dccae3e01a6d5f5325843f1337906286
Author: Sam M W <you@example.com>
Date:   Thu May 27 16:18:27 2021 +0100

    Bugfix for Immediate BIT opcode

commit ee651a6f752456874691cbee377d3a5662c67ccf
Author: Sam M W <you@example.com>
Date:   Tue May 25 09:02:11 2021 +0100

    Correct cycle count for CMOS jmp (absolute,x)

commit efc306a714c0120b8c16a2a0fecfe6c819ab980e
Author: Sam M W <you@example.com>
Date:   Sun May 23 21:38:09 2021 +0100

    Implement the CMOS indirect jump bugfix

commit 6e85251ddfcc1c9d9fe8730655ecc85ad0b3d8c0
Author: Sam M W <you@example.com>
Date:   Sat May 22 14:31:41 2021 +0100

    Extract functions for more ALU ops

commit 45ba34bbf38b5e968362cee95cac82c08750cc3f
Author: Sam M W <you@example.com>
Date:   Sat May 22 12:47:39 2021 +0100

    add remark about what to #define

commit 7da29da21c5f75857934a0a8c24647b5ba92ca80
Author: Sam M W <you@example.com>
Date:   Thu May 20 17:15:25 2021 +0100

    formatting

commit 1569b9094718eab01d43cae12acd9137f0b5ab7b
Author: Sam M W <you@example.com>
Date:   Thu May 20 17:14:00 2021 +0100

    Remove outdated comment

commit 0bc1c68f8a874f4175e66c397e2a100160a79698
Author: Sam M W <you@example.com>
Date:   Mon May 17 09:26:02 2021 +0100

    brainfart

commit d8eadac26fbcdc3d7d5ede11141300303624ed30
Author: Sam M W <you@example.com>
Date:   Sun May 16 21:30:46 2021 +0100

    Remove unused vars

commit a7fd477e01d80ab946f86bb8ae8c8937ca7d4696
Author: Sam M W <you@example.com>
Date:   Sun May 16 21:21:21 2021 +0100

    whitespace

commit 5772fffe9affd21daa97f1fa0ad08afd60074078
Author: Sam M W <you@example.com>
Date:   Sun May 16 21:20:50 2021 +0100

    Implement cycle-penalty for page-crossing

commit 4d886768c929a445de04d1d3210c7957f7602151
Author: Sam M W <you@example.com>
Date:   Fri Apr 30 22:00:59 2021 +0100

    dead code elimination

commit 94a6e3021c696813ffa2b4854ab0665dc46891b3
Author: Sam M W <you@example.com>
Date:   Fri Apr 30 21:48:05 2021 +0100

    detect and fix vicious bug affecting acc fetch

commit e9e07d8d15bc707ea0627778ac4f284215b07856
Author: Sam M W <you@example.com>
Date:   Thu Apr 29 20:44:53 2021 +0100

    A few renamings

commit 74b037ff31f7758df08dc42d595c348ff603bfbc
Author: Sam M W <you@example.com>
Date:   Wed Apr 28 21:36:19 2021 +0100

    Remove now untruthful comments

commit 25afa170e70615295b09c9d218c3a1dc331020b6
Author: Sam M W <you@example.com>
Date:   Wed Apr 28 21:32:21 2021 +0100

    whitespace touchup

commit 1431e20e9b18193200a4ee380ffdf9697b7ff834
Author: Sam M W <you@example.com>
Date:   Wed Apr 28 21:29:31 2021 +0100

    Add instruction table and opcodes for 65C02 variant

commit c468d5c5cd39a658ef97ef4826ddd7f021bf1cba
Author: Sam M W <you@example.com>
Date:   Tue Apr 27 22:36:42 2021 +0100

    put instruction handlers, ticks & addr_modes all in one struct

commit e0de30426189c3b48b428fb57453db1e9cf9d3c6
Author: Sam M W <you@example.com>
Date:   Tue Apr 27 21:48:25 2021 +0100

    DRY for ror and rra opcodes

commit 863f91b2786df7b730fb279f84054c623706f402
Author: Sam M W <you@example.com>
Date:   Tue Apr 20 14:43:10 2021 +0100

    use clang-format

commit da43f5ea22459cad5b946365e94a3220574e2fc9
Author: Sam M W <you@example.com>
Date:   Tue Apr 20 14:21:05 2021 +0100

    break ALU code out of opcode handlers for DRY

commit a2bfd6d8619ade09cb5da84bf3b11d9d2e98034f
Author: Sam M W <you@example.com>
Date:   Tue Apr 20 14:13:13 2021 +0100

    Fix decimal mode for subtract

commit 76c2f0390298975570bd7f5cdada87473e6c5911
Author: Sam M W <you@example.com>
Date:   Wed Apr 14 21:19:32 2021 +0100

    DRY

commit c60319aac88b6cbd4e0e2072af5276490dd9a7b9
Author: Sam M W <you@example.com>
Date:   Wed Apr 14 21:19:21 2021 +0100

    correct number of writes and reads for ror

commit 3f6fc4bdaf0ad674e320d4a170f9145894696ced
Author: Sam M W <you@example.com>
Date:   Wed Apr 14 21:17:28 2021 +0100

    correct number of memory accesses for RRA

commit 122d83785c191b32afdcee73dd26ae22e1d1f132
Author: Sam M W <you@example.com>
Date:   Mon Apr 12 11:38:24 2021 +0100

    Move if(thing) and body to separate lines, for gcov

commit 02f7e3c3c890f5ae09c192a6d0b42124e03e95f2
Author: Sam M W <you@example.com>
Date:   Sun Apr 11 14:58:02 2021 +0100

    Remove dead code

commit 0c2e72cbc5fcc785d9b9da7cebeca20fd7ea2c1b
Author: Sam M W <you@example.com>
Date:   Sun Apr 11 11:57:59 2021 +0100

    Add unit tests for reset and IRQ

commit b463ca4c2b2294109de37d9bb999cdbc58ba8234
Merge: 72a8e66 4ab005e
Author: Sam M W <you@example.com>
Date:   Sun Jan 31 14:26:38 2021 +0000

    Merge branch 'master' of omarandlorraine/fake6502

commit 72a8e669b57ba38a2b762b34eebe8dd1b67ffa48
Author: Sam M W <you@example.com>
Date:   Sun Jan 31 14:25:55 2021 +0000

    Get rid of unnecessary bits

commit 96c84b2a30beadd8979e3c7496d5100018bf60e4
Author: Webfra <webfra14@web.de>
Date:   Tue Jan 26 01:07:16 2021 +0100

    Fixed missing update of Z-flag in and()

commit ef4b8d870db22ecf73550a81622a8d2f8a891eb5
Author: Sam M W <you@example.com>
Date:   Mon Jan 11 21:47:36 2021 +0000

    Brainfart

commit 78cbb7162c5a2ef32694e0a76471a9f571072efd
Author: Sam M W <you@example.com>
Date:   Sun Jan 10 20:29:43 2021 +0000

    bugfix to reset

commit ade6764a32e80064b6ad0f4b308b08ee0ec9dabe
Author: Webfra <webfra14@web.de>
Date:   Sun Jan 10 12:00:46 2021 +0100

    Update fake6502.c

commit 51a595f77d5c74c8803c45df48117af9cd61e0ea
Author: Webfra <webfra14@web.de>
Date:   Sun Jan 10 02:56:20 2021 +0100

    Removed re-calculation of relative branch address in bra().

commit a157dfc8c33085e75d09d19ffc9270a36c40ef14
Author: Webfra <webfra14@web.de>
Date:   Thu Jan 7 14:17:59 2021 +0100

    Fixed B flag consistency, internally and on the stack. Internal B flag is now always "set".

commit a4ab9da459f496c02ea39766f766f1584f9e4625
Author: Webfra <webfra14@web.de>
Date:   Wed Jan 6 17:35:36 2021 +0100

    Added I-flag check in irq6502().

commit f98c858db0a6626acb0da25e903f456ccfc5fef4
Author: Webfra <webfra14@web.de>
Date:   Wed Jan 6 17:35:11 2021 +0100

    Fixed B-flag handling in plp() and rti().

commit cde8c657e62787acd86433ce541f319046cb711b
Author: Sam M W <you@example.com>
Date:   Wed Dec 16 21:55:53 2020 +0000

    Add 65C02 emulation

commit 38a7fc959015fc7da8951d6ddcd94552f53c1c21
Author: Sam M W <you@example.com>
Date:   Wed Dec 16 20:56:06 2020 +0000

    More CMOS opcodes

commit fa111613ca687142d135ab5498ad9551ca3b1f94
Author: Sam M W <you@example.com>
Date:   Wed Dec 16 20:49:12 2020 +0000

    More CMOS opcodes

commit 88eecdf97c88e52a2abc95baca8215ac3f35540d
Author: Sam M W <you@example.com>
Date:   Tue Dec 15 21:24:44 2020 +0000

    Add dea and ina opcodes

commit 61e28dee659c9609b2a1576ad2f35edec26f0552
Author: Sam M W <you@example.com>
Date:   Tue Dec 15 20:58:59 2020 +0000

    dry

commit a99bf55b676101acafaebb0333e5cd1a44ef1c32
Author: Sam M W <you@example.com>
Date:   Tue Dec 15 20:55:09 2020 +0000

    dry

commit 9e6b344cd0162a8f4f1ca006a1784341bfbc8cb3
Author: Sam M W <you@example.com>
Date:   Tue Dec 15 20:50:07 2020 +0000

    dry

commit d94366de315e58d579b2eb7337f0e050861276da
Author: Sam M W <you@example.com>
Date:   Mon Dec 14 06:05:18 2020 +0000

    I trust you are all quite well

```



## [1.2.0] - 14-12-2020

this fork was created:

https://github.com/omarandlorraine/fake6502



## [1.1.0] - 17-12-2011

Small bugfix in BIT opcode, but it was the difference between
a few games in my NES emulator working and being broken!
I went through the rest carefully again after fixing it
just to make sure I didn't have any other typos!



## [1.0.0] - 24-11-2011

Fake6502 was originally created by:

(c) 2011 Mike Chambers (miker00lz@gmail.com) <br/>
Fake6502 CPU emulator core

First release.

This source code is released into the
public domain, but if you use it please do give
credit. I put a lot of effort into writing this!

If you do discover an error in timing accuracy,
or operation in general please e-mail me at the
address above so that I can fix it. Thank you!
