
AviSynth Syntax - Numeric functions
===================================

Numeric functions provide common mathematical operations on numeric
variables.

Note: since 3.7.4 ``int`` and ``float`` means any 32 or 64-bit integer or floating point value, respectively.
The exact type of value (e.g. if a variable or constant is 32-bit float or 64-bit double) is known.
('i'nteger - 32-bit integer, 'l'ong - 64-bit integer, 'f'loat - 32-bit foating point, 'd'ouble - 64-bit floating point) 

Integer calculations are done in 64-bit precision. When the result fits into the 32 bit range, it is stored back as 32-bit integer ``i``.
Otherwise the result is stored into 64-bit long ``l``.

When both operands are 32-bit float, arithmetic is done in 32-bits and the result is a 32-bit float.
Otherwise the calculation is done in 64-bit double ``d`` precision.

Except ``MulDiv`` and ``Rand`` the calculations are 64-bit (int or double) compatible.

Max
~~~
::

    Max(float, float [, ...])

Returns the maximum value of a set of numbers.

Version 3.7.4-:

- If all the values are of type Int (any 32 or 64-bit), the result is an Int. 
- If any of the values are of type 64-bit float (double), the result is a double.
  This may cause an unexpected result when an Int value greater than 9007199254740992
  (2^53) is mixed with double values.

- If all values are 32-bit float, the result is a 32-bit float.

Version <3.7.4:

- If all the values are of type Int, the result is an Int. If any of the values
  are of type Float, the result is a Float.
  
- This may cause an unexpected result when an Int value greater than 16777216 (2^24) 
  is mixed with Float values.

*Examples:*
::

    Max (1, 2) = 2
    Max (5, 3.0, 2) = 5.0

Min
~~~
::

    Min(float, float [, ...])

Returns the minimum value of a set of numbers.

*Examples:*
::

    Max (1, 2) = 1
    Max (5, 3.0, 2) = 2.0


MulDiv
~~~~~~
::

    MulDiv(int, int, int)

Multiplies two ints (m, n) and divides the product by a third (d) in a single
operation, with 64 bit intermediate result. The actual equation used is ``(m
* n + d / 2) / d``.

Parameters are treated as 32 bits, even from Avisynth 3.7.4.

*Examples:*
::

    MulDiv (1, 1, 2) = 1
    MulDiv (2, 3, 2) = 3

Floor
~~~~~
::

    Floor(float)

Converts from float to int (round down on any fractional amount).
An integer argument is returned unchanged. A value out of the 64 bit integer range, NaN or infinity is an error (since 3.7.6).

*Examples:*
::

    Floor(1.2) = 1
    Floor(1.6) = 1
    Floor(-1.2) = -2
    Floor(-1.6) = -2

Ceil
~~~~
::

    Ceil(float)

Converts from float to int (round up on any fractional amount).
An integer argument is returned unchanged. A value out of the 64 bit integer range, NaN or infinity is an error (since 3.7.6).

*Examples:*
::

    Ceil(1.2) = 2
    Ceil(1.6) = 2
    Ceil(-1.2) = -1
    Ceil(-1.6) = -1

Round
~~~~~
::

    Round(float)

Converts from float to int (round off to nearest integer). Halves are rounded away from zero
(``Round(2.5) = 3``, ``Round(-2.5) = -3``), exactly, also for large values (since 3.7.6).
An integer argument is returned unchanged. A value out of the 64 bit integer range, NaN or infinity is an error (since 3.7.6).

*Examples:*
::

    Round(1.2) = 1
    Round(1.6) = 2
    Round(-1.2) = -1
    Round(-1.6) = -2

Int
~~~
::

    Int(float)

Converts from single-precision, `floating-point`_ or any integer value to int (round towards
zero). If the result fit into 32 bit integer, the result type is integer, else 64 bit long.
A value out of the 64 bit integer range, NaN or infinity is an error (since 3.7.6).

*Examples:*
::

    Int(1.2) = 1
    Int(1.6) = 1
    Int(-1.2) = -1
    Int(-1.6) = -1

IntI
~~~~
::

    IntI(float)

Converts from a `floating-point`_ or any integer value to 32 bit int (round towards zero).
Values outside the 32 bit range wrap around, the same way for integer and floating-point
arguments: ``IntI(4294967297) = IntI(4294967297.0) = 1``.
A value out of the 64 bit integer range, NaN or infinity is an error (since 3.7.6).

Since v3.7.4.

*Examples:*
::

    IntI(1.2) = 1
    IntI(1.6) = 1
    IntI(-1.2) = -1
    IntI(-1.6) = -1

Long
~~~~
::

    Long(float)

Converts from a `floating-point`_ or any integer value to 64 bit int (round towards zero).
A value out of the 64 bit integer range, NaN or infinity is an error (since 3.7.6).

Since v3.7.4.

*Examples:*
::

    Long(1.2) = 1
    Long(1.6) = 1
    Long(-1.2) = -1
    Long(-1.6) = -1

Float
~~~~~
::

    Float(int)

Converts any int to `floating-point`_ value. 

Since v3.7.4 this means

- Integer input is converted to 64 bit double:
  Integer values that require more than 53-bits to be represented will have their lower bits
  truncated yielding unexpected values.
- if input is already a floating point type, it's kept as-is, 32 bit float is not converted to double.

Pre v3.7.4:

- Convert to 32 bit float.
- Integer values that require more than 24-bits to be represented will have their lower bits
  truncated yielding unexpected values.


*Examples:*
::

    Float(4) = 4.0
    Float(4) / 3 = 1.333 (while 4 / 3 = 1 , due to integer division)

Floatf
~~~~~~
::

    Floatf(int)

Converts int to 32 bit single-precision, `floating-point`_ value. Integer values
that require more than 24-bits to be represented will have their lower 8-bits
truncated yielding unexpected values.


Fmod
~~~~
::

    Fmod(float, float)

Returns the modulo of the argument. Output is float.

*Examples:*
::

    Fmod(3.5, 0.5) = 0 (since 3.5 - 7*0.5 = 0)
    Fmod(3.5, 1.0) = 0.5 (since 3.5 - 3*1.0 = 0.5)

Pi
~~
::

    Pi()

Returns the value of the "pi" constant (the ratio of a circle's circumference
to its diameter).

*Examples:*
::

    d = Pi()    # d == 3.141593

Exp
~~~
::

    Exp(float)

Returns the natural (base-e) exponent of the argument.

*Examples:*
::

    Exp(1) = 2.718282
    Exp(0) = 1.0

Log
~~~
::

    Log(float)

Returns the natural (base-e) logarithm of the argument.

*Examples:*
::

    Log(1) = 0.0
    Log(10) = 2.30259
    Log(Exp(1)) = 1.0

Log10
~~~~~
::

    Log10(float)

Returns the common logarithm of the argument.

*Examples:*
::

    Log10(1.0) = 0
    Log10(10.0) = 1.0
    Log10(2.0) = 0.3010299957

Pow
~~~
::

    Pow(float base, float power)

Returns "base" raised to the power indicated by the second argument.

*Examples:*
::

    Pow(2, 3) = 8
    Pow(3, 2) = 9
    Pow(3.45, 1.75) = 8.7334

Sqrt
~~~~
::

    Sqrt(float)

Returns the square root of the argument.

*Examples:*
::

    Sqrt(1) = 1.0
    Sqrt(2) = 1.4142

Abs
~~~
::

    Abs(float or int)

Returns the absolute value of its argument (returns float for float, integer
for integer).

*Examples:*
::

    Abs(-3.8) = 3.8
    Abs(-4) = 4

Sign
~~~~
::

    Sign(float)

Returns the sign of the value passed as argument (1, 0 or -1).

*Examples:*
::

    Sign(-3.5) = -1
    Sign(3.5) = 1
    Sign(0) = 0

Frac
~~~~
::

    Frac(float)

Returns the fractional portion of the value provided.
The result is 32-bit float for a 32-bit float argument, otherwise 64-bit double (since 3.7.6).

*Examples:*
::

    Frac(3.7) = 0.7
    Frac(-1.8) = -0.8

Rand
~~~~
::

    Rand([int max] [, bool scale] [, bool seed])

Returns a random integer value. All parameters are optional.

-   *max* sets the maximum value+1 (default 32768) and can be set
    negative for negative results. It operates either in scaled or modulus
    mode (default scale=true only if abs(max) > 32768, false otherwise).
-   Scaled mode (scale=true) scales the internal random number
    generator value to the maximum value, while modulus mode (scale=false)
    uses the remainder from an integer divide of the random generator value
    by the maximum. I found modulus mode is best for smaller maximums.
-   Using *seed=true* seeds the random number generator with the current
    time. *seed* defaults to false and probably isn't necessary, although
    it's there just in case.

Typically, this function would be used with the Select function for random
clips.

*Examples:*
::

    Select(Rand(5), clip1, clip2, clip3, clip4, clip5)

Spline
~~~~~~
::

    Spline(float X, x1, y1, x2, y2, .... [, bool cubic])

Interpolates the Y value at point X using the control points x1/y1, ... There
have to be at least 2 x/y-pairs. The interpolation can be cubic (the result
is a spline) or linear (the result is a polygon). Default is cubic.
The calculation is done in 64-bit double precision, the result is 64-bit double (since 3.7.6).

*Examples:*
::

    Spline(5, 0, 0, 10, 10, 20, 0, false) = 5
    Spline(5, 0, 0, 10, 10, 20, 0, true) = 6.875

ContinuedNumerator, ContinuedDenominator
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
::

    ContinuedNumerator(float, int limit)
    ContinuedNumerator(int, int, int limit)
    ContinuedDenominator(float, int limit)
    ContinuedDenominator(int, int, int limit)

The rational pair (ContinuedNumerator,ContinuedDenominator) returned has the
smallest possible denominator such that the absolute error is less than
1/limit. More information can be found on `the wikipedia article for Continued fraction`_. If *limit* is not
specified in the Float case the rational pair returned is to the limit of the
single precision floating point value. Thus ``(float)((double)Num/(double)Den)
== V``. In the Int pair case if *limit* is not specified then the normalised
original values will be returned, i.e. reduced by the GCD. (GCD = greatest common divisor)

*Examples:*
::

    ContinuedNumerator(PI(), limit=5000]) = 355
    ContinuedDenominator(PI(), limit=5000) = 113

    ContinuedNumerator(PI(), limit=50]) = 22
    ContinuedDenominator(PI(), limit=50) = 7

    ContinuedNumerator(355, 113, limit=50]) = 22
    ContinuedDenominator(355, 113, limit=50) = 7

Changelog
~~~~~~~~~
+-----------------+-----------------------------------+
| Version         | Changes                           |
+=================+===================================+
| 3.7.6           | | Spline: 64 bit double           |
|                 | | Frac: double for double input   |
|                 | | Floor, Ceil, Round, Int, IntI,  |
|                 |   Long: out of range is an error  |
|                 | | Round: exact halves             |
+-----------------+-----------------------------------+
| 3.7.4           | | Changed Float, Int              |
|                 | | Add IntI, Long, FloatF, Double  |
+-----------------+-----------------------------------+
| Avisynth 2.6    | Fmod, Log10,                      |
|                 | ContinuedNumerator,               |
|                 | ContinuedDenominator              |
+-----------------+-----------------------------------+

--------

Back to :doc:`Internal functions <syntax_internal_functions>`.

$Date: 2026/10/20 13:00:00 $

.. _wikipedia: http://en.wikipedia.org/wiki/Hyperbolic_function
.. _floating-point: http://en.wikipedia.org/wiki/Floating_point
.. _the wikipedia article for Continued fraction: http://en.wikipedia.org/wiki/Continued_fraction
