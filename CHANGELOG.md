# fake6502 Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).



## [2.4.0] - 19-07-2022

Source code layout (and some test.c fn naming) updates:

 - removed comments from end of code lines

 - fn() { - moved to a new line

 - tests.c: typedef struct test_t -> test_fn, added prefix 'test_' to fn names

 - all addr/opcode fn's - a static option (by compile time #define option)

 - return's - ensured braces around value



## [2.3.0] - 16-07-2022

Updated all global names to have a common prefix: fake6502_
to avoid conflicts with any other source files.

Changed the naming convention to be prefix_object_action:

setcarry -> fake6502_carry_set, etc...

zerocalc -> fake6502_zero_calc, etc...



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

### Added
- A test harness
- CMOS support

### Changed
- Get reset to behave as on the real hardware
- Some flag and interrupt related bugfixes


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
