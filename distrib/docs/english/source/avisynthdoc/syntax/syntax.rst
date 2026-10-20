
Getting started
===============


.. toctree::
    :maxdepth: 3

.. contents:: Table of contents



Basically, AviSynth works like this: First, you create a simple text document
with special commands, called a script. These commands make references to one
or more videos and the filters you wish to run on them. Then, you run a video
application, such as VirtualDub, and open the script file. This is when
AviSynth takes action. It opens the videos you referenced in the script, runs
the specified filters, and feeds the output to video application. The
application, however, is not aware that AviSynth is working in the
background. Instead, the application thinks that it is directly opening a
filtered AVI file that resides on your hard drive.

There is much new and re-discovered functionality in AviSynth2. To make those
items clearly visible (especially when the feature was not well documented in
the past) they are marked with **v2**

The version 2.5 is a major internal upgrade. Read :doc:`AviSynth 2.5 <../twopointfive>` carefully
before you use it. Relevant changes are marked with **v2.5**

Linear Editing
--------------

The simplest thing you can do with AviSynth is the sort of editing you can do
in VirtualDub. The scripts for this are easy to write because you don't have
to worry about variables and complicated expressions if you don't want.

For testing create a file called test.avs and put the following single line
of text in it:
::

    Version

Now open this file with e.g. Windows Media Player and you should see a ten-
second video clip showing AviSynth's version number and a copyright notice.

``Version`` is what's called a "source filter", meaning that it generates a
clip instead of modifying one. The first command in an AviSynth script will
always be a source filter.

Now add a second line to the script file, so that it reads like this:
::

    Version
    ReduceBy2

Reopen the file in Media Player. You should see the copyright notice again,
but now half as large as before.
:doc:`ReduceBy2 <../corefilters/reduceby2>` is a "transformation filter," meaning that it takes the
previous clip and modifies it in some way. You can chain together lots of
transformation filters, just as in VirtualDub.
Let's add another one to make the video fade to black at the end. Add another
line to the script file so that it reads:
::

    Version
    ReduceBy2
    FadeOut(10)

Now reopen the file. The clip should be the same for the first 9 seconds, and
then in the last second it should fade smoothly to black.
The :doc:`FadeOut <../corefilters/fade>` filter takes a numerical argument, which indicates the number
of frames to fade.

It takes a long time before the fade starts, so let's trim the beginning of
the clip to reduce the wait, and fade out after that.
Let's discard the first 120 of them, and keep the frames 120-150:
::

    Version
    ReduceBy2
    # Chop off the first 119 frames, and keep the frames 120-150
    # (AviSynth starts numbering frames from 0)
    Trim(120,150)
    FadeOut(10)

In this example we used a comment for the first time.
Comments start with the # character and continue to the end of the line, and
are ignored completely by AviSynth.
The :doc:`Trim <../corefilters/trim>` filter takes two arguments, separated by a comma: the first and
the last frame to keep from the clip. If you put 0 for the last frame, it's
the same as "end of clip," so if you only want to remove the first 119 frames
you should use Trim(120,0).

Keeping track of frame numbers this way is a chore. It's much easier to open
a partially-completed script in an application like VirtualDub which will
display the frame numbers for you. You can also use the :doc:`ShowFrameNumber <../corefilters/showframes>`
filter, which prints each frame's number onto the frame itself.

In practice a much more useful source filter than :doc:`Version <../corefilters/version>` is :doc:`AVISource <../corefilters/avisource>`,
which reads in an AVI file (or one of several other types of files) from
disk. If you have an AVI file handy, you can try applying these same filters
to your file:
::

    AVISource("d:\capture.avi")  # or whatever the actual pathname is
    ReduceBy2
    FadeOut(15)
    Trim(120,0)

Even a single-line script containing only the AVISource command can be useful
for adding support for >2GB AVI files to applications which only support <2GB
ones.


--------


Non-Linear Editing
------------------

Now we're getting to the fun part. Make an AVS file with the following script
in it:
::

    StackVertical(Version, Version)

Now open it. Result: An output video with two identical lines of version
information, one on top of the other.
Instead of taking numbers or strings as arguments, :doc:`StackVertical <../corefilters/stack>` takes
video clips as arguments. In this script, the Version filter is being called
twice. Each time, it returns a copy of the version clip. These two clips are
then given to :doc:`StackVertical <../corefilters/stack>`, which joins them together (without knowing
where they came from).

One of the most useful filters of this type is :doc:`UnalignedSplice <../corefilters/splice>`, which
joins video clips end-to-end. Here's a script which loads three AVI files
(such as might be produced by AVI_IO) and concatenates them together.
::

    UnalignedSplice(AVISource("d:\capture.00.avi"), \
    AVISource("d:\capture.01.avi"), \
    AVISource("d:\capture.02.avi"))

Both :doc:`StackVertical <../corefilters/stack>` and :doc:`UnalignedSplice <../corefilters/splice>` can take as few as two arguments
or as many as sixty.
You can use the ``+`` operator as a shorthand for :doc:`UnalignedSplice <../corefilters/splice>`.

For example, this script does the same thing as the previous example:
::

    AVISource("d:\capture.00.avi") + \
    AVISource("d:\capture.01.avi") + \
    AVISource("d:\capture.02.avi")

Now let's suppose you're capturing with an application that also saves the
video in multiple AVI segments, but puts the audio in a separate WAV file.
Can we recombine everything? You bet:
::

    AudioDub(AVISource("d:\capture.00.avi") + \
    AVISource("d:\capture.01.avi") + \
    AVISource("d:\capture.02.avi"), \
    WAVSource("d:\audio.wav"))

--------


Syntax
------


Expressions
~~~~~~~~~~~

An AviSynth script consists of multiple lines of statements looking like
this:
::

    variable_name = expression

In this example expression is evaluated and the result is assigned to
variable_name.

Very important is the common shortcut form:
::

    expression

In this case, expression is evaluated and the result is assigned to the
special clip variable last.
This is the same as
::

    last = expression

The end of the script often looks like:
::

    return expression

Here expression is evaluated and is used as the "return value" of the script
--that is, the video clip that will be seen by the application which opens
the AVS file.

If "return" is not specified explicitly, the clip last is used as a "return
value".

The basic form of an expression which invokes a function is
::

    Function(args)

Clip functions always produce a new video clip and never modify an existing
one,
Args is a list of function arguments separated by commas. The list can be
empty (which means all or some arguments can be optional)

If the filter function expects a video clip as its first argument, and that
argument is not supplied, then the clip in the special variable last will be
used.

AviSynth filters can take named arguments. The named arguments can be
specified in any order,
and the filter will choose default values for any that you leave off (named
arguments are always optional).
This makes certain filters much easier to use. You can write
::

    Subtitle("Hello, World!", text_color=$00FF00, x=100, y=200)

instead of
::

    Subtitle("Hello, World!", 100, 200, 0, 999999, "Arial", 24, $00FF00)

An alternate syntax (called "OOP notation") for clip functions is
::

    expression.Function(args)

    e.g.:
    Version.ReduceBy2.FadeOut(15)

This is equivalent to
::

    Function(expression, args)

    e.g.:
    FadeOut(15, ReduceBy2(Version))

and can be thought of ``Function`` applied to ``expression``.
One disadvantage of OOP notation is that it can only be used with filters
which take a single video-clip argument, not with filters which take several.

All AviSynth functions produce defined number of output frames and framerate,
even if the statement seems very complex.
AviSynth knows after having read the script how long the output will be,
which framerate it has and the "cutting sequence" of all used inputs
This is all calculated on opening the script. Only the actual filtering is
done runtime on request.


--------

**Comments**: AviSynth ignores anything from a # character to the end of that
line:
::

    # comment

In *v2.58* it is possible to add block and nested block comments in the
following way:
::

    # block comment:
    /*
    comment 1
    comment 2
    */ ::# nested block comments:
    [* [* a meaningful example with follow later :) *] *]

AviSynth ignores anything from an ``__END__`` keyword (with double
underscores) to the end of the script file. This can be used to disable some
last commands of script.

::

    Version()
    __END__
    ReduceBy2()
    Result is not reduced and we can write any text here


**Ignore Case**: aViSouRCe is just as good as AVISource.

**Continue** on next or from previous line: \
::

    Subtitle ("Test-Text")

    Subtitle ( \
              "Test-Text")

    Subtitle (
           \ "Test-Text")

--------


Variables
~~~~~~~~~

A variable name can be up to 50 characters long (actually more than 4000
characters in Avisynth *v2.56*) and can contain (English) letters, digits,
and underscores (_), but no other characters. The name cannot start with a
digit.

You may use characters from your language system codepage (locale) in strings
and file names (ANSI 8 bit, utf8, but not a 16 bit Unicode).

The following types of variables can be used:

- *clip*: a video clip containing video and / or audio. At least one variable for
  a clip must be used and returned by the script.
  string: surrounded either by "quotation marks" or by 3 quotation marks like
  ``"""this example"""``. A text string can contain any character except the
  terminating quotation mark or double-apostrophe. If you need to put a
  quotation mark inside a string, use the triple quote-notation:

::

    Subtitle
    ("""This displays "hello world" with quotes""")

Strings containing escaped characters start with ``e`` prefix before the
quotation mark.

::

    Subtitle
    (e"This displays \"hello world\" with quotes\nand automatic multiline")

- *int*, *long*: entered as a string of digits, optionally with a + or - at the
  beginning. Since 3.7.4 64-bit integers are supported. In general 'int' covers
  any 32 or 64-bit integer.
- *float*, *double*: entered as a string of digits with a period (.) somewhere in it and an
  optional + or -. For example, +1. is treated as a floating-point number.
  Since 3.7.4 64-bit double is supported. In general 'float' covers
  any 32 or 64-bit floating point number.
  Since 3.7.6 scientific notation is accepted as well: ``e`` or ``E``, an optional
  + or - sign and the exponent digits, e.g. ``1.5e-5``, ``2E+3`` or ``.5e1``.
  A number with an exponent is always floating point: ``2e3`` is 2000.0.
- *val*: as type of a function argument where it does not matter if it is int or
  float
- *bool*: can be TRUE or FALSE
- *hexadecimal numbers*: entered by preceding it with a $. This variable is
  treated as an integer. Several filters use this notation for specifying
  colors. For example, $FF8800 is a shade of orange. A hexadecimal constant is 
  32-bit integer by default. Suffixed by "L" or "l" makes it 64-bit long.

::

    a = $FFFFFFFF  # integer = -1
    b = $FFFFFFFFL # long 64 bit = 4294967295


- *global*: defines a global variable, which can be used by all user-defined
  functions and the main script in common.

Here's another version of the example from above that's more manageable and
easier to understand:
::

    a = AVISource("d:\capture.00.avi")
    b = AVISource("d:\capture.01.avi")
    c = AVISource("d:\capture.02.avi")
    sound_track = WAVSource("d:\audio.wav")

    AudioDub(a+b+c, sound_track)

--------


Colors
------

In some filters (BlankClip, Letterbox, AddBorders and FadeXXX) a color
argument can be specified. In all cases the color should be specified in RGB
format even if the color format of the input clip is YUV. This can be done in
hexadecimal or decimal notation. In hexadecimal notation the number is
composed as follows: the first two digits denote the red channel, the next
two the green channel and the last two the blue channel. The hexadecimal
number must be preceded with a $. In decimal notation the number is as
follows: the red channel value is multiplied by 65536, the green channel
value is multiplied by 256 and the two resulting products are both added to
the blue channel value.

Let's consider an example. Brown is given by R=$A5 (165), G=$2A (42), B=$2A
(42). Thus ``BlankClip(color=$A52A2A)`` gives a brown frame. Converting each
channel to decimal (remember that A=10, B=11, C=12, D=14, E=14, F=15) gives
::

    R = $A5 = 10*16^1 +  5*16^0 = 165
    G = $2A =  2*16^1 + 10*16^0 =  42
    B = $2A =  2*16^1 + 10*16^0 =  42

    165*65536 + 42*256 + 42 = 10824234

Thus creating a brown frame specifying the color in decimal notation gives
``BlankClip(color=10824234)``.

Common color presets can be found in the file colors_rgb.avsi, which should
be present in your plugin autoload folder (look into the file for list of
presets). Thus ``BlankClip(color=color_brown)`` gives the same brown frames.

Note that black RGB=$000000 will be converted to Y=16, U=V=128 if the
colorformat of the input clip is YUV, since the default color conversion RGB
[0,255] -> YUV [16,235] is used.

--------


Operators
---------

| For all types of operands (clip, int, float, string, bool) you can use:
| ``==`` is equal to
| ``!=`` is not equal to

| For numeric types (int, float):
| ``+`` add
| ``-`` subtract
| ``*`` multiply
| ``/`` divide
| ``%`` mod
| ``>=`` greater than or equal to
| ``<=`` less than or equal to
| ``<`` less than
| ``>`` greater than

Note: since 3.7.4 ``int`` and ``float`` means any 32 or 64-bit integer or floating point value, respectively.
The exact type of value (e.g. if a variable or constant is 32-bit float or 64-bit double) is known.
('i'nteger - 32-bit integer, 'l'ong - 64-bit integer, 'f'loat - 32-bit foating point, 'd'ouble - 64-bit floating point) 

Integer calculations are done in 64-bit precision. When the result fits into the 32 bit range, it is stored back as 32-bit integer ``i``.
Otherwise the result is stored into 64-bit long ``l``.

When both operands are 32-bit float, arithmetic is done in 32-bits and the result is a 32-bit float.
Otherwise the calculation is done in 64-bit double ``d`` precision.

Comparisons (``==``, ``!=``, ``<``, ``>``, ``<=``, ``>=``) of two integers are done in 64-bit precision.
When either operand is a 32-bit float, the comparison is done in 32-bit float precision: the 64-bit
value is rounded to float first, so e.g. ``Floatf(2.6) == 2.6`` is true and ``Floatf(2.6) < 2.6``
is false (since 3.7.6). Values outside the 32-bit float range are not rounded (they would become infinity).
Otherwise the comparison is done in 64-bit double precision.

| For string type:
| ``+`` add
| ``>=`` greater or equal than (case-insensitive)
| ``<=`` less or equal than (case-insensitive)
| ``<`` less than (case-insensitive)
| ``>`` greater than (case-insensitive)

| For clip type:
| ``+`` the same as the function ``UnalignedSplice``
| ``++`` the same as the function ``AlignedSplice``

| For bool type (true/false):
| ``||`` or
| ``&&`` and
| ``?:`` execute code conditionally ::b = (a==true) ? 1 : 2

    This means in pseudo-basic:

    if (a=true) then b=1 else b=2

--------


Script Functions
----------------

The input and output of these functions are not clips, but some other
variables used in the script.

Numerical functions
~~~~~~~~~~~~~~~~~~~

Except ``MulDiv``, ``Rand`` and ``Spline`` the calculations are 64-bit (int or double) compatible.
``HexValue`` and ``Hex`` have special 64 bit variants: ``HexValue64`` and ``Hex64``.

See :doc:`AviSynth Syntax - Numeric functions <syntax_internal_functions_numeric>`, and
:doc:`AviSynth Syntax - Type conversion functions <syntax_internal_functions_conversion>`.


+----------------------------------------------------------+---------------------------------------------+
| ``Max(int, int[, ...])`` / ``Max(float, float[, ...])``: || ``Max`` (1, 2) = 2                         |
|   Maximum value of a set of numbers.                     || ``Max`` (5, 3.0, 2) = 5.0                  |
+----------------------------------------------------------+---------------------------------------------+
| ``Min(int, int[, ...])`` / ``Min(float, float[, ...])``: || ``Min`` (1, 2) = 1                         |
|   Minimum value of a set of numbers.                     || ``Min`` (5, 3.0, 2) = 2.0                  |
+----------------------------------------------------------+---------------------------------------------+
| ``MulDiv(int, int, int)``:                               || ``MulDiv`` (1, 1, 2) = 1                   |
|   (m*n+d/2)/d using 64 bit intermediate result           || ``MulDiv`` (2, 3, 2) = 3                   |
+----------------------------------------------------------+---------------------------------------------+
| ``Floor(float)``:                                        || ``Floor`` (1.6) = 1                        |
|   converts from float to int ``Floor`` (1.2) = 1         || ``Floor`` (-1.2) = -2                      |
|                                                          || ``Floor`` (-1.6) = -2                      |
+----------------------------------------------------------+---------------------------------------------+
| ``Ceil(float)``:                                         || ``Ceil`` (1.6) = 2.0                       |
|   converts from float to int ``Ceil`` (1.2) = 2.0        || ``Ceil`` (-1.2) = -1                       |
|                                                          || ``Ceil`` (-1.6) = -1                       |
+----------------------------------------------------------+---------------------------------------------+
| ``Round(float)``:                                        || ``Round`` (1.6) = 2                        |
|   converts from float to int ``Round`` (1.2) = 1         || ``Round`` (-1.2) = -1                      |
|                                                          || ``Round`` (-1.6) = -2                      |
+----------------------------------------------------------+---------------------------------------------+
| ``Int(float)``:                                          || ``Int`` (1.2) = 1                          |
|   Converts from float to int (round towards zero).       || ``Int`` (1.6) = 1                          |
|                                                          || ``Int`` (-1.2) = -1                        |
|                                                          || ``Int`` (-1.6) = -1                        |
+----------------------------------------------------------+---------------------------------------------+
| ``Float(int)``:                                          | ``Float`` (1) = 1.0                         |
|   Converts int to float                                  |                                             |
+----------------------------------------------------------+---------------------------------------------+
| ``Frac(float)``:                                         || ``Frac`` (3.7) = 0.7                       |
|   Returns the fractional portion of the value provided.  || ``Frac`` (-1.8) = -0.8                     |
+----------------------------------------------------------+---------------------------------------------+
| ``Abs(integer)`` / ``Abs(float)``:                       || ``Abs`` (-6) = 6                           |
|   Returns the absolute value (returns float for float,   || ``Abs`` (-1.8) = 1.8                       |
|   integer for integer).                                  |                                             |
+----------------------------------------------------------+---------------------------------------------+
| ``Sign(int)`` / ``Sign(float)``:                         || ``Sign`` (-3.5) = -1                       |
|   Returns sign of value (1, 0 or -1).                    || ``Sign`` (3.5) = 1                         |
|                                                          || ``Sign`` (0) = 0                           |
+----------------------------------------------------------+---------------------------------------------+
| ``HexValue(string)``:                                    || ``HexValue`` ("FF00") = 65280              |
|   Evaluates string as hexadecimal value.                 || ``HexValue`` ("FFFFFFFF") = -1             |
|   Result is always 32-bits.                              |                                             |
+----------------------------------------------------------+---------------------------------------------+
| ``HexValue64(string)``:                                  || ``HexValue64`` ("FF00") = 65280            |
|   Evaluates string as hexadecimal value.                 || ``HexValue64`` ("FFFFFFFF") = 4294967295   |
|   Result is always 64-bits.                              |                                             |
+----------------------------------------------------------+---------------------------------------------+
| ``Sin(float)``:                                          || ``Sin`` (Pi()/4) = 0.707                   |
|   Returns the sine of the argument (assumes it is        || ``Sin`` (Pi()/2) = 1.0                     |
|   radians).                                              |                                             |
+----------------------------------------------------------+---------------------------------------------+
| ``Cos(float)``:                                          || ``Cos`` (Pi()/4) = 0.707                   |
|   Returns the cosine of the argument (assumes it is      || ``Cos`` (Pi()/2) = 0.0                     |
|   radians).                                              |                                             |
+----------------------------------------------------------+---------------------------------------------+
| ``Pi()``:                                                | d = Pi() # d == 3.141593                    |
|   Returns the value of the "pi" constant (the ratio of   |                                             |
|   a circle's perimeter to its diameter).                 |                                             |
+----------------------------------------------------------+---------------------------------------------+
| ``Log(float)``:                                          || ``Log`` (1) = 0.0                          |
|   Returns the natural (base-e) logarithm of the argument.|| ``Log`` (10) = 2.30259                     |
+----------------------------------------------------------+---------------------------------------------+
| ``Exp(float)``:                                          || ``Exp`` (1) = 2.718282                     |
|   Returns the natural (base-e) exponent of the argument. || ``Exp`` (0) = 1.0                          |
+----------------------------------------------------------+---------------------------------------------+
| ``Pow(float base, float power)``:                        || ``Pow`` (2, 3) = 8                         |
|   Returns "base" raised to the power indicated by the    || ``Pow`` (3, 2) = 9                         |
|   second argument.                                       || ``Pow`` (3.45, 1.75) = 8.7334              |
+----------------------------------------------------------+---------------------------------------------+
| ``Sqrt(float)``:                                         || ``Sqrt`` (1) = 1.0                         |
|   Returns the square root of the argument.               || ``Sqrt`` (2) = 1.4142                      |
+----------------------------------------------------------+---------------------------------------------+
| ``Rand([int max] [, bool scale] [, bool seed])``:        | ``Rand`` (100) =                            |
|   returns random integer between 0 and max.              |    integer number between 0 and 99,         |
|                                                          |    all number equally probable.             |
|  defaults:                                               |                                             |
|                                                          | ``Rand`` (32767, False) =                   |
|  ``max`` = 32768,  scale = TRUE,  seed = FALSE           |    integer number between 0 and 32766,      |
|                                                          |    with 0 two times more probable           |
|  ``Scale`` = FALSE, modulus mode, (Rand(32768)%limit)    |    than the other numbers.                  |
|                                                          |                                             |
|  ``Seed`` = TRUE, use time as seed                       |                                             |
+----------------------------------------------------------+---------------------------------------------+
| ``Spline(float X,  x1, y1, x2, y2, ...., bool "cubic")`` | ``Spline`` (5, 0,0, 10,10, 20,0, false) = 5 |
|   **v2.5**:                                              |                                             |
|                                                          |                                             |
|   Interpolates the Y value at point X using the control  | ``Spline`` (5, 0,0, 10,10, 20,0, true) = 7  |
|   points x1/y1, ...                                      |                                             |
|                                                          |                                             |
|   There have to be at least 2 x/y-pairs.                 |                                             |
|   The interpolation can be cubic (the result is a        |                                             |
|   spline) or linear (the result is a polygon)            |                                             |
+----------------------------------------------------------+---------------------------------------------+


String functions
~~~~~~~~~~~~~~~~

+---------------------------------------------------------------+---------------------------------------------------------------------------------------------+
| ``UCase(string)``: returns the string in uppercase            | ``UCase`` ("AviSynth") = "AVISYNTH"                                                         |
+---------------------------------------------------------------+---------------------------------------------------------------------------------------------+
| ``LCase(string)``: returns the string in lowercase            | ``LCase`` ("AviSynth") = "avisynth"                                                         |
+---------------------------------------------------------------+---------------------------------------------------------------------------------------------+
| ``RevStr(string)``: returns the string in reverse             | ``RevStr`` ("AviSynth") = "htnySivA"                                                        |
+---------------------------------------------------------------+---------------------------------------------------------------------------------------------+
| ``StrLen(string)``: returns the length of string              | ``StrLen`` ("AviSynth") = 8                                                                 |
+---------------------------------------------------------------+---------------------------------------------------------------------------------------------+
| ``Findstr(string1, string2)``:                                | ``Findstr`` ("AviSynth","syn") = 4                                                          |
|   returns the offset of string2 inside string1. The search is |                                                                                             |
|   case-sensitive.                                             |                                                                                             |
+---------------------------------------------------------------+---------------------------------------------------------------------------------------------+
| ``LeftStr(string, length)`` / ``RightStr(string, length)``:   | ``LeftStr`` ("AviSynth",3) = "Avi"                                                          |
|   returns left or right portion of string specified by length |                                                                                             |
+---------------------------------------------------------------+---------------------------------------------------------------------------------------------+
| ``MidStr(string, start [, length])``:                         | ``MidStr`` ("AviSynth",3,2) = "iS"                                                          |
|   returns portion of string starting at start (for the first  |                                                                                             |
|   character start=1) for the number of characters specified   |                                                                                             |
|   by length or to the end.                                    |                                                                                             |
+---------------------------------------------------------------+---------------------------------------------------------------------------------------------+
| ``Chr(int)``: returns the ASCII character **v2.5**            | ``Chr`` (34) returns the quote character                                                    |
+---------------------------------------------------------------+---------------------------------------------------------------------------------------------+
| ``Time(string)``:                                             || Codes for output formatting:                                                               |
|   returns a string with the current system time formatted as  ||   %a Abbreviated weekday name                                                              |
|   defined by the string **v2.5**                              ||   %A Full weekday name                                                                     |
|                                                               ||   %b Abbreviated month name                                                                |
|                                                               ||   %B Full month name                                                                       |
|                                                               ||   %c Date and time representation appropriate for locale                                   |
|                                                               ||   %d Day of month as decimal number (01 - 31)                                              |
|                                                               ||   %H Hour in 24-hour format (00 - 23)                                                      |
|                                                               ||   %I Hour in 12-hour format (01 - 12)                                                      |
|                                                               ||   %j Day of year as decimal number (001 - 366)                                             |
|                                                               ||   %m Month as decimal number (01 - 12)                                                     |
|                                                               ||   %M Minute as decimal number (00 - 59)                                                    |
|                                                               ||   %p Current locale's A.M./P.M. indicator for 12-hour clock                                |
|                                                               ||   %S Second as decimal number (00 - 59)                                                    |
|                                                               ||   %U Week of year as decimal number,                                                       |
|                                                               ||      with Sunday as first day of week (00 - 53)                                            |
|                                                               ||   %w Weekday as decimal number (0 - 6; Sunday is 0)                                        |
|                                                               ||   %W Week of year as decimal number,                                                       |
|                                                               ||      with Monday as first day of week (00 - 53)                                            |
|                                                               ||   %x Date representation for current locale                                                |
|                                                               ||   %X Time representation for current locale                                                |
|                                                               ||   %y Year without century, as decimal number (00 - 99)                                     |
|                                                               ||   %Y Year with century, as decimal number                                                  |
|                                                               ||   %z, %Z Time-zone name or abbreviation; no characters if                                  |
|                                                               ||          time zone is unknown                                                              |
|                                                               ||   %% Percent sign                                                                          |
|                                                               |                                                                                             |
|                                                               || The # flag may prefix any formatting code. In that case,                                   |
|                                                               || the meaning of the format code is changed as follows.                                      |
|                                                               |                                                                                             |
|                                                               ||   %#a, %#A, %#b, %#B, %#p, %#X, %#z, %#Z, %#% # flag is ignored.                           |
|                                                               ||   %#c Long date and time representation, appropriate for current locale.                   |
|                                                               ||       For example: "Tuesday, March 14, 1995, 12:41:29".                                    |
|                                                               ||   %#x Long date representation, appropriate to current locale.                             |
|                                                               ||       For example: "Tuesday, March 14, 1995".                                              |
|                                                               ||   %#d, %#H, %#I, %#j, %#m, %#M, %#S, %#U, %#w, %#W, %#y, %#Y Remove leading zeros (if any).|
+---------------------------------------------------------------+---------------------------------------------------------------------------------------------+

Conversions
~~~~~~~~~~~

| ``Value(string)``, ``HexValue(string)``, ``HexValue64(string)``: string to number.
| ``String(value [, format_string])``: number (or bool) to string, with an optional printf-like
  format, e.g. ``String(1.23, "%5.1f")`` = ' 1.2', ``String(255, "%x")`` = 'ff'.
| ``Hex(int [, width])``, ``Hex64(int [, width])``: integer to hexadecimal string, 32 or 64 bit.
| ``Format(format_string, value1, ...)``: values inserted into a string, e.g.
  ``Format("{} x {}", Width, Height)`` = '640 x 480'.

See :doc:`AviSynth Syntax - Type conversion functions <syntax_internal_functions_conversion>` and
:doc:`AviSynth Syntax - String functions <syntax_internal_functions_string>` (``Format``).

Version checking functions
~~~~~~~~~~~~~~~~~~~~~~~~~~

+---------------------------------------------+---------------------------------------------------------------------+
| ``VersionNumber()``:                        | ``VersionNumber()`` = 2.56                                          |
|   Returns AviSynth version number as float  |                                                                     |
+---------------------------------------------+---------------------------------------------------------------------+
| ``VersionString()``:                        | ``VersionString()`` = "AviSynth 2.56, build:Oct 28 2005 [18:43:54]" |
|   Returns AviSynth version info as string   |                                                                     |
|   (first line used in ``Version`` command). |                                                                     |
+---------------------------------------------+---------------------------------------------------------------------+

Test functions
~~~~~~~~~~~~~~

| ``IsBool(var)``
| ``IsInt(var)``
| ``IsLongStrict(var)``
| ``IsFloat(var)``
| ``IsFloatfStrict(var)``
| ``IsString(var)``
| ``IsClip(var)``

Other functions
~~~~~~~~~~~~~~~

| ``Select`` (index, item0 [, item1...]): Returns item selected by the index (0=item0). Items can be any variable or clip and can even be mixed.
| ``Defined`` (var): for defining optional parameters in user-defined functions.
| ``Default`` (x, d): returns x if Defined(x), d otherwise.
| ``Exist`` (filename): returns TRUE or FALSE after checking if the file exists.
| ``NOP`` (): returns NULL, provided mainly for conditional execution with non-return value items such as import and no "else" condition desired.
| ``Eval`` (string), ``Apply`` (func-string, arg,...): ``Eval`` ("f(x)") is equivalent to ``f(x)`` is equivalent to ``Apply`` ("f", x)

::

    You can use Eval for something like:
    settings = "352, 288"
    Eval( "BicubicResize(" + settings + ")" )

``Import`` (filename): evals contents of another AviSynth script (imports the text of another script)

| For error reporting and catching bad input to user-defined function you can use:
| ``Assert`` (bool, string error-message)

``Try ... Catch``: this is a function for checking if an error WILL arise:
::

    Try {
      AviSource("file.avi")
    }
    Catch(err_msg) {
       Blackness.Subtitle(err_msg)
    }

--------


.. _runtime-functions:

Runtime Functions
-----------------

There are now :doc:`Conditional Filters <../corefilters/conditionalfilter>` which evaluate scripts when a frame is
processed, so you can change variables on a per-frame basis.
To have more applications there have to be :ref:`Runtime Functions <conditional-runtime-functions>` which
evaluate the content of the clip and return a value. **v2.5**

A simple example is to calculate the average luma for each frame and display
it.
::

    Version()      # Generate a test clip
    ConvertToYV12()# We need YV12
    FadeIn(10)     # Make the luma variable so
                   # we can see something happen

                   # Evaluate Subtitle(...) for each frame
                   # the output of AverageLuma is converted to
                   # a string and Subtitled on the input clip
    ScriptClip(" Subtitle(String(AverageLuma())) ")
    ConvertToRgb   # View it in RGB

--------


Control Functions
------------------

:doc:`AviSynth Syntax - Global options and resource control <syntax_internal_functions_global_options>`
and
:doc:`AviSynth+ Syntax - Multithreading functions <syntax_internal_functions_multithreading_new>`

for old Avisynth 2.6 see

:doc:`AviSynth Syntax - Global options and resource control <syntax_internal_functions_multithreading>`.

This section is mainly historical.

``SetPlanarLegacyAlignment`` (clip, bool): Set alignment mode for planar
frames. **v2.5.6**

Some older plugins illegally assume the layout of video frames in memory.
This special filter forces the memory layout of planar frames to be
compatible with prior version of AviSynth. The filter works on the GetFrame()
call stack, so it effects filters **before** it in the script.

*Example : Using an older version of Mpeg2Source()*
::

    LoadPlugin("...\Mpeg2Decode.dll")
    Mpeg2Source("test.d2v")         # A plugin that illegally
                                    # assumes the layout of memory
    SetPlanarLegacyAlignment(true)  # Set legacy memory alignment
                                    # for prior statements
    ConvertToYUY2()                 # Statements thru to the end
                                    # of the script have advanced
    ...                             # memory alignment.

``SetWorkingDir`` (string): Sets the default directory for Avisynth. **v2**

This is primarily for easy loading of source clips, etc. Does not affect
plugin autoloading.

    ``Return value: 0 if successful, -1 otherwise.``

| ``global OPT_AllowFloatAudio=True``: **v2.57**
| This option enables WAVE_FORMAT_IEEE_FLOAT audio output. The default is to autoconvert Float audio to 16 bit.

| ``global OPT_UseWaveExtensible=True``: **v2.58**
| This option enables WAVE_FORMAT_EXTENSIBLE audio output. The default is WAVE_FORMAT_EX.

Note: The default DirectShow component for .AVS files, "AVI/WAV File Source",
does not correctly implement WAVE_FORMAT_EXTENSIBLE processing, so many
application may not be able to detect the audio track. There are third party
DirectShow readers that do work correctly. Intermediate work files written
using the AVIFile interface for later DirectShow processing will work
correctly if they use the DirectShow "File Source (async)" component or
equivalent.

--------


Clip Properties
---------------

These functions take a clip as input and you get back a property of the clip.

Since Avisynth 2.6 many more property checkers are available (more color spaces, audio channels), 
see the relevant sections.

See :doc:`AviSynth Clip properties <syntax_clip_properties>`.

+------------------------------------------+--------------------------------------------------------+
| ``Width(clip)``                          | Returns the width of the clip in pixels (type: int).   |
+------------------------------------------+--------------------------------------------------------+
| ``Height(clip)``                         | Returns the height of the clip in pixels (type: int).  |
+------------------------------------------+--------------------------------------------------------+
| ``Framecount(clip)``                     | Returns the number of frames of the clip (type: int).  |
+------------------------------------------+--------------------------------------------------------+
| ``Framerate(clip)``                      | Returns the number of frames per seconds of the clip.  |
|                                          | (type: float)                                          |
+------------------------------------------+--------------------------------------------------------+
| ``FramerateNumerator(clip)``             |                                                        |
+------------------------------------------+--------------------------------------------------------+
| ``FramerateDenominator(clip)``           |                                                        |
+------------------------------------------+--------------------------------------------------------+
| ``Audiorate(clip)``                      | Returns the sample rate of the audio of the clip.      |
+------------------------------------------+--------------------------------------------------------+
| ``Audiolength(clip)``                    | Returns the number of samples of the audio of the clip |
|                                          | (type: int, since 3.7.4 exact 64 bit number).          |
|                                          | On pre-3.7.4 be aware of possible overflow on very     |
|                                          | long clips ( 2^31 samples limit - 32 bit integer).     |
+------------------------------------------+--------------------------------------------------------+
| ``AudiolengthF(clip)``                   | Returns the number of samples of the audio of the clip |
|                                          | (type: float, since 3.7.4 a 64 bit double, still not   |
|                                          | exact and may overflow, use Audiolength)               |
+------------------------------------------+--------------------------------------------------------+
| ``Audiochannels(clip)``                  | Returns the number of audio channels of the clip.      |
+------------------------------------------+--------------------------------------------------------+
| ``Audiobits(clip)``                      | Returns the audio bit depth of the clip.               |
+------------------------------------------+--------------------------------------------------------+
| ``IsAudioFloat(clip)``                   | Returns true if the bit depth of the audio of the clip |
|                                          | is float.                                              |
+------------------------------------------+--------------------------------------------------------+
| ``IsAudioInt(clip)``                     | Returns true if the bit depth of the audio of the clip |
|                                          | an integer.                                            |
+------------------------------------------+--------------------------------------------------------+
| ``IsRGB(clip)``                          | Returns true if the clip is RGB, false otherwise       |
|                                          | (type: bool).                                          |
+------------------------------------------+--------------------------------------------------------+
| ``IsRGB24(clip)``                        | Returns true if the clip is RGB24, false otherwise     |
|                                          | (type: bool).                                          |
+------------------------------------------+--------------------------------------------------------+
| ``IsRGB32(clip)``                        | Returns true if the clip is RGB32, false otherwise     |
|                                          | (type: bool).                                          |
+------------------------------------------+--------------------------------------------------------+
| ``IsYUY2(clip)``                         | Returns true if the clip is YUY2, false otherwise      |
|                                          | (type: bool).                                          |
+------------------------------------------+--------------------------------------------------------+
| ``IsYV12(clip)``                         | Returns true if the clip is YV12, false otherwise      |
|                                          | (type: bool).                                          |
+------------------------------------------+--------------------------------------------------------+
| ``IsYUV(clip)``                          | Returns true if the clip is YUV, false otherwise       |
|                                          | (type: bool).                                          |
+------------------------------------------+--------------------------------------------------------+
| ``IsPlanar(clip)``                       | Returns true if the clip color format is planar,       |
|                                          | false otherwise (type: bool).                          |
+------------------------------------------+--------------------------------------------------------+
| ``IsInterleaved(clip)``                  | Returns true if the clip color format is interleaved,  |
|                                          | false otherwise (type: bool).                          |
+------------------------------------------+--------------------------------------------------------+
| ``IsFieldBased(clip)``                   |                                                        |
+------------------------------------------+--------------------------------------------------------+
| ``IsFrameBased(clip)``                   |                                                        |
+------------------------------------------+--------------------------------------------------------+
| ``GetParity(clip, int "n")``             | Returns true if frame n (default 0) is top field of    |
|                                          | fieldbased clip, or it is full frame with top field    |
|                                          | first of framebased clip (type: bool).                 |
+------------------------------------------+--------------------------------------------------------+
| ``HasAudio(clip)``                       | Returns true if the clip has audio, false otherwise    |
|                                          | (type: bool).                                          |
+------------------------------------------+--------------------------------------------------------+
| ``HasVideo(clip)``                       | Returns true if the clip has video, false otherwise    |
|                                          | (type: bool).                                          |
+------------------------------------------+--------------------------------------------------------+

Don't forget: you can use the Properties with the implicit variable LAST or in OOP-notation:
::

    BilinearResize(Width/2, Height/2)
       is the same as
    BilinearResize(Width(Last)/2, Height(Last)/2)
       is the same as
    BilinearResize(Last.Width / 2, Last.Height / 2)

--------


User-Defined Functions
----------------------

You can define your own functions. This is best explained by an example:
::

    Function NTSC2PAL( clip c) {
        Assert(c.height == 480, \
               "NTSC2PAL: input clip must have 480 scan
               lines")
        Bob(c, height=576)
        return Weave()
    }

Even recursive functions can be defined.
::

    function TRANSITION(clip clip, int start, int expo, int overlap)
    {
       return ( start >= clip.framecount-expo ?
    \      Trim(clip,start,0) :
    \      Dissolve(Trim(clip,start,start+expo-1),
    \         TRANSITION(clip,start+expo,expo,overlap),
    \         overlap
    \              )
    \         )
    }

--------


.. _multiclip:

Functions with more than one input clip
---------------------------------------

There are some functions which combine two or more clips in different ways.
How the video content is calculated is described for each function, but here
is a summary which properties the result clip will have.

The input clips must always have the same color format and - with the
exception of *Layer* - the same dimensions.

+------------------------------------+---------------------+--------------------------------+---------------------+------------------------+---------------------+
|                                    | frame-rate          | frame-count                    |                     | audio content          | audio sampling rate |
+====================================+=====================+================================+=====================+========================+=====================+
| **AlignedSplice, UnalignedSplice** | first clip          | sum of all clips               |                     | see filter description | first clip          |
+------------------------------------+                     +--------------------------------+---------------------+------------------------+                     |
| **Dissolve**                       |                     | sum of all clips minus the     |                     | see filter description |                     |
|                                    |                     | overlap                        |                     |                        |                     |
+------------------------------------+                     +--------------------------------+---------------------+------------------------+                     |
| **Merge, MergeLuma, MergeChroma,** |                     | first clip                     | the last frame of   | first clip             |                     |
| **Merge(A)RGB**                    |                     |                                | the shorter clip    |                        |                     |
|                                    |                     |                                | is repeated until   |                        |                     |
+------------------------------------+                     |                                | the end of the clip |                        |                     |
| **Layer**                          |                     |                                |                     |                        |                     |
+------------------------------------+                     +--------------------------------+                     |                        |                     |
| **Subtract**                       |                     | longer clip                    |                     |                        |                     |
+------------------------------------+                     |                                |                     |                        |                     |
| **StackHorizontal, StackVertical** |                     |                                |                     |                        |                     |
+------------------------------------+---------------------+--------------------------------+                     |                        |                     |
| **Interleave**                     | (fps of first clip) |                                |                     |                        |                     |
|                                    | x                   |                                |                     |                        |                     |
|                                    | (number of clips)   | N x frame-count of longer clip |                     |                        |                     |
+------------------------------------+---------------------+--------------------------------+---------------------+------------------------+---------------------+

As you can see the functions are not completely symmetric but take some
attributes from the FIRST clip.


--------


.. _syntax-rst-plugins:

Plugins
-------

With these functions you can add external functions to AviSynth.

``LoadPlugin("filename" [, ...])``

Loads one or more external avisynth plugins (DLLs).

Avisynth+ can automatically detect and load any C or CPP plugins by a simple LoadPlugin.


--------

``LoadVirtualDubPlugin("filename", "filtername", preroll)``

This loads a plugin written for VirtualDub. "filename" is the name of the
.vdf file. After calling this function, the filter will be known as
"filtername" in avisynth. VirtualDub filters only supports RGB32. If the
video happens to be in RGB24-format, then you must use ``ConvertToRGB32``
(``ConvertToRGB`` won't suffice).

Some filters output depends on previous frames; for those preroll should be
set to at least the number of frames the filter needs to pre-process to fill
its buffers and/or updates its internal variables.


--------

``LoadVFAPIPlugin("filename", "filtername")``

This allows you to use VFAPI plugins (TMPGEnc import plugins).


--------

| ``LoadCPlugin("filename" [, ...])``
| ``Load_Stdcall_Plugin("filename" [, ...])``

Loads so called Avisynth C-plugins (DLLs).
Load_Stdcall_Plugin() is an alias for LoadCPlugin().
C-plugins are created on pure C language and use special "AviSynth C API"
(unlike ordinary Avisynt plugins which are created with MS C++). C-plugins
must be loaded with LoadCPlugin() or Load_Stdcall_Plugin().

Kevin provides a LoadCPlugin.dll that overloads the LoadCPlugin() verb to
support plugins compiled using the C subroutine calling sequence, use
Load_Stdcall_Plugin() to load stdcall calling sequence plugins when using
Kevins version. Advice: keep these plugins outside your auto plugin loading
directory to prevent crashes. [`discussion`_] [`AVISynth C API (by
kevina20723)`_]

--------


Plugin autoload and name precedence **v2**
------------------------------------------

It is possible to put all plugins and script files with user-defined
functions or (global) variables in a directory from where all files with the
extension .AVSI (**v2.08, v2.5**, the type was .AVS in **v2.05-2.07**) and
.DLL are loaded at startup, unloaded and then loaded dynamically as the
script needs them.

.AVSI scripts in this directory should only contain function definitions and
global variables, no main processing section (else strange errors may occur),
it also is not recommended to put other files in that directory.

The directory is stored in the registry (the registry key has changed for
**v2.5**). You can use double-clicking a .REG-file with the following lines
to set the path (of course inserting your actual path):
::

    REGEDIT4

    [HKEY_LOCAL_MACHINE\SOFTWARE\Avisynth]
    "plugindir2_5"="c:\\program files\\avisynth 2.5\\plugins"

The order in which function names take precedence is as follows: ::user-
defined function (always have the highest priority)
::

       plugin-function (have higher priority than built-in
       functions, they will override a built-in function)
          built-in function

Inside those groups the function loaded at last takes precedence, there is no
error in a namespace conflict.


Plugin autoload and conflicting function names **v2.55**
--------------------------------------------------------

Starting from v2.55 there is DLLName_function() naming convention support. The problem is that
two plugins can have different functions which are named the same. To call
the needed one, DLLName_function() support is added. It auto-generates the
additional names both for auto-loaded plugins and for plugins loaded with
LoadPlugin.

**Some examples:**

::

    # using fielddeinterlace from decomb510.dll
    AviSource("D:\captures\jewel.avi")
    decomb510_fielddeinterlace(blend=false)

Suppose you have  the plugins mpeg2dec.dll and mpeg2dec3.dll in your auto
plugin dir, and you want to load a d2v file with mpeg2dec.dll (which outputs
YUY2):

::

    # using mpeg2source from mpeg2dec.dll
    mpeg2dec_mpeg2source("F:\From_hell\from_hell.d2v")

or with mpeg2dec3.dll (which outputs YV12):

::

    # using mpeg2source from mpeg2dec3.dll
    mpeg2dec3_mpeg2source("F:\From_hell\from_hell.d2v")



Changelog
~~~~~~~~~
+----------------+------------------------------------------------------------+
| Version        | Changes                                                    |
+================+============================================================+
| Avisynth 3.7.6 | | Floating point constants in scientific notation          |
|                | | Add Hex64 function                                       |
+----------------+------------------------------------------------------------+
| Avisynth 3.7.4 | | Added 64-bit types, integer long, floating point double  |
|                | | Add "L" suffixed hexadecimal notation                    |
|                | | Add HexValue64 function                                  |
|                | | Add IsFloatfStrict() and IsLongStrict()                  |
|                | | Add Double(), FloatF(), Long(), IntI() type casts        |
|                | | AudioLength returns 64 bit integer (exact size)          |
|                | | AudioLengthF returns 64 bit double                       |
+----------------+------------------------------------------------------------+

$Date: 2025/02/05 10:22:00 $

.. _discussion: http://forum.doom9.org/showthread.php?s=&threadid=58840
.. _AVISynth C API (by kevina20723):
    http://kevin.atkinson.dhs.org/avisynth_c/
