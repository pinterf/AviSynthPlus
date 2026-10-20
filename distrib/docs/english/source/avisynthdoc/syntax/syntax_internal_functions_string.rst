
AviSynth Syntax - String functions
==================================

String functions provide common operations on string variables.

These function may not work as expected, if you feed them with utf8 strings.
Some function (e.g. UCase, LCase) may not work as expected, if parameter contains national accented characters.

E.g. Strlen of an utf8-encoded string, which contains non-latin or accented national characters 
may report more length than is visible by human reader.

LCase
~~~~~
::

    LCase(string)

Returns lower case of string.

*Examples:*
::

    LCase("AviSynth") = "avisynth"

UCase
~~~~~
::

    UCase(string)

Returns upper case of string.

*Examples:*
::

    UCase("AviSynth") = "AVISYNTH"

StrToUtf8
~~~~~~~~~
::

    StrToUtf8(string)

Converts string from ANSI to UTF8. 

Note, that since Windows 10 the codepage for non-unicode programs can
be set to UTF8 natively. ANSI is meant to be the active code page of the system.

StrFromUtf8
~~~~~~~~~~~
::

    StrFromUtf8(string)

Converts string from UTF8 to ANSI.

Valid on Windows.

Note, that since Windows 10 the codepage for non-unicode programs can
be set to UTF8 natively.

StrLen
~~~~~~
::

    StrLen(string)

Returns length of string.

*Examples:*
::

    StrLen("AviSynth") = 8

RevStr
~~~~~~
::

    RevStr(string)

Returns string backwards.

*Examples:*
::

    RevStr("AviSynth") = "htnySivA"

LeftStr
~~~~~~~
::

    LeftStr(string, int)

Returns first int number of characters.

*Examples:*
::

    LeftStr("AviSynth", 3) = "Avi"

RightStr
~~~~~~~~
::

    RightStr(string, int)

Returns last int number of characters.

*Examples:*
::

    RightStr("AviSynth", 5) = "Synth"

MidStr
~~~~~~
::

    MidStr(string, int pos [, int length])

Returns substring starting at ``pos`` for optional ``length`` or to end. ``pos=1``
specifies start.

*Examples:*
::

    MidStr("AviSynth", 3, 2) = "iS"

FindStr
~~~~~~~
::

    FindStr(string, substring)

Returns position of ``substring`` within ``string``.

Returns 0 if substring is not found.

*Examples:*
::

    FindStr("AviSynth", "Syn") ## returns 4
    FindStr("AviSynth", "SYN") ## returns 0

ReplaceStr
~~~~~~~~~~
::

    ReplaceStr(string, substring, replacement_string [, bool sig])

Replaces occurrences of ``substring`` with ``replacement_string`` and returns the result.

.. describe:: bool sig

    - false (the default): search is case sensitive
    - true: search is not case insensitive.

*Example:*
::

    ReplaceStr("FlipHorizontal", "Horizontal", "Vertical")
    ReplaceStr("$a x *", "$a", String(1.5)) ## (a MaskTools expression with argument)
    ## this latter is more elegant with using "Format"

Avisynth 2.6.x users have other options, such as the user function below, adapted from StrReplace, 
found `here <https://avisynth.nl/index.php/HDColorBars>`_ .

::

    # for old systems where ReplaceStr is missing:
    function ReplaceStr(string base, string sought, string repstr) {
        pos = FindStr(base, sought)
        return (sought=="" || pos==0) ? base : ReplaceStr(
        \       LeftStr(base, pos-1) + repstr +
        \       MidStr(base, pos+StrLen(sought)),
        \       sought, repstr)
    }

Format
~~~~~~
::

    Format(formatstring [, value1, value2, ...])

Replaces the in-string placeholders with parameters and returns the result

Parameters can be of any type; each parameter is converted to string beforehand.

.. describe:: string formatstring

    unnamed parameter. A string literal with parameter insertion points marked with ``{}``.

.. describe:: value1, value2, etc.

    zero or more values which are inserted into the format string one after another 

Description:

    The format string consists of 

    - ordinary characters ( except ``{`` and ``}`` ), which are copied unchanged to the output,
    - escape sequences: double ``"{"`` (``"{{"``) and double ``"}"`` (``"}}"``), which are replaced with 
      ``"{"`` and ``"}"`` respectively in the output
    - replacement placeholder fields. 

Each replacement field has the following format:

::

    introductory { character; 
    (optional) 
        arg-id, a non-negative number;
      or: 
        identifier which is used for lookup named parameters. (when values are given in ["name", value] construction)
      or: 
        valid AviSynth variable name 
    (optional, since 3.7.6)
        : character followed by a format specification for numbers, e.g. {:.2f} {0:x} {name:05d}
    final } character. 

- If arg-id is a number it specifies the index of the argument 
  in args whose value is to be used for formatting; 
- Index is zero based. 
- If arg-id is string then it serves as a lookup key from the 
  parameters list given as an array ["name",value] pair. 
- If not found, then arg-id is searched among Avisynth variables. 
- If arg-id is omitted, the arguments are used in order. 
- Mixing manual and automatic indexing is not an error. 
- The optional format specification after ``:`` (since 3.7.6) is a printf-like single numeric
  conversion, the leading ``%`` is optional (Python style ``{:.2f}`` or ``{:%.2f}``):
  ``[%][flags][width][.precision]`` and one of ``d i u o x X f F e E g G a A``.
  It works exactly as the format string of ``String(x, format_string)``: the value is converted
  according to the conversion letter (an integer can be printed with ``f``, a floating point value
  with ``d`` is truncated), 64 bit integers are printed exactly, C type size prefixes (``l``,
  ``ll``, ``I64``) are not needed. It is allowed only for numbers; no other text may be in it.
  ``{a:}`` (empty specification) is the same as ``{a}``.

Notes

    It is not an error to provide more arguments than the format string requires: 

::

    Format("{} {}!", "Hello", "world", "something") # OK, produces "Hello world!"

Number formatting

    Integers are inserted with all digits. Floating point values are inserted with the shortest
    representation which reads back to exactly the same value, in the same way as Python's ``repr()``
    (since 3.7.6, before: 6 fixed decimals like ``String``, which silently falsified expressions built
    with ``Format``, e.g. in ``Expr(Format("x {a} *"))`` ``1.0/65535`` became ``0.000015`` and values
    below 5e-7 became ``0``):

    - positional notation between 0.0001 and 1e16, otherwise scientific notation with ``e``,
      a sign and at least two exponent digits;
    - a whole value keeps its ``.0``, so it is read back as a floating point number;
    - ``.`` is the decimal separator, independent of the locale;
    - a 32 bit float value gets its own shortest digits (``Floatf(0.1)`` gives "0.1");
    - not-a-number and infinity are ``nan``, ``inf`` and ``-inf``.

    So the result can be used to build script text for ``Eval`` or an ``Expr`` expression without
    losing precision. For a fixed number of decimals, e.g. for display, use a format specification
    (``{:.2f}``, since 3.7.6) or ``String(x, "%.2f")``.

::

    Format("{}", 1.5)        # "1.5"
    Format("{}", 1.0)        # "1.0"
    Format("{}", 1.0/3)      # "0.3333333333333333"
    Format("{}", 0.0001)     # "0.0001"
    Format("{}", 1.0/65535)  # "1.5259021896696422e-05"
    Format("{}", Pow(2.0,70)) # "1.1805916207174113e+21"
    Format("{}", 255)        # "255"
    Expr(Format("x {} *", 1.0/65535)) # exact constant in the expression

    # with format specification (since 3.7.6)
    Format("{:.2f}", Pi())                         # "3.14"
    Format("{0:.3f} {0:e}", Pi())                  # "3.142 3.141593e+00"
    Format("{:05d}|{:x}|{:#06X}", 42, 255, 255)    # "00042|ff|0X00FF" (width 6 includes the "0X" prefix)
    Format("{w:5d}x{h:-5d}|", ["w", 640], ["h", 480]) # "  640x480  |"
    Format("{:.0f}", 123)                          # "123"   (integer printed as float)
    Format("{:d}", 2.7)                            # "2"     (truncated like Int)
    Format("{:x}", 4294967297)                     # "100000001" (64 bit)
    Format("{:5.1f}%", 99.44)                      # " 99.4%" (text outside {} is not printf: no %% needed)
    my_coeff = 1.0/65535
    Format("x {my_coeff:.8f} *")                   # "x 0.00001526 *" (variable, with specification)

*Examples:*

::

    # By Avisynth variable
    max_pixel_value = 255
    SubTitle(Format("max={max_pixel_value}!")) # no format value given, inserts directly from variable
    # result: "max=255!"

::

    # By index:
    SubTitle(Format("{0} {1} {0}", "Home", "sweet"))
    # result: "Home sweet Home"

::

    # by order:
    SubTitle(Format("{} {} {}", "AviSynth", "+", 2020))
    # result: "AviSynth + 2020"

::

    # by Array name-value pairs:
    SubTitle(Format("maximum={max} minimum={min} max again {max}!", ["max",255], ["min",0]))
    # result: ""maximum=255 minimum=0 max again 255!"


FillStr
~~~~~~~
::

    FillStr(int [, string]) Fills a string.

When ``int > 1`` it concatenates the string ``int`` times. ``String`` is space by default.

*Examples:*
::

    FillStr(1, "AviSynth") = "AviSynth"
    FillStr(2, "AviSynth") = "AviSynthAviSynth"

StrCmp
~~~~~~
::

    StrCmp(string, string)

Compares two character strings. The comparison is case-sensitive. If the
first string is less than the second string, the return value is negative. If
it's greater, the return value is positive. If they are equal, the return
value is zero. (The actual value seems to be host operating system dependent
so it can't be relied upon.)

*Examples:*
::

    StrCmp("AviSynth", "AviSynth") = 0 # strings are equal.
    StrCmp("AviSynth", "Avisynth") != 0 # strings are not equal.

StrCmpi
~~~~~~~
::

    StrCmpi(string, string)

Compares two character strings. The comparison is not case-sensitive. If the
first string is less than the second string, the return value is negative. If
it's greater, the return value is positive. If they are equal, the return
value is zero. (The actual value seems to be host operating system dependent
so it can't be relied upon.)

*Examples:*
::

    StrCmpi("AviSynth", "AviSynth") = 0 # strings are equal.
    StrCmpi("AviSynth", "Avisynth") = 0 # strings are equal.
    StrCmpi("abcz", "abcdefg") != 0 # returns the difference betweeen "z"
    and "d" (which is positive).

TrimLeft, TrimRight, TrimAll
~~~~~~~~~~~~~~~~~~~~~~~~~~~~
::

    TrimLeft(string)
    TrimRight(string)
    TrimAll(string)

Removes whitespace characters (space, tab, `nonbreaking space <https://en.wikipedia.org/wiki/Non-breaking_space>`_
) from the left, right, or both ends (respectively) of ``string``.

Chr
~~~
::

    Chr(int)

Returns the ASCII character. Note that characters above the ASCII character
set (ie above 127) are code page dependent and may render different (visual)
results in different systems. This has an importance only for user-supplied
localised text messages.

*Examples:*
::

    Chr(34) returns the quote character
    Chr(9) returns the tab  character

Ord
~~~
::

    Ord(string)

Gives the ordinal number of the  first character of a string.

*Examples:*
::

    Ord("a") = 97
    Ord("AviSynth") = Ord("A") = 65
    Ord("§") = 167

Time
~~~~
::

    Time(string)

Returns a string with the current system time formatted as defined by the
string. The string may contain any of the codes for output formatting
presented below:

+--------+---------------------------------------------------+
| Code   | Description                                       |
+========+===================================================+
| %a     | Abbreviated weekday name                          |
|        |                                                   |
| %A     | Full weekday name                                 |
+--------+---------------------------------------------------+
| %b     | Abbreviated month name                            |
|        |                                                   |
| %B     | Full month name                                   |
+--------+---------------------------------------------------+
| %c     | Date and time representation                      |
|        | appropriate for locale                            |
+--------+---------------------------------------------------+
| %d     | Day of month as decimal number (01 ? 31)          |
+--------+---------------------------------------------------+
| %H     | Hour in 24-hour format (00 ? 23)                  |
|        |                                                   |
| %I     | Hour in 12-hour format (01 ? 12)                  |
+--------+---------------------------------------------------+
| %j     | Day of year as decimal number (001 ? 366)         |
+--------+---------------------------------------------------+
| %m     | Month as decimal number (01 ? 12)                 |
+--------+---------------------------------------------------+
| %M     | Minute as decimal number (00 ? 59)                |
+--------+---------------------------------------------------+
| %p     | Current locale's A.M./P.M.                        |
|        | indicator for 12-hour clock                       |
+--------+---------------------------------------------------+
| %S     | Second as decimal number (00 ? 59)                |
+--------+---------------------------------------------------+
| %U     | Week of year as decimal number,                   |
|        | with Sunday as first day of week (00 ? 53)        |
+--------+---------------------------------------------------+
| %w     | Weekday as decimal number (0 ? 6; Sunday is 0)    |
+--------+---------------------------------------------------+
| %W     | Week of year as decimal number,                   |
|        | with Monday as first day of week (00 ? 53)        |
+--------+---------------------------------------------------+
| %x     | Date representation for current locale            |
+--------+---------------------------------------------------+
| %X     | Time representation for current locale            |
+--------+---------------------------------------------------+
| %y     | Year without century, as decimal number (00 ? 99) |
|        |                                                   |
| %Y     | Year *with* century, as decimal number            |
+--------+---------------------------------------------------+
| %z, %Z | Time-zone name or abbreviation;                   |
|        | no characters if time zone is unknown             |
+--------+---------------------------------------------------+
| %%     | Percent sign                                      |
+--------+---------------------------------------------------+

The # flag may prefix any formatting code. In that case, the meaning of the
format code is changed as follows:

+-------------------------------+---------------------------------------------------------------------------------+
| Code with # flag              | Change in meaning                                                               |
+===============================+=================================================================================+
| %#a, %#A, %#b, %#B,           |                                                                                 |
|                               | No change; # flag is ignored.                                                   |
| %#p, %#X, %#z, %#Z, %#%       |                                                                                 |
+-------------------------------+---------------------------------------------------------------------------------+
| %#c                           || Long date and time representation, appropriate for current locale. For example:|
|                               || ``?Tuesday, March 14, 1995, 12:41:29?.``                                       |
+-------------------------------+---------------------------------------------------------------------------------+
| %#x                           || Long date representation, appropriate to current locale. For example:          |
|                               || ``?Tuesday, March 14, 1995?.``                                                 |
+-------------------------------+---------------------------------------------------------------------------------+
| %#d, %#H, %#I, %#j, %#m, %#M, |                                                                                 |
|                               | Remove leading zeros (if any).                                                  |
| %#S, %#U, %#w, %#W, %#y, %#Y  |                                                                                 |
+-------------------------------+---------------------------------------------------------------------------------+


Changelog
~~~~~~~~~
+----------------+--------------------------------------------+
| Version        | Changes                                    |
+================+============================================+
| 3.7.6          | | "Format": shortest round-trip floating   |
|                |   point output (was 6 fixed decimals)      |
|                | | "Format": format specification ``{:.2f}``|
+----------------+--------------------------------------------+
| Avisynth+      | | Added "Format"                           |
|                | | Added "StrToUtf8"                        |
|                | | Added "StrFromUtf8"                      |
|                | | Added "ReplaceStr"                       |
|                | | Added "Format"                           |
|                | | Added "TrimLeft", "TrimRight", "TrimAll" |
+----------------+--------------------------------------------+


--------

Back to :doc:`Internal functions <syntax_internal_functions>`.

$Date: 2024/01/08 14:58:00 $
