# Rebuilding the C fixture set

From the repository root, run with Python 3.8 or newer; only the standard library is used:

```sh
python c/tools/generate.py
python c/tools/generate.py --check
```

From another directory, use the corresponding path to `generate.py`; input and output
paths are resolved relative to the repository, not the process working directory.

The first command regenerates the 21 artifacts listed in `generated.json`. The second
generates them in a temporary directory, verifies all input hashes, and reports drift
without changing checked-in files. No terminal installation, network, C++ source tree,
compiled CLI, or ignored `output/` files are needed. Use this entry point rather than
invoking individual historical generators: it also converts long UTF-8 C strings to
NUL-terminated byte initializers supported by MSVC and ISO C implementations.

`generators/` preserves the original capture extraction, field selection, and formatting
logic, with its paths redirected to `c/tests/fixtures/inputs`. The convertible generator
includes the original two-document extension; subscription includes the original wider
selection that joins against the new-bond projection. Historical scripts that edited
unrelated source files or fetched data have been removed from the regeneration path.
The finance ZIP generator still independently constructs its table and archives through
Python's `struct` and `zipfile`; its timestamp and originating platform are fixed.

`c/tests/fixtures/sources.json` distinguishes the SHA-256 of each retained input from
the original full capture's size, count, MD5, and SHA-256 where available. Reduced GBK
tables were checked against the original capture during migration. Binary prefixes,
minute windows, and valuation snapshots explicitly identify migration from the existing
fixture; these preserved inputs are **not a new independent capture**. Original full-file
facts in fixture comments remain provenance, not measurements of the shortened input.
Panorama and cipher-state input files contain only the reference's constant-table excerpts.

When intentionally updating inputs, review their provenance, update their manifest hashes,
regenerate, and run the affected C tests. `--check` rejects changed input bytes until the
manifest is reviewed. Retained capture files have binary Git attributes so checkout line
ending conversion cannot corrupt their hashes.

The presets in `c/CMakePresets.json` use Ninja and separate build directories. From
`c/`, run `cmake --preset linux-gcc`, `cmake --build --preset linux-gcc`, and
`ctest --preset linux-gcc`. Substitute `linux-clang-sanitize` for ASan/UBSan,
`windows-mingw` in a UCRT64 shell, or `windows-msvc` in an x64 Visual Studio developer
shell. All need zlib. POSIX builds also use threads and iconv (normally supplied by
libc on Linux). CMake registers the four Python-based CTest checks only when it finds
a Python interpreter during configuration.

For MSVC, install `zlib:x64-windows` in an existing vcpkg installation, set
`VCPKG_INSTALLATION_ROOT` to that installation, and run from `c/` in developer PowerShell:

```powershell
cmake --preset windows-msvc "-DCMAKE_TOOLCHAIN_FILE=$env:VCPKG_INSTALLATION_ROOT/scripts/buildsystems/vcpkg.cmake" -DVCPKG_TARGET_TRIPLET=x64-windows
cmake --build --preset windows-msvc
$env:PATH = "$env:VCPKG_INSTALLATION_ROOT/installed/x64-windows/debug/bin;$env:VCPKG_INSTALLATION_ROOT/installed/x64-windows/bin;" + $env:PATH
ctest --preset windows-msvc
```

Alternatively, pass compatible `ZLIB_INCLUDE_DIR` and `ZLIB_LIBRARY` paths and make
the matching runtime DLL available. The MinGW sysroot fallback library name `z`
does not configure an MSVC zlib installation.

`linux-fuzz` builds only `tdx-fuzz-codecs`; pass a finite `-runs` and/or `-max_total_time`
when running it. The repository's C workflow checks regeneration, builds these compiler
families, runs CTest, and bounds the sanitizer fuzz run to 30 seconds and 10,000 inputs.
