
AviSynth Syntax - Type conversion functions
===========================================

Conversion functions convert between different types. There are also some
:doc:`numeric functions <syntax_internal_functions_numeric>` that can be classified in this 
category, namely: ``Ceil``, ``Floor``, ``Float``, ``Int``, ``IntI``, ``Long``, ``FloatF``,
``Double`` and ``Round``.

See :doc:`AviSynth Syntax - Numeric functions <syntax_internal_functions_numeric>`.

Value
~~~~~
::

    Value(string)

Converts a decimal string to its associated numeric value.

*Examples:*
::

    Value ("-2.7") = -2.7


HexValue
~~~~~~~~
HexValue64
~~~~~~~~~~
::

    HexValue(string[, int "pos"])
    HexValue64(string[, int "pos"])

Converts a hexadecimal string to its associated numeric value.

HexValue returns 32 bit integer.

HexValue64 returns 64 bit long. (since v3.7.4)

Conversion will cease at the first non legal number base digit, without 
producing an error.

Function returns 0 if ``pos`` is erroenus: ie less than 1 or greater than string length.

.. describe:: int pos (default 1)

    Sets the starting point of the numeric parser. All characters to the 
    left of pos are ignored. Default is 1 (start of string).

*Examples:*
::

    HexValue ("FF00") # 65280
    HexValue ("FFA0", 3) # 160
    HexValue ("FFFFFFFF") # -1
    HexValue64 ("FF00") # 65280
    HexValue64 ("FFFFFFFF") # 4294967295


Hex
~~~
::

    Hex(int [, int "width"])

Converts a numerical value to its hexadecimal value string. See `Colors`_ for
more information on specifying colors.

Function returns hex string in uppercase, instead of lowercase.

The value is handled as 32 bit integer: a negative value gives 8 digits
(``Hex(-1)`` = "FFFFFFFF"), a 64 bit value is cut to its lower 32 bits. For 64 bit values use ``Hex64``.

.. describe:: int width (default 0)

    Width is 0 to 8, the *minimum* width of the returned string (8 hex digits is the
    max of an Avisynth 32 bit integer).

    Resulting string will be left-padded with zeroes as needed.

    When width is 0 (default) or not supplied then string length is a minimum needed.

*Examples:*
::

    Hex(10824234) # "A52A2A"
    Hex(255,4) # returns "00FF".

Hex64
~~~~~
::

    Hex64(int [, int "width"])

64 bit version of ``Hex``, the pair of ``HexValue64``. Converts a 32 or 64 bit integer value
to its hexadecimal value string (uppercase), handled as 64 bit: a negative value gives 16 digits.

.. describe:: int width (default 0)

    Width is 0 to 16, the *minimum* width of the returned string.
    Resulting string will be left-padded with zeroes as needed.

*Examples:*
::

    Hex64(255)          # "FF"
    Hex64(255, 4)       # "00FF"
    Hex64(-1)           # "FFFFFFFFFFFFFFFF" (Hex(-1) is "FFFFFFFF")
    Hex64(4294967297)   # "100000001"        (Hex(4294967297) is "1")
    HexValue64(Hex64(x)) == x # true for any integer x

Since 3.7.6.

String
~~~~~~
::

    String(float / int [, string format_string])

Converts a variable to a string. 

- String arguments are passed along unchanged; 
- bools are converted to "true" or "false"; 
- numbers are formatted as described below; 
- other variable types (clip, val) are converted to the empty string. 

If the variable is float or integer and ``format_string`` exists, the value is converted
according to the conversion letter of ``format_string``: for a floating point conversion (e.g. ``f``)
it is converted to float, for an integer conversion (e.g. ``d``, since 3.7.6) to integer.
The syntax of ``format_string`` is as follows:

- ``%[flags][width][.precision]f``

  the leading ``'%'`` and trailing ``'f'`` are required!

  Instead of ``f``, other conversions are accepted as well (since 3.7.6):

  - floating point: ``f F e E g G a A``. Integer values are converted to float.
  - integer: ``d i u o x X``. Floating point values are truncated toward zero, like ``Int``
    (an error out of the 64 bit range). A 64 bit long value is printed with all its digits;
    a 32 bit int is formatted as 32 bit, e.g. ``String(-1, "%x")`` = 'ffffffff' like ``Hex``.

  C type size prefixes before the conversion letter (``h``, ``l``, ``ll``, ``L``, ``I64``, e.g. the
  ``l`` in ``%lf`` or ``%lld``) are accepted and ignored: the value's own type decides (32 or 64 bit
  integer; floating point is always printed with double precision). Exactly one conversion is allowed; ``%%`` prints a ``%`` sign. Anything else
  (e.g. ``%s``, ``%c``, ``%n``, ``*`` width, a second conversion, a lone ``%``) is an error
  (since 3.7.6; before it printed garbage or crashed).

  - *width*: the minimum width (the string is never truncated if it is wider than width)
  - *precision*: the number of digits printed
  - *flags*:

    - ``-`` left align (instead right align)
    - ``+`` always print the +/- sign (show only '+' by default)
    - ``0`` pad (see width) with leading zeroes (pad with spaces by default)  
    - ``' '`` print a blank instead of a "+"
    - ``#`` always print the decimal point (dropped by default if there are no decimal digits)

You can also put arbitrary text around the format_string as defined above, similar to the C-language *printf* function.

*Examples:*
::

    Subtitle( String(1.23) )                    # '1.230000' (six decimals by default for floats)
    Subtitle( String(123) )                     # '123'      (no decimals by default for ints)
    
    Subtitle( String(1.23, "%0.2f" ))           # '1.23'
    Subtitle( String(1.23, "%0.1f" ))           # '1.2'
    Subtitle( String(1.23, "%5.1f") )           # '  1.2'    (padded to 5 characters wide)
    Subtitle( String(1.23, "%1.3f") )           # '1.230'    (3 decimals; add trailing zeroes)
    
    Subtitle( String(123, "%0.0f") )            # '123'      (no decimals for precision=0)
    Subtitle( String(123, "%#0.0f") )           # '123.'     ('#' flag: always show decimal point)
    Subtitle( String(123, "%0.2f") )            # '123.00'   (2 decimals: add trailing zeroes)
    Subtitle( String(123, "%5.0f") )            # '  123'    (padded to 5 characters wide using ' ')
    Subtitle( String(123, "%05.0f") )           # '00123'    (padded to 5 characters wide using '0')
    
    Subtitle( String(PI, "PI=%0.0f") )          # 'PI=3'     (text around format_string)
    Subtitle( String(PI, "PI=%#0.0f") )         # 'PI=3.'    ('#' flag: always show decimal point)
    Subtitle( String(PI, "PI=%2.0f") )          # 'PI= 3'
    Subtitle( String(PI, "PI=%3.2f") )          # 'PI=3.14'
    Subtitle( String(PI, "PI=%0.5f") )          # 'PI=3.14159'
    Subtitle( String(PI, "PI=%6.3f") )          # 'PI= 3.142'
    
    Subtitle( String(32, "%0.0f") )             # '32'
    Subtitle( String(32, "%3.0f") )             # ' 32'
    Subtitle( String(32, "%8.0f") )             # '      32'

    # integer conversions (since 3.7.6)
    Subtitle( String(123, "%05d") )             # '00123'
    Subtitle( String(255, "%x") )               # 'ff'
    Subtitle( String(255, "%#06X") )            # '0X00FF'   (width 6 includes the '0X' prefix)
    Subtitle( String(2.7, "%d") )               # '2'        (truncated like Int)
    Subtitle( String(9007199254740993, "%d") )  # '9007199254740993' (64 bit, exact)
    Subtitle( String(50, "%d%%") )              # '50%'

::

    ## arbitrary text around format_string:
    Subtitle( String(Last.Height, "Clip height is %0.0f") ) # 'Clip height is 480'
    ## same output as above but using string concatenation:
    Subtitle( "Clip height is " + String(Last.Height) )
    
    Subtitle( String(x, "Value of x is %.3f after AR calc") )
    Subtitle( "Value of x is " + String(x, "%.3f") + " after AR calc") )
    # same as above

How the format string is processed (since 3.7.6)
------------------------------------------------

The format string is checked and normalized before it is passed to the C ``printf`` family:
text and ``%%`` are kept, flags, width and precision are kept, a C type size prefix is dropped,
and the argument is passed with the type the conversion letter expects
(the 64 bit prefix of the current compiler is inserted when needed).

So the same format string works with any compiler and on any OS: a script never needs the
platform dependent 64 bit prefixes (``%lld`` on Linux/macOS and newer Windows runtimes,
``%I64d`` on old Microsoft runtimes). This also brings 64 bit integer support: a 64 bit long
value is printed exactly, with all its digits, by any integer conversion, e.g. ``"%d"`` or ``"%x"``.

.. list-table::
   :header-rows: 1

   * - format string
     - value
     - passed to printf
   * - ``"%lf"``
     - 1.5
     - ``"%f"``, double
   * - ``"%05.0f"``
     - 123 (int)
     - ``"%05.0f"``, double 123.0
   * - ``"%x"``
     - 255 (int)
     - ``"%x"``, 32 bit int
   * - ``"%d"``
     - 9007199254740993 (long)
     - ``"%lld"`` (compiler dependent), 64 bit int
   * - ``"%I64d"``
     - 5 (int)
     - ``"%d"``, 32 bit int
   * - ``"%d"``
     - 2.7 (float)
     - ``"%d"``, 32 bit int 2 (truncated like ``Int``)
   * - ``"%d%%"``
     - 50
     - ``"%d%%"``, 32 bit int

Before 3.7.6 the format string was passed unchecked, always with a floating point argument.
The floating point conversions worked, only ``%f`` was documented:

.. list-table::
   :header-rows: 1

   * - format string
     - before 3.7.6
     - since 3.7.6
   * - ``f F e E g G a A`` (e.g. ``"%.2f"``, ``"%lf"``)
     - correct
     - same result
   * - ``d i u o x X``, e.g. ``String(5, "%d")``, ``String(255, "%x")``, ``String(2.7, "%d")``
     - garbage: ``0``, ``0``, ``-1717986918``
     - ``5``, ``ff``, ``2``
   * - 64 bit value, e.g. ``String(9007199254740993, "%d")``
     - garbage (``0``)
     - ``9007199254740993``
   * - ``%Lf`` (long double)
     - crash (where long double is 80 bit)
     - same as ``%f``
   * - ``%s``, ``%n``, ``%p``, ``%c``
     - crash or garbage
     - error
   * - two conversions, e.g. ``"%f %f"``
     - second one printed stack garbage
     - error
   * - ``*`` width or precision, a lone ``%``
     - garbage
     - error


Changelog
---------
+-----------------+----------------------------------+
| Version         | Changes                          |
+=================+==================================+
| 3.7.6           | | Add Hex64                      |
|                 | | String: integer conversions    |
|                 |   (d i u o x X), more float ones,|
|                 |   invalid format is an error     |
+-----------------+----------------------------------+
| 3.7.4           | | Add HexValue64                 |
|                 | | Add IntI, Long, FloatF, Double |
+-----------------+----------------------------------+
| Avisynth+ r2632 | | Hex: added "width"             |
|                 | | Hexvalue: added "pos"          |
+-----------------+----------------------------------+


--------

Back to :doc:`Internal functions <syntax_internal_functions>`.

$Date: 2026/10/20 13:00:00 $

.. _Colors: http://avisynth.nl/index.php/Colors
