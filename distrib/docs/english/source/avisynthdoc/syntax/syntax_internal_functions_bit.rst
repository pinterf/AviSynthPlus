
AviSynth Syntax - Bit functions
===============================

The functions are *bitwise operators*. They manipulate individual bits within integer variables. 
This means that their arguments (being integers) are converted to binary numbers, the operation is 
performed on their bits, and the resulting binary number is converted back again.

Since Avisynth 3.7.4 the functions have special 64-bit versions. 
Old functions work and return with the lower 32 bits even if 64-bit data is passed.

64-bit versions work on full 64 bit data.

The shift count of the shift and rotate functions and the bit index of the bit test, set, clear
and change functions are taken modulo the bit width: 0..31 for the 32 bit, 0..63 for the 64 bit
versions (only the lowest 5 or 6 bits of the count are used, like on x86 processors).
E.g. ``BitLShift(1, 32) = 1``, ``BitLShift(1, 33) = 2``, ``BitLShift(1, -1) = BitLShift(1, 31)``,
``BitSet64(0, 64) = 1``. Defined behavior since 3.7.6; before it was undefined in C++ (in practice the same results).

BitAnd
~~~~~~
BitAnd64
~~~~~~~~
::

    BitAnd(int, int)
    BitAnd64(int, int)

Returns the bitwise AND (sets bit to 1 if both bits are 1 and sets bit to 0 otherwise).

*Examples:*
::

    BitAnd(5, 6) = 4 # since 5 = 101, 6 = 110, and 101&110 = 100

BitNot
~~~~~~
BitNot64
~~~~~~~~
::

    BitNot(int)
    BitNot64(int)

Returns the bit-inversion (sets bit to 1 if bit is 0 and vice-versa).

*Examples:*
::

    BitNOT(5) = -6 
    # since  5 = 101, 
    # and ~101 = 1111 1111 1111 1111 1111 1111 1111 1010 = -6

Note: 
::

    1111 1111 1111 1111 1111 1111 1111 1010 = 
    (2^32-1)-2^0-2^2 = 2^32-(1+2^0+2^2) = 
    (signed) -(1+2^0+2^2) = 
    -6.

BitOr
~~~~~
BitOr64
~~~~~~~
::

    BitOr(int, int)
    BitOr64(int, int)

Returns the bitwise inclusive OR (sets bit to 1 if one of the bits (or both) 
is 1 and sets bit to 0 otherwise). 

*Examples:*
::

    BitOr(5, 6) = 7 # since 5 = 101, 6 = 110, and 101|110 = 111
    BitOr(4, 2) = 6 # since 4 = 100, 2 = 010, and 100|010 = 110


BitXor
~~~~~~
BitXor64
~~~~~~~~
::

    BitXor(int, int)
    BitXor64(int, int)

Returns the bitwise exclusive OR (sets bit to 1 if exactly one of the bits is 
1 and sets bit to 0 otherwise). 

*Examples:*
::

    BitXor(5, 6) = 3 # since 5 = 101, 6 = 110, and 101^110 = 011
    BitXor(4, 2) = 6 # since 4 = 100, 2 = 010, and 100^010 = 110

Bit Shift Left (BitLShift, BitShl, BitSal)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Bit Shift Left 64-bit (BitShl64, BitSal64)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
::

    BitLShift(int, int)
    BitShl(int, int)
    BitSal(int, int)
    BitShl64(int, int)
    BitSal64(int, int)

Shift the bits of a number to the left.

Note: When you shift to the left, both logical and arithmetic shifts behave the same way.
Nevertheless, multiple names are provided.

Synonyms. Even more synonyms for 32 bit versions are: ``BitLShiftL``, ``BitLShiftA``, ``BitLShiftU``, ``BitLShiftS``

*Examples:*

Shifts the bits of the number 5 two bits to the left:
::

    BitLShift(5, 2) = 20 (since 101 << 2 = 10100)

Bit shift right, signed (BitRShiftA, BitRShiftS, BitSar)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Bit shift right, signed 64-bit (BitSar64)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
::

    BitRShiftA(int, int)
    BitRShiftS(int, int)
    BitSar(int, int)
    BitSar64(int, int)

Shift the bits of an integer to the right. (Arithmetic, Sign bit fill, Right Shift) 

All 32 bit versions are synonyms.

*Examples:*

Shifts the bits of the number -42 one bit to the right, treating it as signed:
::

    BitRShiftA(-42, 1) = -21 
    # (since 1111 1111 1111 1111 1111 1111 1101 0110 >> 1  
    #      = 1111 1111 1111 1111 1111 1111 1110 1011)

Bit shift right, unsigned (BitRShiftL, BitRShiftU, BitShr)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Bit shift right, unsigned 64-bit (BitShr64)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

::

    BitRShiftL(int, int)
    BitRShiftU(int, int)
    BitShr(int, int)
    BitShr64(int, int)

Shift the bits of an unsigned integer to the right. (Logical, zero fill, Right Shift) 

All 32 bit versions are synonyms.

*Examples:*

Shifts the bits of the number -42 one bit to the right, treating it as unsigned:
::

    BitRShiftL(-42, 1) = 2147483627 
    # (since 1111 1111 1111 1111 1111 1111 1101 0110 >> 1 
    #      = 0111 1111 1111 1111 1111 1111 1110 1011)

Note:
::

    -42 = -(1+2^0+2^3+2^5) = (unsigned) (2^32-1)-(2^0+2^3+2^5) =
    1111 1111 1111 1111 1111 1111 1101 0110 

Bit rotate left (BitLRotate, BitRol)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Bit rotate left 64-bit (bitrol64)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
::

    BitLRotate(int, int)
    BitRol(int, int)
    BitRol64(int, int)

Rotates the bits of an integer to the left by the number of bits specified in 
the second operand. For each rotation specified, the high order bit that exits from 
the left of the operand returns at the right to become the new low order bit. 

*Examples:*

Rotates the bits of the number -2147483642 one bit to the left:
::

    BitLRotate(-2147483642, 1) = 13 
    # (since 10000000000000000000000000000110 ROL 1
    #      = 00000000000000000000000000001101)

Bit rotate right (BitRRotate, BitRor)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Bit rotate right 64-bit (BitRor64)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
::

    BitRRotateL(int, int)
    BitRor(int, int)
    BitRor64(int, int)

Rotates the bits of an integer to the right by the number of bits specified in 
the second operand. For each rotation specified, the low order bit that exits from 
the right of the operand returns at the left to become the new high order bit. 

*Examples:*

Rotates the bits of the number 13 one bit to the right:
::

    BitRRotate(13, 1) = -2147483642 
    # (since 00000000000000000000000000001101 ROR 1 
    #      = 10000000000000000000000000000110)

Bit test (BitTest, BitTst)
~~~~~~~~~~~~~~~~~~~~~~~~~~
Bit test 64-bit (BitTst64)
~~~~~~~~~~~~~~~~~~~~~~~~~~
::

    BitTest(int, int)
    BitTst(int, int)
    BitTst64(int, int)

Tests a single bit (that is, it returns true if its state is one, else it 
returns false). The second operand denotes the location of the bit which is 
specified as an offset from the low order end of the operand (starting at zero). 

*Examples:*

Check the state of the fourth bit:
::

    BitTest(3, 4) = False
    BitTest(19, 4) = True


Check the state of the sign bit on a 32 bit value:
::

    BitTest(-1, 31) = True
    BitTest(2147483647, 31) = False


BitSet
~~~~~~
BitSet64
~~~~~~~~
::

    BitSet(int, int)
    BitSet64(int, int)

Sets a single bit to one (so it sets its state to one). The second operand denotes the 
location of the bit which is specified as an offset from the low order end of the 
operand (starting at zero). 

*Examples:*

Set the state of the fourth bit to one:
::

    BitSet(3, 4) = 19
    BitSet(19, 4) = 19


Set the state of the sign bit to one, return a 32 bit data:
::

    BitSet(-1, 31) = -1
    BitSet(2147483647, 31) = -1


BitSetCount
~~~~~~~~~~~
BitSetCount64
~~~~~~~~~~~~~
::

    BitSetCount(int [, int...])
    BitSetCount64(int [, int...])

Returns the total number of set bits in all supplied integer arguments. 


Bit clear (BitClear, BitClr)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Bit clear 64-bit (BitClr64)
~~~~~~~~~~~~~~~~~~~~~~~~~~~
::

    BitClear(int, int)
    BitClr(int, int)
    BitClr64(int, int)

Sets a single bit to zero (so it sets its state to zero). The second operand denotes 
the location of the bit which is specified as an offset from the low order end of the 
operand (starting at zero). 

*Examples:*

Clear the bits of the number 5
::

    BitClear(5, 0) = 4 (first bit is set to zero)
    BitClear(5, 1) = 5 (second bit is already zero)
    BitClear(5, 2) = 1 (third bit is set to zero)
    BitClear(5, 3) = 5 (fourth bit is already zero)


Clear the state of the sign bit, returns a 32 bit data:
::

    BitClear(-1, 31) = 2147483647

Bit change (BitChange, BitChg)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Bit change 64-bit (BitChg64)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~
::

    BitChange(int, int)
    BitChg(int, int)
    BitChg64(int, int)

Sets a single bit to its complement (so it changes the state of a single bit; 1 becomes 0 
and vice versa). The second operand denotes the location of the bit which is specified as 
an offset from the low order end of the operand (starting at zero). 

The sign bit is bit 31 on the 32-bit version.

The sign bit is bit 63 on the 64-bit version of the function.

*Examples:*

Change the state of the a bit of the number 5:
::

    BitChange(5, 0) = 4 (first bit is set to zero)
    BitChange(5, 1) = 7 (second bit is set to one)
    BitChange(5, 2) = 1 (third bit is set to zero)
    BitChange(5, 3) = 13 (fourth bit is set to one)


Change the state of the sign bit:
::

    BitChange(-1, 31) = 2147483647


Changelog
---------
+-----------------+-----------------------------------------------------+
| Version         | Changes                                             |
+=================+=====================================================+
| Avisynth 3.7.6  || Shift count and bit index are taken modulo the     |
|                 |  bit width (defined behavior, same results)         |
+-----------------+-----------------------------------------------------+
| Avisynth 3.7.4  || 64-bit versions of the bit functions.              |
|                 || Add "BitAnd64", "BitNot64", "BitOr64", "BitXor64   |
|                 || Add "BitShl64", "BitSal64"                         |
|                 || Add "BitShr64", "BitSar64"                         |
|                 || Add "BitRol64", "BitRor64"                         |
|                 || Add "BitChg64", "BitClr64", "BitSet64", "BitTst64" |
|                 || Add "BitSetCount64"                                |
+-----------------+-----------------------------------------------------+
| Avisynth 3.7.3  | Fix bitrol/bitror when first                        |
|                 | argument is negative (Avisynth+                     |
|                 | regression)                                         |
+-----------------+-----------------------------------------------------+
| Avisynth+ r2632 | BitSetCount                                         |
+-----------------+-----------------------------------------------------+
| Avisynth 2.6    | | BitAnd, BitNot, BitOr, BitXor,                    |
|                 | | BitLShift, BitShl, BitSal,                        |
|                 | | BitRShiftA, BitRShiftS, BitSar,                   |
|                 | | BitRShiftL, BitRShiftU, BitShr,                   |
|                 | | BitRol, BitRor,                                   |
|                 | | BitTest, BitTst,                                  |
|                 | | BitSet, BitClear, BitClr,                         |
|                 | | BitChange, BitChg                                 |
+-----------------+-----------------------------------------------------+


--------

Back to :doc:`Internal functions <syntax_internal_functions>`.

$Date: 2025/02/05 13:48:34 $
