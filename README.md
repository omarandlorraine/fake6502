# fake6502

This emulator was written originally by Mike Chambers.

I have made a few modifications to it, the biggest of which is that you now
pass it a pointer to a struct holding the CPU's state. This means that your
program may now emulate more than one 6502 concurrently. This was necessitated
by stoc, my superoptimiser, which needs to run two programs in parallel to
check them for equivalence.

The emulator as I found it, can be for an ordinary 6502 or for a 2A03 (rando
variant found in some games console). I've taken the liberty of adding a CMOS
support; this adds a few opcodes here and there). I might get round to adding
support for other chips in the future.

I've put a test harness around this. Coverage is currently at around 90%, which
includes all of the documented NMOS instructions.

I would not recommend using this. There are better emulators out there,
including the original fake6502 by Mike Chambers, and others that are both more
accurate and more permissively licensed.

Do not contribute to this project. It is dead. It is not going anywhere.
