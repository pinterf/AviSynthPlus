
SetFilterProp / GetFilterProps
==============================

Automatically inject frame properties on the output of a named filter every
time it is instantiated by the script engine.  The rule is registered once
(typically in an auto-loaded ``.avsi`` file) and is applied transparently,
without touching the script that uses the filter.

There are two forms:

- **Simple form** — injects a fixed property value (or a value captured from
  the filter's own call argument) on every instantiation.
- **Conditional form** — injects a property value only when a specific named
  call argument equals a given match value.


Syntax and Parameters
---------------------

*Simple form*

::

    SetFilterProp (string filter, string key, int       value [, int "mode"])
    SetFilterProp (string filter, string key, float     value [, int "mode"])
    SetFilterProp (string filter, string key, string    value [, int "mode"])
    SetFilterProp (string filter, string key, bool      value [, int "mode"])
    SetFilterProp (string filter, string key, func      value [, int "mode"])
    SetFilterProp (string filter, string key, undefined()     [, int "mode"])

*Conditional form*

::

    SetFilterProp (string filter, string param_name, any param_match,
                   string key, any value [, int "mode"])

    GetFilterProps ()

----

.. describe:: filter

    Name of the filter whose output should receive the property.  The
    comparison is case-insensitive and follows the same normalisation rules
    used by ``SetFilterMTMode``.

    When ``SetFilterProp`` is called from inside a plugin's initialisation
    function (``AvisynthPluginInit3``), the plugin's base name is
    automatically prepended (``pluginname_filtername``), exactly as
    ``SetFilterMTMode`` does.

.. describe:: key

    Frame-property key name.  Must start with a letter (``a``–``z``,
    ``A``–``Z``) or underscore, followed only by letters, digits or
    underscores — the same constraints enforced by ``propSet`` and the
    VapourSynth-compatible property API.  Key names are **case-sensitive**.

.. describe:: value  (simple form)

    The value to stamp on every frame produced by *filter*.  Accepted types:

    - **int** — stored as a 64-bit integer property.
    - **float** — stored as a double-precision float property.
    - **string** — stored as a UTF-8 string property.
    - **bool** — stored as a 64-bit integer property (``true`` → ``1``,
      ``false`` → ``0``).  Frame properties have no dedicated bool type.
    - **func** — a function object (lambda or named function) evaluated
      **per frame** with the filter's output clip as its implicit first
      argument.  The function must return an ``int``, ``float``, or
      ``string``.  This allows the injected value to depend on per-clip
      attributes such as resolution or colour format::

          SetFilterProp("Spline36Resize", "_Matrix",
              function[](clip c) { c.Width() >= 1280 ? 1 : 6 })

    - **undefined()** — *capture mode*: instead of a fixed value, the actual
      value of the filter's named call argument whose name equals *key* is
      captured at instantiation time and injected as the property.  The
      property key and the parameter name must therefore be identical.  If
      the filter is not called with that named argument, no property is
      injected for that invocation::

          # Capture the "radius" argument of Blur() and store it as a
          # frame property also named "radius".
          SetFilterProp("Blur", "radius", undefined())

      .. note::
          Capture mode works for both positional and named call sites.
          ``Blur(1.5)`` and ``Blur(radius=1.5)`` are treated identically.

.. describe:: param_name  (conditional form)

    The name of the filter's call argument to inspect.  Case-insensitive.

.. describe:: param_match  (conditional form)

    The value — or **array of aliases** — that *param_name* must equal for
    the injection to take place.

    - **Scalar** (int, float, bool, or string): the condition is true when the
      actual argument value equals *param_match* exactly.
    - **Array of scalars**: the condition is true when the actual argument
      value equals *any* element of the array (int, float, bool, or string).
      This is the idiomatic way to handle the many equivalent string
      representations that colour-science parameters tend to accept::

          # "709", "1", and "AVS_MATRIX_BT709" all map to _Matrix = 1
          SetFilterProp("Convert", "matrix",
              ["709", "1", "AVS_MATRIX_BT709"], "_Matrix", 1)

    Comparisons:

    - string vs string — case-insensitive (``_stricmp``), consistent with
      how AviSynth itself matches named parameters.
    - int vs int, bool vs bool, int vs bool — integer equality; ``true``
      and ``1`` are considered equal, ``false`` and ``0`` are equal.
    - float vs float — exact bitwise equality.

    A type mismatch between the actual argument value and a candidate never
    raises an error; the condition simply evaluates to false.

    If the filter is not called with *param_name* as a named argument, the
    condition cannot be evaluated and the injection is skipped silently.

.. describe:: value  (conditional form)

    The property value to inject when the condition is met.  Accepted types:
    **int**, **float**, **bool**, **string**, **func**.  ``bool`` is stored as
    integer (``true`` → ``1``, ``false`` → ``0``).  ``undefined()`` is not
    accepted here; the value must be known at registration time.

.. describe:: mode

    Controls how the property is written when the frame already carries a
    value for *key*.  Uses the same constants as ``propSet``:

    - ``0`` — **replace** (default): overwrite any existing value.
    - ``1`` — **append**: add to an array alongside the existing value.
    - ``2`` — **touch**: do nothing if the key already exists.

    The default ``replace`` mode includes an optimisation inherited from
    ``SetProperty``: if the existing value is already identical to the new
    value (int, float, or string comparison), the frame is returned
    unmodified without making the property map writable.

    Default: 0 (replace)

.. describe:: GetFilterProps

    Returns all registered rules as a JSON string (array of objects), useful
    for inspection and debugging.

    Simple rules::

        [
          {"filter":"spline36resize","key":"_Matrix","type":"int","value":1,"mode":0},
          {"filter":"blur","key":"radius","type":"capture","value":null,"mode":0}
        ]

    Conditional rules gain two additional fields, ``when_param`` and
    ``when_value``.  When an alias array was registered, ``when_value`` is a
    JSON array::

        [
          {"filter":"colormatrix","key":"_Matrix",
           "when_param":"mode",
           "when_value":["Rec.601->Rec.709","FCC->Rec.709","SMPTE 240M->Rec.709","Rec.2020->Rec.709"],
           "type":"int","value":1,"mode":0},
          {"filter":"convert","key":"_Matrix",
           "when_param":"matrix","when_value":["709","1","AVS_MATRIX_BT709"],
           "type":"int","value":1,"mode":0}
        ]

    Type values in the JSON output:

    ============  ===========================================
    ``"int"``     Static integer value (includes bool, stored as 0/1).
    ``"float"``   Static float value.
    ``"string"``  Static string value.
    ``"function"``  Per-frame function object (value is null).
    ``"capture"`` Capture-from-param rule (value is null).
    ``"unknown"`` Unexpected type (should not occur).
    ============  ===========================================

    Function-valued rules are serialised with ``"type":"function","value":null``.

    Bool values in ``when_value`` (conditional *param_match*) are serialised as
    JSON booleans (``true`` / ``false``).


Placement in the filter graph
------------------------------

The ``SetProperty`` wrapper is inserted **after** both ``MTGuard`` and
``CacheGuard``, and **outside** the "unaltered clip" bypass.  This means:

- The property is stamped even when the filter returns one of its input clips
  unchanged (a common optimisation in no-op conditions).
- The property wrapper is not duplicated on filters that are constructed
  internally as part of another filter's constructor (chained construction).
- The cache sees the property-stamped output, so cached frames already carry
  the correct properties.

For the conditional and capture forms, argument matching happens **before**
``InstantiateFilter`` is called, using the fully-resolved argument array
(``args3``) built from the ``Invoke`` call.  This means:

- Both positional and named call sites are matched: ``ColorMatrix("Rec.601->Rec.709")``
  and ``ColorMatrix(mode="Rec.601->Rec.709")`` are treated identically.
- The matched value reflects exactly what the script passed, not any default
  substitution the filter may apply internally.

Example
-------
::

    ColorbarsHD().ConvertToYV12() # ColorMatrix only accepts YV12
    SetFilterProp("ColorMatrix", "mode", ["Rec.601->Rec.709"], "_Matrix", 1) # 1 = BT.709
    SetFilterProp("ColorMatrix", "mode", "Rec.709->Rec.601", "_Matrix", 6)   # 6 = 170M
    Colormatrix("Rec.601->Rec.709")
    propShow(align=1) # see _Matrix = 1 in bottom left corner
    Colormatrix(mode="Rec.709->Rec.601")
    propShow(align=3) # see _Matrix = 6 in bottom right corner




Typical usage via auto-loaded script
-------------------------------------

Create a file ``my_props.avsi`` in the AviSynth+ auto-load folder and add
rules there.  Because ``.avsi`` files are executed before the main script,
the rules are in place for every subsequent filter instantiation::

    # my_props.avsi  (place in the autoload directory)

    # Tag all Spline36Resize output as BT.709, limited range
    SetFilterProp("Spline36Resize", "_Matrix",     1)   # 1 = BT.709
    SetFilterProp("Spline36Resize", "_ColorRange", 1)   # 1 = limited

    # Derive _Matrix from output resolution (HD → 709, SD → 601)
    SetFilterProp("ConvertToYUV420", "_Matrix",
        function[](clip c) { c.Width() >= 1280 ? 1 : 6 })

    # Do not overwrite _FieldBased if the filter already set it
    SetFilterProp("Bob", "_FieldBased", 0, mode=2)

    # Tag ColorMatrix output with the _Matrix of the destination colorspace.
    # ColorMatrix uses mode="source->dest"; group all modes by their destination.
    # String matching is case-insensitive, so "rec.709->rec.601" matches too.
    SetFilterProp("ColorMatrix", "mode",
        ["Rec.601->Rec.709", "FCC->Rec.709", "SMPTE 240M->Rec.709", "Rec.2020->Rec.709"],
        "_Matrix", 1)   # 1 = BT.709
    SetFilterProp("ColorMatrix", "mode",
        ["Rec.709->Rec.601", "FCC->Rec.601", "SMPTE 240M->Rec.601", "Rec.2020->Rec.601"],
        "_Matrix", 6)   # 6 = Rec.601 / SMPTE 170M
    SetFilterProp("ColorMatrix", "mode",
        ["Rec.709->FCC", "Rec.601->FCC", "SMPTE 240M->FCC", "Rec.2020->FCC"],
        "_Matrix", 4)   # 4 = FCC
    SetFilterProp("ColorMatrix", "mode",
        ["Rec.709->SMPTE 240M", "Rec.601->SMPTE 240M", "FCC->SMPTE 240M", "Rec.2020->SMPTE 240M"],
        "_Matrix", 7)   # 7 = SMPTE 240M
    SetFilterProp("ColorMatrix", "mode",
        ["Rec.709->Rec.2020", "Rec.601->Rec.2020", "FCC->Rec.2020", "SMPTE 240M->Rec.2020"],
        "_Matrix", 9)   # 9 = BT.2020 NCL
    # Integer dest= parameter form (when mode= is not used):
    SetFilterProp("ColorMatrix", "dest", 0, "_Matrix", 1)   # 0 = Rec.709
    SetFilterProp("ColorMatrix", "dest", 2, "_Matrix", 6)   # 2 = Rec.601
    SetFilterProp("ColorMatrix", "dest", 1, "_Matrix", 4)   # 1 = FCC
    SetFilterProp("ColorMatrix", "dest", 3, "_Matrix", 7)   # 3 = SMPTE 240M
    SetFilterProp("ColorMatrix", "dest", 4, "_Matrix", 9)   # 4 = Rec.2020

    # Capture Blur's radius argument as a frame property of the same name
    SetFilterProp("Blur", "radius", undefined())

Multiple calls for the same filter and key are allowed; they are applied in
registration order (the last ``replace``-mode entry that matches wins).


Notes
-----

- Rules registered with ``SetFilterProp`` survive the autoload phase and
  remain active for the entire script session.
- The rule table is global to the script environment; it cannot be modified
  after a ``SetFilterProp`` call has been made from within a plugin init.
- Conditional rules use exact-equality matching.  For range checks or other
  complex logic, use the ``func`` value type in the simple form instead.
- The alias-array form ``["a", "b", "c"]`` for *param_match* is the
  recommended way to handle colour-science parameters that accept many
  equivalent string representations for the same concept.  A single
  ``.avsi`` file with one ``SetFilterProp`` call per canonical value covers
  what would otherwise require a separate call per alias string.
- Multiple conditional rules for the same filter and property are evaluated
  in registration order; only the first matching rule that writes (mode 0 or
  1) takes effect on a given frame if an earlier rule already set the key.
  Use ``mode=2`` (touch) on later rules if the first match should win.
- ``GetFilterProps`` is intended for diagnostics; parsing its output to
  reconstruct a rule table is not supported — write rules directly in
  ``.avsi`` instead.


Examples
--------

**Simple static injection**

The following script demonstrates mode ``0`` (replace): the property is
always overwritten, regardless of any value the filter may have already set.
After ``Spline36Resize(640, 480)`` the output clip is 640×480, so the
function evaluates ``Width() >= 1280`` as false and stamps ``_Matrix = 6``
(BT.601).  The string-valued ``Width`` property and integer-valued
``HeightInt`` property illustrate that function rules can return any
supported type.  ``SubTitle`` shows the rule table; ``propShow`` shows the
properties actually present on the decoded frame.

::

    ColorbarsHD()

    # mode=0 (default) always overwrites the property
    SetFilterProp("Spline36Resize", "_Matrix",
        function[](clip c) { c.Width() >= 1280 ? 1 : 6 }, 0)

    # Function returning a string (String() converts int to string)
    SetFilterProp("Spline36Resize", "Width",
        function[](clip c) { String(c.Width()) })

    # Function returning an int directly
    SetFilterProp("Spline36Resize", "HeightInt",
        function[](clip c) { c.Height() })

    Spline36Resize(640, 480)  # output is SD → _Matrix will be 6

    SubTitle(GetFilterProps())  # display the registered rule table
    propShow(align=1)           # display frame properties on the frame

**Conditional injection — ColorMatrix**

``ColorMatrix`` (the third-party filter) converts between colour matrices
using a ``mode="source->dest"`` string parameter and, alternatively, integer
``source`` / ``dest`` parameters.  The alias-array form lets all modes that
share a destination be grouped into one rule, covering the complete matrix
in five calls::

    # my_props.avsi
    # String mode= form — group by destination, one rule per output matrix.
    # String matching is case-insensitive, so any capitalisation variant works.
    SetFilterProp("ColorMatrix", "mode",
        ["Rec.601->Rec.709", "FCC->Rec.709",
         "SMPTE 240M->Rec.709", "Rec.2020->Rec.709"],   "_Matrix", 1)
    SetFilterProp("ColorMatrix", "mode",
        ["Rec.709->Rec.601", "FCC->Rec.601",
         "SMPTE 240M->Rec.601", "Rec.2020->Rec.601"],   "_Matrix", 6)
    SetFilterProp("ColorMatrix", "mode",
        ["Rec.709->FCC", "Rec.601->FCC",
         "SMPTE 240M->FCC", "Rec.2020->FCC"],           "_Matrix", 4)
    SetFilterProp("ColorMatrix", "mode",
        ["Rec.709->SMPTE 240M", "Rec.601->SMPTE 240M",
         "FCC->SMPTE 240M", "Rec.2020->SMPTE 240M"],    "_Matrix", 7)
    SetFilterProp("ColorMatrix", "mode",
        ["Rec.709->Rec.2020", "Rec.601->Rec.2020",
         "FCC->Rec.2020", "SMPTE 240M->Rec.2020"],      "_Matrix", 9)

    # Integer dest= form — for scripts that use source/dest integers instead.
    SetFilterProp("ColorMatrix", "dest", 0, "_Matrix", 1)   # 0 = Rec.709
    SetFilterProp("ColorMatrix", "dest", 2, "_Matrix", 6)   # 2 = Rec.601
    SetFilterProp("ColorMatrix", "dest", 1, "_Matrix", 4)   # 1 = FCC
    SetFilterProp("ColorMatrix", "dest", 3, "_Matrix", 7)   # 3 = SMPTE 240M
    SetFilterProp("ColorMatrix", "dest", 4, "_Matrix", 9)   # 4 = Rec.2020

    # Main script — no changes needed:
    ColorMatrix(mode="Rec.601->Rec.709")  # → _Matrix = 1 (BT.709)
    ColorMatrix(mode="Rec.709->Rec.2020") # → _Matrix = 9 (BT.2020 NCL)
    ColorMatrix(dest=2)                   # → _Matrix = 6 (Rec.601)

**Capture mode — propagating a call argument as a property**

::

    # Register once in an .avsi file:
    SetFilterProp("Blur", "radius", undefined())

    # Main script:
    Blur(radius=1.5)   # → frame property "radius" = 1.5
    Blur(radius=3.0)   # → frame property "radius" = 3.0
    Blur(0.5)          # positional: property "radius" = 0.5 (same as named)


----


SetFilterPropPassthrough
========================

Compatibility shim for old filters that predate frame-property support.  These
filters create output frames with ``NewVideoFrame`` (not ``NewVideoFrameP``),
silently discarding every frame property that arrived on the input clip.
``SetFilterPropPassthrough`` inserts a transparent wrapper that forwards all
input frame properties to the filter's output.


Syntax and Parameters
---------------------

::

    SetFilterPropPassthrough (string filter)

----

.. describe:: filter

    Name of the filter to wrap.  Case-insensitive, follows the same
    normalisation rules as ``SetFilterProp`` and ``SetFilterMTMode``.

    The first clip-typed argument of the filter is used as the property source.
    If the filter does not take a clip as its first argument the call has no
    effect.


Behaviour
---------

The wrapper is evaluated per frame at ``GetFrame`` time:

1. The filter's output frame is obtained.
2. If the output frame already carries **any** frame properties (i.e. the
   plugin was updated to use ``NewVideoFrameP`` in the meantime), the wrapper
   is a no-op and returns the frame unmodified.  This makes the shim
   **self-healing**: when the plugin author ships a fixed build, the wrapper
   retires itself automatically without any ``.avsi`` changes.
3. Otherwise all properties from the first input clip's corresponding frame are
   copied to the output frame (via ``env->copyFrameProps``).

If ``SetFilterProp`` rules are also registered for the same filter they are
applied **after** the passthrough copy, so specific injections override the
inherited values (subject to the ``mode`` parameter of each rule).


Typical usage
-------------

Place in an auto-loaded ``.avsi`` file alongside any ``SetFilterProp`` rules
for the same filter::

    # my_compat.avsi

    # ColorMatrix (third-party) predates frame properties; forward them.
    SetFilterPropPassthrough("ColorMatrix")

    # On top of the forwarded properties, stamp the correct output matrix.
    SetFilterProp("ColorMatrix", "mode",
        ["Rec.601->Rec.709", "FCC->Rec.709",
         "SMPTE 240M->Rec.709", "Rec.2020->Rec.709"],   "_Matrix", 1)
    SetFilterProp("ColorMatrix", "mode",
        ["Rec.709->Rec.601", "FCC->Rec.601",
         "SMPTE 240M->Rec.601", "Rec.2020->Rec.601"],   "_Matrix", 6)
    # ... remaining rules ...

    # Main script — no changes needed:
    ColorbarsHD().ConvertToYV12()
    ColorMatrix(mode="Rec.601->Rec.709")
    # Output frame carries all properties from ConvertToYV12's output,
    # with _Matrix overridden to 1 (BT.709) by the SetFilterProp rule.

Notes
-----

- Call ``SetFilterPropPassthrough`` **once** per filter name; registering the
  same name twice is harmless but redundant.
- Only the **first** clip argument is used as the property source.  Multi-clip
  filters (dissolves, masked merges, …) are not typical targets for this shim;
  those filters usually postdate frame properties and propagate them natively.
- ``SetFilterPropPassthrough`` is a no-op for filters that are not called with
  a clip as their first argument (source filters, generators).
- Remove the call from your ``.avsi`` when the plugin is updated to propagate
  properties natively.  The self-healing ``propNumKeys > 0`` check means
  leaving it in is safe but wastes a small amount of work per frame.


Changelog
---------

.. list-table::
   :widths: auto

   * - v3.7.6
     - Added ``SetFilterProp`` (simple form: static int/float/string/function value),
       ``GetFilterProps`` (JSON diagnostic dump), and ``SetFilterPropPassthrough``
       (input-property forwarding shim for legacy filters).
   * - v3.7.6
     - Added ``SetFilterProp`` conditional form: inject a property only when a named
       call argument equals a given value or any element of an alias array.
       String comparison is case-insensitive.
   * - v3.7.6
     - Added ``SetFilterProp`` capture mode (``undefined()`` value): capture the
       actual call-time argument value and stamp it as a frame property.
       Works for both positional and named call sites.

