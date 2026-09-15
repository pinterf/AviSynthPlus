=========
ReduceBy2
=========

Set of filters to resize the width and/or height by 2:

* **HorizontalReduceBy2** reduces the horizontal size of each frame by half.
* **VerticalReduceBy2** reduces the vertical size by half.
* **ReduceBy2**, reduces the vertical and horizontal size by half. It is the
  same as ``HorizontalReduceBy2`` followed by ``VerticalReduceBy2``.

The filter kernel used is (1/4,1/2,1/4), which is the same as in VirtualDub's
"2:1 reduction (high quality)" filter. This avoids the aliasing problems that
occur with a (1/2,1/2) kernel.

If the source video is interlaced, the ``VerticalReduceBy2`` filter will
deinterlace it (by field blending) as a side-effect.

.. note::

    Note that, ``ReduceBy2`` is a quick and dirty filter (performance related compromise).
    Unlike the standard :doc:`resize <resize>` filters, the ``ReduceBy2`` filters do not
    preserve the position of the image center. It shifts color planes by half of
    pixel. In fact, ``ReduceBy2()`` is equivalent to:

    ::

        # for RGB
        BilinearResize(Width/2, Height/2, 0.5, -0.5)

        # for YUY420
        MergeChroma(BilinearResize(Width/2,Height/2,0.5,0.5),BilinearResize(Width/2, Height/2,1.0,1.0))

        # for YUV422
        MergeChroma(BilinearResize(Width/2,Height/2,0.5,0.5),BilinearResize(Width/2,Height/2,1.0,0.5))

    See the `"ReduceBy2() introduces chroma shift"`_ thread for more information.


Syntax and Parameters
----------------------

::

    HorizontalReduceBy2 (clip)
    VerticalReduceBy2 (clip)
    ReduceBy2 (clip)

.. describe:: clip

    Source clip; all color formats supported.


Changelog
---------

+-----------------+--------------------------------------------------------------------------+
| Version         | Changes                                                                  |
+=================+==========================================================================+
| v3.7.6          | Horizontal/VerticalReduceBy2: fixed a crash for 4:1:0 sources near the   |
|                 | minimum reducible size (a chroma plane could underflow to a huge invalid |
|                 | row count); also relaxed the overly conservative minimum-size check.     |
|                 | No longer rejects small but valid clips (e.g. a height=2 4:4:4 source).  |
+-----------------+--------------------------------------------------------------------------+
| AviSynth+ r2487 | Horizontal/VerticalReduceBy2: add support for RGB48/64, planar RGB(A)    |
|                 | and YUV(A).                                                              |
+-----------------+--------------------------------------------------------------------------+
| AviSynth+ r2290 | Horizontal/VerticalReduceBy2: add 16/32 bit support.                     |
+-----------------+--------------------------------------------------------------------------+

$Date: 2026/09/15 17:43:00 $

.. _"ReduceBy2() introduces chroma shift":
    https://forum.doom9.org/showthread.php?t=143692
