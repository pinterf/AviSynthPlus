
propShow
========

See also: :doc:`Internal functions: frame properties <../syntax/syntax_internal_functions_frame_properties>`.

Lists frame properties to screen (a debug filter).

Gives frame property information as a text overlay, if the position is not specified, in the upper-left corner.

The displayed information consists of:

* Number of frame properties (only if no name filter is given)
* each line describes a frame property with its name and value
* display _Matrix, _ColorRange and _ChromaLocation constants with friendly names, e.g. _ColorRange = 1 = limited

Listing appears as a name = value list. Arrays values are put between [ and ]
Floating point values are shown in their shortest exact form, the same way as in ``Format``
(e.g. ``0.1``, ``0.3333333333333333``, ``1e-07``), with ``.`` as the decimal separator.
Top line contains number is properties. If no properties found, nothing is displayed.


Syntax and Parameters
----------------------

::

    propShow (clip clip, int "size", string "font", bool "showtype", 
          string "font", int "text_color", int "halo_color", 
          bool "bold", float "x", float "y", int "align", string[] "props")

.. describe:: clip

    Source clip which properties will be written to itself

.. describe:: showtype

    If true, the data type in parenthesis appears next to the property key name
    
    Default: false

.. describe:: size

    | Height of the text in pixels (fixed fonts)
    
    Default: 16

.. describe:: font

    Font name; "Terminus" or "info_h" or can be the name of a bdf bitmap font.

    Default: "Terminus"

.. describe:: text_color, halo_color

    | Colors for font fill and outline respectively.  See the
      :doc:`colors <../syntax/syntax_colors>` page for more information on 
      specifying colors.
    | Default text color is yellow and halo color is black.
    | halo color MSB have special meaning
    |   FF (e.g. FF000000) -> no outline + semi transparent background
    |   FE (e.g. FE000000) -> outline + semi transparent background
    |   01 (e.g. 01000000) -> no outline + normal display
    |   00 (e.g. 00000000) -> outline + normal display
    
    Default: $FFFF00, $000000 (that is $00FFFF00, $00000000)

.. describe:: bold

    | Using bold letters or not

    Default: false

.. describe:: x, y

    | Reference point of the Info text box area, alignments are
      calculated relative to that.

    Default: 4, 0 (top left), screen centers or right/bottom when alignment is specified

.. describe:: align

    | an integer number describing at what screen area (or given x,y coordinates)
      will be the info text box aligned.
      Values 1-9 are allowed. See your numeric keypad layout, e.g. 7 is top-left,
      9 is top right, 3: bottom-right, etc..

    Default: 7 (top-left)
    
    Note: The individual lines are aligned horizontally as well.

.. describe:: props

    | single string or array of strings
      - name: exact match
      - regex: starting "^" end with "$"
      - wildcard: contains "*"
      
      If given, no "Number of keys" header is shown.


Examples
--------

::

    ColorBarsHD()
    propShow(align=1, halo_color=$FF000000)
    propShow(size=6,bold=true, align=3, halo_color=$FE000000)  
    propShow(size=16,bold=true, align=5, halo_color=$00000000)
    propShow(font="info_h", align=9, halo_color=$01000000)
    
    ColorBarsHD()
    PropSet("MyProp1", 121)
    PropSet("MyProp2", "Hello Avisynth")
    propShow() # display all properties, with header
    propShow(props="*", align=2) # display all properties, no header, bottom center
    propShow(props="_*", align=8) # display properties starting with "_", top center
    propShow(props="^[^_].*$", align=9) # display properties NOT starting with "_", , top right corner

The following script aligns Info box to the top right, and propShow to the bottom left.
    
::

    ColorBars()
    propShow(align=1)
    Info(cpu=false, align=9)


Changelog
---------

+-----------------+-----------------------------------------------------------------------+
| Version         | Changes                                                               |
+=================+=======================================================================+
| AviSynth+ 3.7.6 | Floating point values in shortest exact form (was 6 fixed decimals)   |
+-----------------+-----------------------------------------------------------------------+
| AviSynth+ 3.7.4 || Add ``font``, ``text_color``, ``halo_color``, ``bold``  parameter    |
|                 || Add ``x, y, align`` parameters                                       |
|                 || add "props" parameter with wildcard and regex support                |
+-----------------+-----------------------------------------------------------------------+
| AviSynth+ 3.7.1 | display _Matrix, _ColorRange and _ChromaLocation constants with       |
|                 | friendly names                                                        |
+-----------------+-----------------------------------------------------------------------+
| AviSynth+ 3.6.0 | Initial release                                                       |
+-----------------+-----------------------------------------------------------------------+

Back to :doc:`Internal functions <../syntax/syntax_internal_functions>`.

$Date: 2025/03/04 12:20:00 $
