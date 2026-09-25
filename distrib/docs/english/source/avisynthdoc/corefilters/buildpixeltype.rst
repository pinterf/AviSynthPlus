
==============
BuildPixelType
==============

**BuildPixelType** builds and returns a ``pixel_type`` format **name** (a string, such as
``"YV12"`` or ``"YUV444P10"``), not a clip. Give it a colorspace family, a bit depth, and
(for `YUV`_ families) a chroma subsampling ratio, and it assembles the correctly-spelled
format name for you - so you don't have to hand-build strings like ``"YUV420P10"`` or
remember whether a given combination even has a valid name.

Any argument you don't specify can instead be taken from an existing clip's own format via
``sample_clip``, letting you build "the same format, but changed in just one respect" -
for example, the same clip's colorspace and chroma layout at a different bit depth.

The resulting string is meant to be fed into another filter's own ``pixel_type`` argument,
typically :doc:`BlankClip <blankclip>`, or wherever else a format name string is expected.


Syntax and Parameters
----------------------

::

    BuildPixelType(string "family", int "bits", int "chroma", bool "compat",
                   bool "oldnames", clip "sample_clip")

.. describe:: family

    The colorspace family: ``"Y"``, ``"YA"``, ``"YUV"``, ``"YUVA"``, ``"RGB"`` or ``"RGBA"``
    (case insensitive). ``"RGB"``/``"RGBA"`` mean *planar* RGB(A) unless ``compat`` is used.

    If omitted, ``sample_clip`` must be given, and the family is read off the template clip's
    own colorspace (``"Y"`` for greyscale, ``"YA"`` for Y+Alpha, ``"YUV"``/``"YUVA"`` for
    planar YUV(A), ``"RGB"``/``"RGBA"`` for both planar and packed RGB(A) - a packed
    ``RGB32``/``RGB64`` template counts as ``"RGBA"``). Not every source format can be
    classified this way (e.g. ``YUY2``); such a ``sample_clip`` raises an error.

    Default: taken from ``sample_clip``; an error is raised if neither is given.

.. describe:: bits

    Bit depth per component: ``8``, ``10``, ``12``, ``14``, ``16`` or ``32``.

    Default: taken from ``sample_clip``'s own bit depth; an error is raised if neither is
    given.

.. describe:: chroma

    Chroma subsampling, for the ``"YUV"``/``"YUVA"`` families only: ``444``, ``422``, ``420``,
    ``411``, ``440`` or ``410``. Ignored for every other family (``"Y"``, ``"YA"``, ``"RGB"``,
    ``"RGBA"``) - there's no chroma to subsample.

    Default: ``444`` - unless ``sample_clip`` is given, in which case (independently of
    whether ``family``/``bits`` were themselves given explicitly) it is read off the
    template's own chroma subsampling ratio instead.

.. describe:: compat

    RGB(A) only. When true, returns one of the legacy *packed* RGB format names
    (``"RGB24"``, ``"RGB32"``, ``"RGB48"``, ``"RGB64"``) instead of a planar RGB(A) one -
    only ``bits=8`` (giving RGB24/RGB32) or ``bits=16`` (giving RGB48/RGB64) are accepted
    with ``compat=true``. Ignored (treated as ``false``) for every other family.

    Default: false.

.. describe:: oldnames

    When true, returns the older, short-form name instead of the modern generic one, for the
    handful of formats that have one: ``"YV12"`` (instead of ``"YUV420P8"``), ``"YV16"``
    (``"YUV422P8"``), ``"YV24"`` (``"YUV444P8"``), ``"YV411"`` (``"YUV411P8"``) and ``"YUV9"``
    (``"YUV410P8"``). There is no legacy short name for 4:4:0 (``YUV440``/``YUV440P8``) - it
    never existed under the old naming scheme - so ``oldnames`` has no effect on it.

    Default: false.

.. describe:: sample_clip

    A template clip. Any of ``family``, ``bits`` or ``chroma`` you leave unspecified is
    inherited from this clip's own format instead; anything you *do* specify overrides the
    template. This is the way to change just one aspect of an existing format, e.g.
    "the same clip, but 16-bit" (see Example #2 below).

    Either ``sample_clip``, or both ``family`` and ``bits``, must be given.


How it works
------------

**BuildPixelType** is a pure string-builder - it never touches pixel data and does not
require ``sample_clip`` to have any particular content, only a queryable format. The name is
assembled as:

    ``family`` + (``chroma`` + ``"P"``, for YUV/YUVA only) + a bit-depth suffix

* For ``"YUV"``/``"YUVA"``, the chroma ratio is appended first (``"444"``, ``"422"``,
  ``"420"``, ``"411"``, ``"440"`` or ``"410"``), followed by a literal ``"P"`` - giving
  intermediate results like ``"YUV420P"`` or ``"YUVA411P"`` - before the bit-depth suffix is
  added. ``"Y"``, ``"YA"``, ``"RGB"`` (→ ``"RGBP"``) and ``"RGBA"`` (→ ``"RGBAP"``) skip this
  step entirely, since they carry no chroma ratio.
* The bit-depth suffix is the plain number (``"8"``, ``"10"``, ``"12"``, ``"14"``, ``"16"``)
  for every family - **except** at ``bits=32``, where every family except plain ``"Y"`` uses
  ``"S"`` instead (giving e.g. ``"YUV444PS"``, ``"RGBPS"``, ``"YAS"``), matching the general
  32-bit-float naming convention; ``"Y"`` alone keeps a numeric suffix, giving ``"Y32"``
  (a ``"YS"`` alias also exists for that one format, but is not what this function returns).
* ``compat=true`` (RGB(A) only) short-circuits all of the above and directly returns one of
  the four fixed packed-RGB names.
* ``oldnames=true`` is applied as a final substitution afterwards, and only ever matches one
  of the five specific 8-bit names listed under the ``oldnames`` parameter above - it has no
  effect on any other result.

Because the family/chroma/bits inferred from ``sample_clip`` are read directly off that
clip's own ``VideoInfo`` (not from a fixed table), the returned name is only as meaningful as
the template clip's format is well-formed; passing a clip whose colorspace can't be mapped to
one of the six families (for example a raw/packed ``YUY2`` clip) raises an error rather than
guessing.


Examples
--------

Example #1: define YUV 4:4:4, 10-bit, explicitly::

    family = "YUV"
    bits = 10
    chroma = 444
    s = BuildPixelType(family, bits, chroma)
    BlankClip(width=320, height=200, pixel_type=s, color=$008080)

Example #2: take an existing clip's format, but change only the bit depth to 16::

    c = last
    s = BuildPixelType(bits=16, sample_clip=c)
    BlankClip(width=320, height=200, pixel_type=s, color=$008080)

For YUV clips, :doc:`Greyscale <greyscale>` is internally equivalent to::

    /* assume a YUV(A) clip as the source */
    src = last
    csp = BuildPixelType(sample_clip=src)
    ShowY(pixel_type=csp)

For RGB clips, ``Greyscale(matrix="Rec601")`` is internally equivalent to::

    /* assume an RGB(A) clip as the source */
    src = last
    compat = IsInterleaved(src) ? true : false
    csp = BuildPixelType(compat=compat, sample_clip=src)
    ConvertToY(matrix="PC.601") /* or "PC.709" "PC.2020", "Average" */
    ShowY(pixel_type=csp)
    HasAlpha(src) ? AddAlphaPlane(last, src) : last


Changelog
---------

+------------------+--------------------------------------------------------------------+
| Version          | Changes                                                            |
+==================+====================================================================+
| AviSynth+ 3.7.6  | Add ``"YA"`` family, and ``440``/``410`` chroma support.           |
+------------------+--------------------------------------------------------------------+
| AviSynth+ 3.6.0  | Fix: chroma subsampling of ``sample_clip`` was ignored.            |
+------------------+--------------------------------------------------------------------+
| AviSynth+ r2772  | New function.                                                      |
+------------------+--------------------------------------------------------------------+

.. _YUV:
    http://avisynth.nl/index.php/YUV
