=======================
ColorBars / ColorBarsHD
=======================
The `ColorBars`_. `ColorBarsHD`_ and `ColorBarsUHD`_ filters generate a video clip containing
`SMPTE color bars`_ scaled to any image size. 

You can find `ColorBarsUHD`_ in a separate document, here we describe the first two version.
Both filters also generate audio, see the `audio`_ section for details.

.. _ColorBars:

ColorBars
---------

.. figure:: pictures/colorbars-320x240.png
   :align: left

**ColorBars** produces a video clip containing SMPTE color bars
(`Rec. ITU-R BT.801-1`_) scaled to any image size. By default, a 640x480, RGB32,
`TV range`_, 29.97 fps, 1 hour long clip is produced.

The color values are computed from ground-truth linear RGB primaries using the
**BT.601** matrix (``AVS_MATRIX_ST170_M``). This applies to all YUV output formats.
RGB output formats use the same primaries directly in studio-swing encoding.

|clearfloat|

.. rubric:: Syntax and Parameters

::

    ColorBars (int "width", int "height", string "pixel_type", bool "staticframes")

.. describe:: width, height

    Set size of the returned clip.

    Default: 640, 480

.. describe:: pixel_type

    Set color format of the returned clip. May be any of the following: "RGB24",
    "RGB32", "RGB48", "RGB64", "YUY2", "YV12", "YV16" "YV24", "YV411", or any
    planar RGBPx, RGBAPx, YUV4xxPx, YUVA4xxPx format.

    Default: "RGB32"

.. describe:: staticframes

    If set to false, generate all frames. Default true (one static frame is served).

    Default: true

.. _ColorBarsHD:

ColorBarsHD
-----------

.. figure:: pictures/colorbarshd-320x180.png
   :align: left

**ColorBarsHD** produces a video clip containing SMPTE color bars
(Rec. ITU-R BT.709 / `ARIB STD-B28 v1.0`_) scaled to any image size. By default,
a 1288x720, YV24, `TV range`_, 29.97 fps, 1 hour long clip is produced.

The color values are computed from ground-truth linear RGB primaries using the
**BT.709** matrix (``AVS_MATRIX_BT709``). Output is always a YUV 4:4:4 format.

|clearfloat|

.. rubric:: Syntax and Parameters

::

    ColorBarsHD (int "width", int "height", string "pixel_type", bool "staticframes")

.. describe:: width, height

    Set size of the returned clip.

    Default: 1288, 720

.. describe:: pixel_type

    Set color format of the returned clip. Must be "YV24" or any YUV444Px /
    YUVA444Px format.

    Default: "YV24" (identical to "YUV444P8")

.. describe:: staticframes

    If set to false, generate all frames. Default true (one static frame is served).

    Default: true

Audio
-----

For both filters, an audio :doc:`tone <tone>` is also generated. The tone is a
440Hz sine at 48KHz sample rate, 32 bit (Float), stereo. The tone pulses in the
right speaker, being turned on and off once every second. Level is `0 dBFS`_.

You can use :doc:`Amplify <amplify>` to set a softer level (0dB can be a little
deafening!) ::

    ColorBarsHD
    AmplifyDB(-20)

Broadcasting organizations usually specify an "alignment tone" accompanying
colorbars at anywhere from -12 to -20 dBFS; if sending materials to another
party, be sure to get their preferred alignment tone level. The exact level
doesn't matter as long as all parties agree to it.

A note on notation
------------------

This page adopts the *ITU style* when discussing video levels which might be
represented at different bit depths:

    **"** To avoid confusion between 8-bit and 10-bit representations, the eight
    most-significant bits are considered to be an integer part while the two
    additional bits, if present, are considered to be fractional part.
    For example, the bit pattern ``10010001`` would be expressed as 145\ |d|,
    whereas the pattern ``1001000101`` would be expressed as 145.25\ |d|. **"**

    `ITU-R BT.601-7 (page 4)`_

Video levels shown below with the subscript "d" are assumed to be scaled by 2^(bit depth-8).
For example, 235\ |d| at bit depth 10 becomes 235 × 2^(10-8) = 235 × 4 = 940.

* see `Deep Color`_
* see `AviSynthPlus color formats`_

TV range
--------

For both filters, in all color formats, luminance levels are :doc:`TV (limited)
range <../advancedtopics/luminance_levels>`, where black=16\ |d| and white=235\ |d|,
within a total possible range of 0-255\ |d|.

The table below shows the TV-range values **ColorBarsHD** generates, and those
same values as they should be after converting to full range.

:math:`\mathtt{Y_\text{full} = (Y_\text{tv}-16_\text{d})  × 255_\text{d}/(235_\text{d}-16_\text{d})}` // (for R, G, B, Y)

:math:`\mathtt{U_\text{full} = (U_\text{tv}-128_\text{d}) × 255_\text{d}/(240_\text{d}-16_\text{d}) + 128_\text{d}}` // (for U, V)

:math:`\mathtt{Y_\text{tv}   = Y_\text{full} × (235_\text{d}-16_\text{d})/255_\text{d} + 16_\text{d}} \quad` // (for R, G, B, Y)

:math:`\mathtt{U_\text{tv}   = (U_\text{full}-128_\text{d}) × (240_\text{d}-16_\text{d})/255_\text{d} + 128_\text{d}}` // (for U, V)

The table below shows the 8-bit TV-range values **ColorBarsHD** generates, and
those same values as they should be after converting to full range. At higher
bit depths, values are computed directly from linear RGB primaries via the
BT.709 matrix and are not simple left-shifts of these 8-bit codes.

.. table::
    :widths: auto

    +----------------+----------+----------+----------+-----+----------+----------+----------+-----+----------+----------+----------+-----+----------+----------+----------+
    | Color bar      | TV range output (8-bit)                                               |     | (expanded to full range, 8-bit)                                       |
    +================+==========+==========+==========+=====+==========+==========+==========+=====+==========+==========+==========+=====+==========+==========+==========+
    |                | **R**    | **G**    | **B**    |     | **Y**    | **U**    | **V**    |     | **R**    | **G**    | **B**    |     | **Y**    | **U**    | **V**    |
    +----------------+----------+----------+----------+-----+----------+----------+----------+-----+----------+----------+----------+-----+----------+----------+----------+
    | **75% White**  | 180      | 180      | 180      |     | 180      | 128      | 128      |     | 191      | 191      | 191      |     | 191      | 128      | 128      |
    +----------------+----------+----------+----------+-----+----------+----------+----------+-----+----------+----------+----------+-----+----------+----------+----------+
    | **75% Yellow** | 180      | 180      | 16       |     | 168      | 44       | 136      |     | 191      | 191      | 0        |     | 177      | 32       | 137      |
    +----------------+----------+----------+----------+-----+----------+----------+----------+-----+----------+----------+----------+-----+----------+----------+----------+
    | **75% Cyan**   | 16       | 180      | 180      |     | 145      | 147      | 44       |     | 0        | 191      | 191      |     | 150      | 149      | 32       |
    +----------------+----------+----------+----------+-----+----------+----------+----------+-----+----------+----------+----------+-----+----------+----------+----------+
    | **75% Green**  | 16       | 180      | 16       |     | 133      | 63       | 52       |     | 0        | 191      | 0        |     | 136      | 54       | 41       |
    +----------------+----------+----------+----------+-----+----------+----------+----------+-----+----------+----------+----------+-----+----------+----------+----------+
    | **75% Magenta**| 180      | 16       | 180      |     | 63       | 193      | 204      |     | 191      | 0        | 191      |     | 55       | 201      | 214      |
    +----------------+----------+----------+----------+-----+----------+----------+----------+-----+----------+----------+----------+-----+----------+----------+----------+
    | **75% Red**    | 180      | 16       | 16       |     | 51       | 109      | 212      |     | 191      | 0        | 0        |     | 41       | 106      | 223      |
    +----------------+----------+----------+----------+-----+----------+----------+----------+-----+----------+----------+----------+-----+----------+----------+----------+
    | **75% Blue**   | 16       | 16       | 180      |     | 28       | 212      | 120      |     | 0        | 0        | 191      |     | 14       | 223      | 118      |
    +----------------+----------+----------+----------+-----+----------+----------+----------+-----+----------+----------+----------+-----+----------+----------+----------+

These colors are at "75%" of maximum, per common broadcast practice. You may
occasionally see "100%" color bars.\ `[1]`_ They are rather useless, as you cannot
detect gain or saturation that is too high on a signal that is already at maximum.

PLUGE
-----

The lower part of the frame is called the `PLUGE`_ (also lowercase: "pluge")
signal. From left to right it consists of: `-I`_, white, `+Q`_, then a series of
black and near-black bars: 0, -4, 0, +4 and 0 `IRE`_ relative to black.

    **Note** 'IRE' is used here to mean 'percent luminance', on a scale from
    0 (black) to 100 (white), ignoring the varying broadcast standards where
    black might be 0 IRE or 7.5 IRE depending on the country.

    This section documents the **ColorBars** pluge only; **ColorBarsHD**'s pluge
    is similar, but dispenses with -I and +Q.

The table below shows the 8-bit TV-range values **ColorBars** generates, and
those same values as they should be after converting to full range. At higher
bit depths, values are computed directly from linear RGB primaries via the
BT.601 (SMPTE 170M) matrix and are not simple left-shifts of these 8-bit codes.

.. table::
    :widths: auto

    +----------------+----------+----------+----------+-----+----------+----------+----------+-----+----------+----------+----------+-----+----------+----------+----------+
    | PLUGE Element  | TV range output (8-bit)                                               |     | (expanded to full range, 8-bit)                                       |
    +================+==========+==========+==========+=====+==========+==========+==========+=====+==========+==========+==========+=====+==========+==========+==========+
    |                | **R**    | **G**    | **B**    |     | **Y**    | **U**    | **V**    |     | **R**    | **G**    | **B**    |     | **Y**    | **U**    | **V**    |
    +----------------+----------+----------+----------+-----+----------+----------+----------+-----+----------+----------+----------+-----+----------+----------+----------+
    | **-I**         | 16       | 90       | 130      |     | 16       | 158      | 95       |     | 0        | 86       | 130      |     | 0        | 162      | 90       |
    +----------------+----------+----------+----------+-----+----------+----------+----------+-----+----------+----------+----------+-----+----------+----------+----------+
    | **100% White** | 235      | 235      | 235      |     | 235      | 128      | 128      |     | 255      | 255      | 255      |     | 255      | 128      | 128      |
    +----------------+----------+----------+----------+-----+----------+----------+----------+-----+----------+----------+----------+-----+----------+----------+----------+
    | **+Q**         | 92       | 16       | 143      |     | 16       | 174      | 149      |     | 88       | 0        | 148      |     | 0        | 180      | 151      |
    +----------------+----------+----------+----------+-----+----------+----------+----------+-----+----------+----------+----------+-----+----------+----------+----------+
    | Black          | 16       | 16       | 16       |     | 16       | 128      | 128      |     | 0        | 0        | 0        |     | 0        | 128      | 128      |
    +----------------+----------+----------+----------+-----+----------+----------+----------+-----+----------+----------+----------+-----+----------+----------+----------+
    | **-4 IRE**     | 7        | 7        | 7        |     | 7        | 128      | 128      |     | -10      | -10      | -10      |     | -10      | 128      | 128      |
    +----------------+----------+----------+----------+-----+----------+----------+----------+-----+----------+----------+----------+-----+----------+----------+----------+
    | Black          | 16       | 16       | 16       |     | 16       | 128      | 128      |     | 0        | 0        | 0        |     | 0        | 128      | 128      |
    +----------------+----------+----------+----------+-----+----------+----------+----------+-----+----------+----------+----------+-----+----------+----------+----------+
    | **+4 IRE**     | 25       | 25       | 25       |     | 25       | 128      | 128      |     | 10       | 10       | 10       |     | 10       | 128      | 128      |
    +----------------+----------+----------+----------+-----+----------+----------+----------+-----+----------+----------+----------+-----+----------+----------+----------+
    | Black          | 16       | 16       | 16       |     | 16       | 128      | 128      |     | 0        | 0        | 0        |     | 0        | 128      | 128      |
    +----------------+----------+----------+----------+-----+----------+----------+----------+-----+----------+----------+----------+-----+----------+----------+----------+
    |                                                                                        |     | *(negative values will be clipped to 0)*                              |
    +----------------+----------+----------+----------+-----+----------+----------+----------+-----+----------+----------+----------+-----+----------+----------+----------+

**Important note on -I and +Q values:** The RGB and YUV values shown above do **not**
convert to each other via standard BT.601 matrix conversion. This is intentional and
reflects the dual-specification nature of these legacy NTSC test signals:

- **RGB output** uses luma-corrected values where the most negative component is lifted
  to studio black (code 16), ensuring all RGB codes are broadcast-safe (≥16).

- **YUV output** uses zero-luma pure chroma-axis values (Y=16) that preserve the
  theoretical definition but would require super-black RGB components if converted back.

The -I and +Q bars are vestigial artifacts of NTSC analog TV and are not really used any more.

    The -4, 0 and +4 IRE bars can be used to set your monitor brightness – assuming
    your playback chain expands TV range (16-235) to full-range (0-255)
    as shown in the images above. The -4 IRE and 0 IRE bars should have the same
    apparent brightness (they should be as dark as the monitor can display), and the
    +4 should be a little brighter. If you can see the -4 bar, your monitor
    brightness is set too high; if you cannot see the +4 bar, your monitor
    brightness is set too low.\ `[2]`_

Note that the PLUGE signal intentionally includes super-black values in the -4 IRE bar
(code 7, below studio black at code 16). These test the full capability of the signal
chain and cannot be accurately represented when converting to full-range 0-255.

More information about the colorbars and the PLUGE can be found on the
`color bars theory`_ page.

Miscellaneous
-------------

Note, that for example ::

    ColorBars(pixel_type="YUV444P8")

...is equivalent to ::

    ColorBars(pixel_type="RGB32")
    ConvertToYUV444(matrix="PC.601")
    # "PC.601" / "PC.709" / "PC.2020" don't scale the luma range

When directly generating YUV format data, the color transitions are arranged to
occur on a chroma-aligned boundary.

Advanced: color value derivation
---------------------------------

This section documents the precise mathematical derivation of the color values
used by **ColorBars** and **ColorBarsHD**, including the handling of the -I and
+Q PLUGE components.

**ColorBarsHD** derives all color values from ground-truth linear RGB primaries
using double-precision arithmetic and the **BT.709** RGB-to-YUV matrix:

.. code-block:: none

    Y  =  0.2126·R + 0.7152·G + 0.0722·B
    Cb = (B − Y) / 1.8556
    Cr = (R − Y) / 1.5748

**ColorBars** uses the same approach with the **BT.601** (SMPTE 170M) matrix:

.. code-block:: none

    Y  =  0.299·R + 0.587·G + 0.114·B
    Cb = (B − Y) / 1.772
    Cr = (R − Y) / 1.402

For integer output at any bit depth *n*, the encoding is:

.. code-block:: none

    Y_encoded  = (Y  × 219 + 16 ) × 2^(n−8)   (luma,   limited range)
    Cb_encoded = (Cb × 224 + 128) × 2^(n−8)   (chroma, limited range)
    Cr_encoded = (Cr × 224 + 128) × 2^(n−8)   (chroma, limited range)

For 32-bit float output, the AviSynth limited-range float convention is used,
derived from the same centralized conversion constants as all other AviSynth
filters:

.. code-block:: none

    Y_float  = Y  × (219/255) + (16/255)
    Cb_float = Cb × (224/255)
    Cr_float = Cr × (224/255)

All RGB output formats use studio-swing encoding with the same ground-truth
primaries. The helper function ``studio_rgb_to_integer`` encodes a normalized
double value *v* as:

.. code-block:: none

    code = (int)(v × 219 × 2^(n−8) + 16 × 2^(n−8) + 0.5)

where *v* = 0.0 maps to studio black (code 16) and *v* = 1.0 maps to
studio white (code 235). Convention: normalized values represent limited-range
positions, where 0.0 = studio black (not absolute black).

**-I and +Q derivation**

The -I and +Q signals are defined in the YIQ colour space as pure chroma-axis
signals with zero luma and 20 IRE saturation (0.2162 normalized):

.. code-block:: none

    -I:  I = −0.2162,  Q = 0
    +Q:  I = 0,        Q = +0.2162

Converting via the BT.601 UV rotation (Poynton eq. 33, with UV swap):

.. code-block:: none

    -I raw RGB (Y=0):  R = −0.2067,  G = +0.0588,  B = +0.2394
    +Q raw RGB (Y=0):  R = +0.1343,  G = −0.1400,  B = +0.3685

Both signals contain out-of-range (negative) RGB components. Three
interpretations exist in the literature:

* **Option 1 — Zero-luma, studio black hack** (legacy AviSynth):
  Y = 16 (studio black). This is a HACK applied differently for RGB vs YUV:
  
  - YUV output: Uses raw zero-luma values (Y=16, Cb=158, Cr=95 for -I)
  - RGB output: Individually adjusted/clipped components to avoid super-blacks
  
  Result: Two incompatible specifications that don't convert via standard matrices.

* **Option 2 — Luma-corrected to absolute black** (valid alternative, not used):
  Luma raised until most negative component reaches code 0 (absolute black).
  Gives colorimetric consistency but uses super-black range (codes 0-15).

* **Option 3 — Luma-corrected to studio black** *(current RGB implementation)*:
  Luma raised until most negative component reaches code 16 (studio black).
  
  .. code-block:: none
  
      For -I: Y_lift = 0.2067 - 16/219 = 0.13364
      For +Q: Y_lift = 0.1400 - 16/219 = 0.06694
  
  After lifting (RGB output):
  
  .. code-block:: none
  
      -I: R = 16 (studio black), G = 90, B = 130, Y ≈ 77
      +Q: R = 92, G = 16 (studio black), B = 143, Y ≈ 63

**Dual specification implementation**

The current implementation maintains **two separate ground-truth tables**:

1. **RGB-native values** (Option 3): Used for RGB output formats
   
   - All components >= 16 (broadcast-safe)
   - Colorimetrically consistent RGB↔YUV conversion
   - -I: RGB(16, 90, 130) → Y≈77 after BT.601 conversion
   - +Q: RGB(92, 16, 143) → Y≈63 after BT.601 conversion

2. **YUV-targeted values** (Option 1): Used for YUV output formats
   
   - Produces exact legacy values: -I Y=16, +Q Y=16 (zero-luma)
   - Contains out-of-range RGB components if converted back
   - Preserves theoretical chroma-axis purity

This dual-table approach acknowledges historical reality: the original AviSynth
ColorBars had two independent specifications that don't convert to each other.
The -I and +Q signals were analog broadcast test signals (voltage levels), not
digital RGB/YUV values, and their digital representation requires compromises.

Higher bit-depth output is computed directly from the double-precision ground-truth
values via the full encoding formula — not by bit-shifting 8-bit codes — giving
greater accuracy at 10, 12, 14, 16-bit and float.

Changelog
---------

+------------------+---------------------------------------------------------+
| Version          | Changes                                                 |
+==================+=========================================================+
| AviSynth+ 3.7.6  || ColorBarsHD: fixed high bit-depth output (values now   |
|                  |  computed from linear RGB primaries via BT.709 matrix   |
|                  |  instead of upscaling 8-bit table entries).             |
|                  || ColorBarsHD: fixed ramp in pattern 3                   |
|                  || ColorBars: fixed high bit-depth output for all YUV     |
|                  |  and RGB formats (same ground-truth RGB approach).      |
|                  || ColorBars: fixed reported frame property ``_matrix``   |
|                  |  to ``AVS_MATRIX_ST170_M`` (BT.601); was incorrectly    |
|                  |  reporting ``AVS_MATRIX_BT709``.                        |
|                  || ColorBars: -I and +Q values kept from legacy, but      |
|                  |  the explanation is included in this documentation.     |
|                  || Fix: "staticframes"=false parameter copied U instead   |
|                  |  of A for alpha plane.                                  |
+------------------+---------------------------------------------------------+
| AviSynth+ 3.4.0  || ColorBars: add support for all YUV(A)422 formats and   |
|                  |  RGB24, RGB48, YV411.                                   |
+------------------+---------------------------------------------------------+
| AviSynth+ r2487  || ColorBars: add support for all YUV(A)444/420, planar   |
|                  |  RGB(A) formats and RGB64.                              |
|                  || ColorBarsHD: add support for all YUV(A)444 formats.    |
+------------------+---------------------------------------------------------+
| AviSynth 2.6.0   || Added pixel_type="YV24" to ColorBars.                  |
|                  || Initial release of ColorBarsHD.                        |
+------------------+---------------------------------------------------------+
| AviSynth 2.5.6   || Added ``pixel_type`` parameter.                        |
|                  || Added "YUY2" and "YV12" pixel types.                   |
+------------------+---------------------------------------------------------+
| AviSynth 2.5.5   | Width and height parameters are now named and optional. |
+------------------+---------------------------------------------------------+

$Date: 2026/02/19 09:57:00 $

.. _SMPTE color bars:
    https://en.wikipedia.org/wiki/SMPTE_color_bars
.. _Rec. ITU-R BT.801-1:
    https://www.itu.int/rec/R-REC-BT.801/en
.. _ARIB STD-B28 v1.0:
    https://www.arib.or.jp/english/html/overview/doc/6-STD-B28v1_0-E1.pdf
.. _0 dBFS:
    https://en.wikipedia.org/wiki/DBFS
.. _ITU-R BT.601-7 (page 4):
    https://www.itu.int/rec/R-REC-BT.601-7-201103-I/en
.. _Deep Color:
    http://avisynth.nl/index.php/High_bit-depth_Support_with_Avisynth#What_is_Deep_Color.3F
.. _AviSynthPlus color formats:
    http://avisynth.nl/index.php/Avisynthplus_color_formats
.. _[1]:
    http://trac.ffmpeg.org/wiki/FilteringGuide#multipleinputoverlayin2x2grid
.. _PLUGE:
    https://en.wikipedia.org/wiki/Picture_line-up_generation_equipment
.. _-I:
    https://en.wikipedia.org/wiki/YIQ
.. _+Q:
    https://en.wikipedia.org/wiki/YIQ
.. _IRE:
    https://en.wikipedia.org/wiki/IRE_(unit)
.. _[2]:
    http://spearsandmunsil.com/portfolio-item/setting-the-brightness-control-2/
.. _color bars theory:
    http://avisynth.nl/index.php/ColorBars_theory
.. |d| replace:: :sub:`d`
.. |clearfloat|  raw:: html

    <div class="clearer"></div>