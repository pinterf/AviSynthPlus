release-sample  --  Origin folder for copy-only DLLs
======================================================

This folder provides DLLs that are NOT rebuilt by build_release.bat
but are still included in the release package.

Populate it by copying the relevant files from your previous release
before running build_release.bat.


Expected folder structure
--------------------------

release-sample\
  x86\
    plugins\
      DirectShowSource.dll          (32-bit, copy-only)
      imageseq-xp-legacy\
        DevIL.dll                   (legacy DevIL for old ImageSeq)
        ImageSeq.dll                (legacy ImageSeq with DevIL dependency)
  x64\
    plugins\
      DirectShowSource.dll          (64-bit, copy-only)
      imageseq-xp-legacy\
        DevIL.dll
        ImageSeq.dll


Notes
-----
- DirectShowSource is omitted from the v143/win7 variant by convention.
- imageseq-xp-legacy is only copied into x86-xp and x64-xp variants.
- ImageSeq.dll and TimeStretch.dll in the main plugins\ folders ARE
  freshly built by the script (ENABLE_PLUGINS=on); this legacy bundle
  is for the separate imageseq-xp-legacy\ subfolder only.
- If imageseq-xp-legacy\ does not exist in the origin, that subfolder
  is simply skipped (no error).
