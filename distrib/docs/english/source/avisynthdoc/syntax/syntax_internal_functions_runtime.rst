
AviSynth Syntax - Runtime functions
===================================

These are the internal functions which are evaluated at every frame. They can
be used inside the scripts passed to runtime filters (:doc:`ConditionalFilter <../corefilters/conditionalfilter>`,
:doc:`ScriptClip <../corefilters/conditionalfilter>`, :doc:`FrameEvaluate <../corefilters/conditionalfilter>`) to return information for a frame (usually
the current one). When using these functions there is an implicit
``last`` clip (the one that is passed to the runtime filter). Thus, first
parameter doesn't have to be specified; it is replaced by the ``last`` clip.

Average
~~~~~~~

::

    AverageLuma(clip[, int offset = 0])
    AverageChromaU(clip[, int offset = 0])
    AverageChromaV(clip[, int offset = 0])
    AverageR(clip[, int offset = 0])
    AverageG(clip[, int offset = 0])
    AverageB(clip[, int offset = 0])

This group of functions return a float value with the average pixel value of
a plane (Luma, U-chroma and V-chroma, R, G or B respectively).
The result is 64 bit double (since 3.7.6, before: 32 bit float).

In v2.61 an offset argument is added which enables you to access other frames than the current one.

*Examples:*
::

    ScriptClip(Last, """
        threshold = 55
        luma = AverageLuma ## gives the average luma of the current frame
        #luma = AverageLuma(1) ## gives the average luma of the next frame
        luma < threshold 
        \ ? Levels(0, 1.0+0.5*(threshold-luma)/threshold, 255, 0, 255) 
        \ : last
        Subtitle("luma=" + String(luma), align=2)
    """)

Difference
~~~~~~~~~~
::

  RGBDifference(clip1, clip2)
  LumaDifference(clip1, clip2)
  ChromaUDifference(clip1, clip2)
  ChromaVDifference(clip1, clip2)
  RDifference(clip1, clip2)
  GDifference(clip1, clip2)
  BDifference(clip1, clip2)

Single-plane versions support only planar YUV or planar RGB clips. 
RGBDifference supports only packed (non-planar) RGB clips (that is RGB24/32/48/64).

This group of functions return a float value between 0 and 255 (or 0 and max_pixel_value for 
bit depths over 8) of the absolute difference between two planes from two different clips.
The result is 64 bit double (since 3.7.6, before: 32 bit float).

Either the combined RGB difference or the Luma, U-chroma or V-chroma, or R, B or B
component differences, respectively.

*Examples:*
::

    ovl = Overlay(last, mov_star, x=some_xvalue, y=some_yvalue, mask=mov_mask)
    ldif = LumaDifference(ovl) # implicit last for clip1
    udif = ChromaUDifference(Tweak(hue=24), ovl)
    ...

Difference from previous
~~~~~~~~~~~~~~~~~~~~~~~~

The "Difference" function groups should be quite handy for detecting scene
change transitions:

::

  RGBDifferenceFromPrevious(clip)
  LumaDifferenceFromPrevious(clip)
  ChromaUDifferenceFromPrevious(clip)
  ChromaVDifferenceFromPrevious(clip)
  RDifferenceFromPrevious(clip1)
  GDifferenceFromPrevious(clip1)
  BDifferenceFromPrevious(clip1)

This group of functions return the absolute difference of pixel value between 
the current and previous frame of clip – either the combined RGB difference or
the Luma, U-chroma, V-chroma or R, G, B differences, respectively. 

*Examples:*
::

    scene_change = (YDifferenceFromPrevious) > threshold)
    scene_change ? some_filter(...) : another_filter(...)

Difference to next
~~~~~~~~~~~~~~~~~~
::

    RGBDifferenceToNext(clip[, int offset = 1])
    LumaDifferenceToNext(clip[, int offset = 1])
    ChromaUDifferenceToNext(clip[, int offset = 1])
    ChromaVDifferenceToNext(clip[, int offset = 1])
    RDifferenceToNext(clip[, int offset = 1])
    GDifferenceToNext(clip[, int offset = 1])
    BDifferenceToNext(clip[, int offset = 1])

This group of functions return the absolute difference of pixel value between 
the current and next frame of clip – either the combined RGB difference or 
the Luma, U-chroma, V-chroma, R, G or B differences, respectively.

In v2.61 an offset argument is added, which enables you to access the difference 
between the RGB, luma, chroma or R/G/B plane of the current frame and of any other 
frame. 

Note that for example ``clip.RGBDifferenceToNext(-1) = clip.RGBDifferenceToPrevious``, 
and ``clip.RGBDifferenceToNext(0) = 0``. 

*Examples:*
::

    # both th1, th2 are positive thresholds; th1 is larger enough than th2
    scene_change = (YDifferenceFromPrevious > th1) && (YDifferenceToNext < th2)
    scene_change ? some_filter(...) : another_filter(...)

Color plane median, min, max, range 
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

See also: 


::

    YPlaneMedian(clip[, int offset = 0])
    UPlaneMedian(clip[, int offset = 0])
    VPlaneMedian(clip[, int offset = 0])
    RPlaneMedian(clip[, int offset = 0])
    GPlaneMedian(clip[, int offset = 0])
    BPlaneMedian(clip[, int offset = 0])
    
    YPlaneMax(clip[, float threshold, int offset = 0])
    UPlaneMax(clip[, float threshold, int offset = 0])
    VPlaneMax(clip[, float threshold, int offset = 0])
    RPlaneMax(clip[, float threshold, int offset = 0])
    GPlaneMax(clip[, float threshold, int offset = 0])
    BPlaneMax(clip[, float threshold, int offset = 0])
    
    YPlaneMin(clip[, float threshold, int offset = 0])
    UPlaneMin(clip[, float threshold, int offset = 0])
    VPlaneMin(clip[, float threshold, int offset = 0])
    RPlaneMin(clip[, float threshold, int offset = 0])
    GPlaneMin(clip[, float threshold, int offset = 0])
    BPlaneMin(clip[, float threshold, int offset = 0])
    
    YPlaneMinMaxDifference(clip[, float threshold, int offset = 0])
    UPlaneMinMaxDifference(clip[, float threshold, int offset = 0])
    VPlaneMinMaxDifference(clip[, float threshold, int offset = 0])
    RPlaneMinMaxDifference(clip[, float threshold, int offset = 0])
    GPlaneMinMaxDifference(clip[, float threshold, int offset = 0])
    BPlaneMinMaxDifference(clip[, float threshold, int offset = 0])

This group of functions return statistics about the distribution of pixel values on 
a plane (Luma, U-chroma, V-chroma, R, G and B respectively). The statistics are, 
in order of presentation: maximum, minimum, median and range (maximum - minimum difference). 

Threshold is a percentage, stating how many percent of the pixels are allowed 
above or below minimum. The ``threshold`` is optional and defaults to 0.

In v2.61 an offset argument is added, which enables you to access the statistics of 
other frames than the current one. 

The decision is based on creating a histogram on pixel values and counts then
the given threshold is checked against this pixel level-pixel count table.

This histogram cannot be done for 32 bit float data thus 32 bit float pixels
are converted to 16 bit integer data before creating the histogram.

Result types: integer for 8-16 bit clips. For 32 bit float clips the minimum and maximum
without threshold are the real pixel values (32 bit float), the other results are 64 bit double.

*Examples:*
::

    # median and average are close only on even distributions; 
    # this can be a useful diagnostic
    have_intense_brights = YPlaneMedian() - AverageLuma() < threshold
    ...
    # a simple per-frame normalizer to [16..235], CCIR, range
    Levels(YPlaneMin(), 1.0, YPlaneMax(), 16, 235)

See more at :doc:`Conditional filters <../corefilters/conditionalfilter>` section.

All-in-one stats
~~~~~~~~~~~~~~~~
::

    PlaneMinMaxStats(clip[, float threshold, int offset, int plane, bool setvar])

Returns an 6-element array with [min,max,thresholded minimum,thresholded maximum,median,average]

The elements are 64 bit double, except min and max of a 32 bit float clip, which are the real
32 bit float pixel values (since 3.7.6, before: all elements were 32 bit float).

See more at :doc:`Conditional filters <../corefilters/conditionalfilter>` section.

--------

Back to :doc:`Internal functions <syntax_internal_functions>`.

Changelog
---------
+----------------+------------------------------------------------------------+
| Version        | Changes                                                    |
+================+============================================================+
| Avisynth 3.7.6 | Average*, *Difference*, PlaneMinMaxStats: 64 bit double    |
|                | results (were 32 bit float). Min, max of 32 bit float clips|
|                | stay 32 bit float pixel values.                            |
|                | Fix: PlaneMinMaxStats thresholded min, max of 32 bit float |
|                | clips with threshold 0.                                    |
+----------------+------------------------------------------------------------+
| Avisynth 3.7.2 | Added a 6th element to PlaneMinMaxStats result             |
+----------------+------------------------------------------------------------+
| Avisynth 3.7.1 | Added PlaneMinMaxStats                                     |
+----------------+------------------------------------------------------------+
| Avisynth+      | Added R, G, B versions of AverageXX, Min, Max,             |
| pre2294        | Difference and Median family.                              |
+----------------+------------------------------------------------------------+
| AviSynth 2.61  | added offset parameters                                    |
+----------------+------------------------------------------------------------+

$Date: 2026-10-19 17:15:00 $

.. _YV12: http://avisynth.nl/index.php/YV12
.. _colorspace: http://avisynth.nl/index.php/Color_spaces
