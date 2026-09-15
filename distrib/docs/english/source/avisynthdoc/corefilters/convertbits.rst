ConvertBits
===========

.. rubric:: Syntax and Parameters

::

    ConvertBits(clip, int bits [, bool truerange, int dither, int dither_bits, bool fulls, bool fulld ] )

Changes bit depth while keeping color format the same, if possible.
If the conversion is not possible – for example, converting RGB32 to 14bit – an error is raised.


.. describe:: clip   = (required)

        Source clip. 

.. describe:: bits

     int  bits = (actual bit depth)

        Bit depth of output clip. If provided valid values are: 8, 10, 12, 14, 16 (integer) or 32 (floating point). 
        Parameter is optional when no bitdepth change is needed but doing only range conversion (fulls-fulld) 
        or artistic dithering (dither_bits<bit depth).

.. describe:: truerange

    bool  truerange = true

        **Legacy parameter — avoid using it.** A workaround from the early days of
        AviSynth+ high-bit-depth support, when only 16-bit integer storage existed
        for anything above 8 bits and some tools put 10-bit data inside a 16-bit
        container. Setting ``truerange=false`` makes ``ConvertBits`` treat the
        source's *actual* bit depth as 16 regardless of what its pixel format
        nominally says. Only meaningful for planar sources; specifying it for a
        non-planar source is an error.

        This was left undocumented on purpose for a long time, since it only exists
        for that narrow historical case. It's documented here only because some
        existing scripts and plugins already call ``ConvertBits`` (and
        ``ConvertTo8bit``/``ConvertTo16bit``/``ConvertToFloat``) with positional
        arguments and so already depend on where this parameter sits in the list.
        New code should not use it — pass ``dither``, ``dither_bits``, ``fulls`` and
        ``fulld`` by name instead of positionally so this parameter is skipped
        entirely, and just leave it at its default.

.. describe:: dither

    int  dither = -1

            If -1 (default), do not add dither;
            If 0, add ordered dither;
            If 1, add error diffusion (Floyd-Steinberg) dither doom9 

        Dithering is allowed only for scaling down (bit depth reduction), not up. Bit depth can be kept though
        if a smaller dither_bits is given. 
        
        Note: (behind the scenes) 32 bit float clips are first converted down to 16 (or less if needed) bits, 
        then are further dithered down from this intermediate clip. 

.. describe:: dither_bits

    int  dither_bits = bits

        Exaggerated dither effect: dither to a lower color depth than required by bits argument. 
        The parameter has no effect if dither=-1 (off).

        Arbitrary number from 1 to bits, inclusive. dither_bits = 1 means black and white.
        
        ConvertBits(8, dither=1, dither_bits=2);

.. describe:: fulls

    bool  fulls = (auto)

        Use the default value unless you know what you are doing. 
        Default value can come from _ChromaRange frame property 
        If true (RGB default), scale by multiplication: 0-255 → 0-65535;

        Note: full scale U and V chroma is specially handled
        if false (YUV default), scale by bit-shifting. 
        Use case: override greyscale conversion to fullscale instead of bit-shifts. 
        Conversion from and to float is always full-scale. 
        Alpha plane is always treated as full scale. 

.. describe:: fulld

    bool  fulld = fulls

        Use the default value unless you know what you are doing. 


ConvertBits writes _ChromaRange frame property (0-full or 1-limited) 

Examples
--------

Convert to 16 bits from whatever bit depth.
::

    clip16 = source.ConvertBits(16)

Convert to 8 bits source is full, target is limited rage
::

    clip = source.ConvertBits(8, fulls=true, fulld=false)

Convert to 8 bits source to 32 bit
::

  clip = source.ConvertBits(32,fulls=false, fulld=true)
  # Y: 16..235 -> 0..1
  # U/V: 16..240 -> -0.5..+0.5
  # Note: now ConvertBits does not assume full range for YUV 32 bit float.
  # Default values of fulls and fulld are now true only for RGB colorspaces. Frame prop can help.

Changelog
---------

.. table::
    :widths: auto

    +-----------------+---------------------------------------------------------------------------+
    | Version         | Changes                                                                   |
    +=================+===========================================================================+
    | v3.7.6          || Documented (and discouraged) the previously-undocumented 'truerange'     |
    |                 |  parameter, kept only for scripts/plugins already relying on positional   |
    |                 || Supports 4:4:0 and 4:1:0 formats                                         |
    |                 || Supports 4:1:1 over 8-bits                                               |
    +-----------------+---------------------------------------------------------------------------+
    | 3.7.1           || Support YUY2 (by autoconverting to and from YV16), support YV411         |
    |                 || "bits" parameter is not compulsory, bit depth can stay as it was         |
    |                 || much nicer output for low bit depth targets (dither_bits 1 to 7)         |
    |                 || allow dither down from 8 bit sources by giving a lower dither_bits value |
    |                 || dither=1 (Floyd-S) to support dither_bits = 1 to 16 (similar to ordered) |
    |                 || dither=0 (ordered) to allow odd dither_bits values.                      |
    |                 |  Any dither_bits=1 to 16 (was: 2,4,6,8,..)                                |
    |                 || dither=0 (ordered) allow larger than 8 bit difference when dither_bits<8 |
    |                 || Correct conversion of full-range chroma at 8-16 bits.                    |
    |                 |  Like 128+/-112 -> 128+/-127 in 8 bits                                    |
    |                 || allow dither from 32 bits to 8-16 bits                                   |
    |                 || allow different fulls fulld when converting between integer bit depths   |
    |                 || allow 32 bit to 32 bit conversion                                        |
    |                 || use input frame property _ColorRange to detect full/limited input        |
    +-----------------+---------------------------------------------------------------------------+
    | 3.4             | allow fulls-fulld combinations when either clip is 32bits                 |
    +-----------------+---------------------------------------------------------------------------+
    | r2455           | dither=1 (Floyd-Steinberg): allow any dither_bits value between           |
    |                 | 0 and 8 (0=b/w)                                                           |
    +-----------------+---------------------------------------------------------------------------+
    | r2440 20170310  | new: dither=1: Floyd-Steinberg (was: dither=0 for ordered dither)         |
    +-----------------+---------------------------------------------------------------------------+
    | Avisynth+       | First added                                                               |
    +-----------------+---------------------------------------------------------------------------+

$Date: 2026/09/15 17:46:00 $
