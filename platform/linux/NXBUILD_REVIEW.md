# Linux nxbuild validation findings

Date: 2026-10-19.
Status: Approved decoder corrections applied. Linux simulator compiles and links;
font-provider ownership and locale-string lifetime fixes pass release checks.
The separately approved IndexArray allocator correction passes its two NXTests.
The approved console/window and unused-audio cleanup corrections are validated.
The final optimized build and all seven runtime checks pass. The sanitizer NXTest
suite is 30 passed / 2 failed; both failures report only the 2,928-byte installed
Mesa/EGL leak accepted by the owner for now. That leak remains unchanged and
visible in the tests; no further repair or vendor investigation is pending.
The promoted standard Linux x86-64 toolchain supplies the PIC RTL; the actual
Pascal plugin builds and passes 23 assertions in the real engine through a new
NXTest. Native Apple/Windows backend execution has not been validated.

## Historical baseline: ALmixer decoder callback types disagree

The direct Linux simulator build reaches `Compile30` after all 153 embedded Lua
sources are generated. Clang 18 rejects the callback tables in the actual
MPG123, OGG/Tremor and WAV decoder source files with six incompatible-function-
pointer errors. The build produced 494 object files before stopping; it did not
link or run the simulator.

`external/ALmixer/Isolated/SoundDecoder_Internal.h` declares:

```c
size_t (*read)(SoundDecoder_Sample *sample);
int (*seek)(SoundDecoder_Sample *sample, size_t ms);
```

The corresponding forward declarations and definitions use Uint32 or uint32_t
in `Isolated/LGPL/mpg123.c`, `oggtremor.c` and `wav.c`. These are different
function types on the verified x86-64 Linux target: size_t is 64 bits, while
the implementations use 32-bit read counts and seek arguments. The public
SoundDecoder API and internal buffer sizes also use size_t.

A repair needs to align the decoder declarations and definitions with their
callback contract and review the existing narrowing of read counts and seek
values. No decoder source, callback cast or diagnostic setting was changed in
this pass. This finding remains open for discussion.

## Actual-source NXTest reproduction

Three separate cases are checked in under the sibling Nexus repository:

- `Nexus2D.DecoderABI.MPG123`
- `Nexus2D.DecoderABI.OGG`
- `Nexus2D.DecoderABI.WAV`

Their project is `projects/nxbuild/test/nxbuildEngineTests.nxproject`. Each case
loads this repository's real `Nexus2DEngine.nxproject` through the normal nxbuild
loader/planner, selects the affected actual source file and reuses its native
compiler/options/includes/defines. It adds a syntax-only check and explicitly
treats incompatible function pointers as errors. It does not execute producers,
copy/patch engine code or write object files into source.

From the Nexus repository, with its compiler environment configured and outputs
under the external worktree:

```sh
"$NXBUILD" /action=build /project=projects/nxbuild/test/nxbuildEngineTests.nxproject /targetos=linux /targetcpu=x86_64
"$NX_OUT/nexus2d-decoder-tests/x86_64-linux/nxbuildEngineTests" ../nexus-mobile/Nexus2DEngine.nxproject
```

Verified result: **0 passed, 3 failed, 0 errors/skips**. Each failing NXTest
records the two real callback-type diagnostics for its decoder. The test runner
itself builds successfully and reports zero unfreed heap blocks. The engine
sources are unchanged, and the external evidence records their SHA-256 hashes.
These failing cases are separate from the 37 passing core nxbuild regressions.

Evidence root: `/media/kcollins/dev/worktree/nxproject-unified-wpoyr_f5`.
Full engine retry: `process-fix-engine-1792403820698615472-*` (297.6 seconds).
Regression build: `engine-abi-regression-build-1792404293689582846-*`.
Failing NXTests, command and source hashes: `engine-decoder-abi/`.

## Resolved Process blocker

The first engine attempt stopped during Lua embedding because the Unix Process
unit dropped an empty argument. The owner-approved correction in the sibling
`nexus/packages/nexus-packages/fcl-process` now allocates an actual buffer even
for an empty argument. Its unchanged NXTest passes with zero heap leaks, the
full Process suite passes 12 cases with 3 Windows-only skips, and rebuilt
nxbuild preserves the independent argv probe. All 153 Lua embedding operations
succeed after that correction. No compiler or bootstrap change was required.


## Historical expanded interface review — before implementation, 2026-10-19

The owner requested correctness review across the full decoder interface and
analysis of every change in the proposed upstream patch, rather than limiting
work to the first Linux compilation failure. This section records that expanded
review. Production engine sources and build flags remain unchanged. This is not
an assertion that the interface is correct or that all platform behavior has
been exercised.

### Provenance and upstream patch

Current upstream master was checked through GitHub at
`d74923057d3fc3abad1903b61de5a459d9566363`. The callback header and the three
failing decoder source files are byte-identical to ours. Their history begins
with Corona's initial public import, `b4c4f180a0ff0c80f38100a96518fa8165af9991`
(2018-12-31). The import author does not establish the original defect's author.

[Upstream issue 865](https://github.com/coronalabs/corona/issues/865) reports the
same Linux error. [PR 866](https://github.com/coronalabs/corona/pull/866), head
`2e69928ee191f1056b08fc121b4b42fb634952b1`, remains open and unmerged at review.
Clang made incompatible C function-pointer assignments errors by default in
[version 16](https://releases.llvm.org/16.0.0/tools/clang/docs/ReleaseNotes.html#potentially-breaking-changes).

Every substantive change in that PR was assessed:

| Change | Assessment |
| --- | --- |
| Read return types and seek arguments changed to size_t in MP3, Tremor and WAV | Correct for the SoundDecoder contract. Both declarations and definitions must agree. The alternate Vorbis implementation and internal narrowing still need coverage. |
| MPG123's supported-decoder variable becomes const char ** | Correct: that matches mpg123_supported_decoders(). |
| Cast the two decoder-name arrays to char ** for print_decoders() | The current helper only reads the strings, so these casts do not themselves introduce writes. However, the helper's parameter should also be const char **, which removes the mismatch and preserves the type checks without casts. |
| Reverse HAVE_STDLIB_H, HAVE_STRING_H and HAVE_UNISTD_H from ifdef to ifndef | Fixes this incomplete Linux configuration by coincidence, but reverses the flags' documented meaning. src/config.h.in explicitly says they mean the named header exists; configure.ac checks them; the existing Android recipe defines HAVE_STDLIB_H and the MSVC configuration defines HAVE_STDLIB_H/HAVE_STRING_H. Reversal omits headers when available and requests unistd.h when it is not declared available. Correct the configuration instead. |
| Whitespace/newline changes | No functional effect. |

The actual Linux Compile32 settings reproduce five missing-declaration errors
in unchanged libmpg123/compat.c (malloc, realloc, strlen, strcpy, close).
A diagnostic-only invocation adding the three HAVE_* definitions compiles that
same file successfully. The recipe and header have NOT been altered, and the
NXTest against actual recipe settings remains failing. This is a proposed
configuration correction, not a passing result for the current build.

### Interface coverage and findings

The review covered all seven callback slots (init, quit, open, close, read,
rewind, seek), the shared dispatcher, sample ownership, buffers, EOF/error
flags, size and seek conversions, duration values, immediate RWops adapters,
and ALmixer consumers. All six implementations present were inspected:
MPG123, Tremor, Vorbis, WAV, CoreAudio and DirectX. The OPUS registration has no
implementation in the tree; it was not counted as a tested decoder.

| Area | Finding and correction direction |
| --- | --- |
| Four portable codecs | MPG123, Tremor, Vorbis and WAV have the read/seek signature mismatch. Other callback slots match the shared signatures by source inspection. |
| Ogg read errors | Both Ogg adapters set ERROR on a negative ov_read result but return that value converted to uint32_t. A negative error becomes a huge byte count. Return the valid bytes actually produced, preserving ERROR separately. |
| DecodeAll | Copies the returned count even when ERROR is set, without checking it against the allocated sample buffer; accumulation also lacks overflow checks. A deterministic decoder reporting 9 bytes for an 8-byte buffer reproduces an out-of-bounds read under AddressSanitizer. This models the invalid-count interface failure without allocating gigabytes or claiming a real corrupt-Ogg reproduction. |
| Quit/sample ownership | Quit removes the sample from the list, then FreeSample rejects it because it is no longer in that list. The decoder close and stream close callbacks are never called. Let one routine own removal and destruction. |
| Zero-size resize | On the tested Linux allocator, realloc(buffer, 0) frees the old allocation and returns NULL. SetBufferSize reports failure but retains the freed pointer and old size. The public failure path must preserve usable state; define/reject zero sizes before allocation. |
| CoreAudio seek/rewind | Both return success even when ExtAudioFileSeek reports an error. The shared wrapper then clears the error flag. Return failure when the platform call fails. This is source-confirmed; no Apple runtime execution was performed. |
| CoreAudio size boundary | The size_t buffer size is cast directly to UInt32. Use explicit bounded reads or a validated limit before entering the platform API. Matching outer signatures alone does not establish full-width behavior. |
| DirectX size/time boundary | The outer read callback is size_t, but BaseDecoder/ACM/MF ReadUsing returns int; ACM narrows its size_t total and MF accumulates into int. Duration is also int, and milliseconds-to-100ns conversion can overflow. Review the complete internal conversion path, with explicit native API limits. Native Windows behavior was not executed. |
| Portable seek/count boundaries | MPG123 narrows an already-size_t count to Uint32 and casts off_t seek results to int. WAV seeks pass through uint32_t time/offset helpers and signed int offsets. Tremor uses int lengths/counters. Keep real format-width fields fixed-width, but bound/chunk runtime counts and reject unrepresentable seeks rather than silently truncating. |
| RWops errors | Its read/write comments promise an error sentinel, while the stdio implementation returns fread/fwrite counts and discards ferror information. Decoder callers cannot reliably distinguish I/O error from EOF. The error contract needs one consistent definition and propagation path. |

Additional source-review concerns requiring dedicated fault-injection coverage:
failed sample allocation does not consistently close a stream already opened by
NewSampleFromFile; Internal_LoadSample ignores a failed LinkedList_PushBack;
CoreAudio_open has allocation-failure exits that retain earlier allocations.
The vendored mpg123 config also hard-codes 32-bit sizeof metadata on this 64-bit
target. These are recorded for the correctness pass, not claimed runtime-tested
or silently repaired. The scope is the decoder interface and its immediate
adapters/consumers, not a rewrite of the underlying codec algorithms.

### Expanded actual NXTests

The same nxbuildEngineTests project now contains eight cases:

- Three original callback ABI tests: fail on unchanged sources.
- MPG123Headers: fails on unchanged compat.c with the actual Linux recipe flags.
- ExplicitFree: passes; real dispatcher calls both close callbacks once.
- QuitClosesSamples: fails; both close counts are zero instead of one.
- ZeroResizePreservesState: fails with AddressSanitizer heap-use-after-free.
- RejectInvalidErrorCount: fails with AddressSanitizer heap-buffer-overflow.

Result: **1 passed, 7 failed, 0 test errors/skips**. The Pascal NXTest runner has
zero unfreed heap blocks. Those runner results do not erase the native memory
failures detected in child probes. No production fixes have been applied.

The deterministic fixture implements only a test decoder through the real
SoundDecoder_DecoderFunctions interface. The tests compile the actual,
unchanged SoundDecoder.c, ALmixer_RWops.c, tErrorLib.c and LinkedList.c; no
implementation is copied, extracted or patched. Their compiler comes from the
real engine plan. The fixture selects its own single decoder registration and
adds AddressSanitizer/UndefinedBehaviorSanitizer. Native outputs go beside the
external NXTest executable. No source outputs, new threads or bootstrap changes.

Build evidence: `decoder-contract-review-complete-1792412739165809050-*`.
Run evidence: `decoder-interface-review/tests.log` and `results.json` under the
existing evidence root. The recommended next work is a coherent interface
correction with regressions for every repaired path; native platform findings
must retain explicit validation limits until their platform checks can run.


## Approved interface corrections — 2026-10-19

The owner approved implementation after requiring complete interface review and
analysis of every substantive PR change. Corrections cover all four portable
callback tables and their inner paths, CoreAudio/DirectX bounds and return values,
the dispatcher, RWops, and immediate OpenAL consumers. No threading implementation,
bootstrap stage, compiler diagnostic setting or external build generator changed.

- Read callbacks return valid size_t counts, with errors/EOF separate. Ogg returns
  valid partial data on error; requests fit codec widths and whole PCM frames.
  Its seek callback maps RWops positions to the zero-success convention required
  by [Vorbis](https://xiph.org/vorbis/doc/vorbisfile/callbacks.html), and I/O errors
  propagate through flags and errno.
- Seeks use checked conversions. WAV keeps unsigned RIFF sizes, checks long file
  offsets, accounts for padding, handles EOF seeks and short ADPCM output buffers,
  and seeks within ADPCM blocks without mixing compressed and decoded byte counts.
- CoreAudio reports seek/rewind failures, chunks reads to UInt32 limits, honors
  8-bit output and cleans up reviewed failed-open exits. Seek uses the retained
  file sample rate and frame units required by
  [ExtAudioFileSeek](https://developer.apple.com/documentation/audiotoolbox/extaudiofileseek(_:_:)).
- DirectX preserves size_t counts through ACM/MF copies and ptrdiff_t durations;
  native offsets/timestamps are checked before conversion. Synchronous I/O adapters
  preserve errors and file lengths exclude a fictitious extra byte. The asynchronous-read result
  storage also widens from signed long to int64_t to preserve ULONG byte counts.
  Thread creation, scheduling and synchronization are unchanged.
- FreeSample owns list removal/destruction during shutdown. NewSample closes its
  stream on allocation failure and handles failed list insertion. Zero buffers
  are rejected before realloc. DecodeAll checks returned counts, accumulation,
  allocation failure and zero progress.
- PCM metadata describes the actual returned bytes: no generic converter exists,
  so desired format is a decoder hint. Retaining an unfulfilled request previously
  mislabeled the data. This is now documented in the public header and tested.
- ALmixer checks OpenAL's signed size limit before narrowing. It no longer discards
  an initial short buffer and treats the next tail as the entire sound. Duration
  preallocation no longer narrows to int.
- MPG123 declares available headers using positive HAVE_* flags and type sizes for
  the retained native data models. compat.h's tests retain their original meaning.
  The decoder-name helper accepts const data without a cast.

An untouched before snapshot is in `decoder-interface-fix/before/` under the
external evidence root. Original evidence remains **1 pass/7 fail** in
`decoder-interface-review/`. Final NXTest results are **24 passed, 1 failed,
0 errors/skips**, with zero Pascal runner heap leaks. All 21 interface/configuration
tests and the actual-codec integration test pass. Two simulator exit scenarios
pass; the original application scenario fails as detailed below. Evidence:
`decoder-interface-fix/final-width-tests.log` and its `-command.json` companion.

Native probes use ASan/UBSan and actual implementation files. Vorbis/MPG123/CoreAudio
API doubles exercise deterministic failure/boundary paths; these are not native
codec/platform passes. Actual WAV tests exercise PCM/ADPCM seek/replay, errors,
RIFF padding and short buffers. Actual engine mpg123/Tremor objects decode existing
MP3/Ogg assets, with rewind/seek replay comparisons; Ogg DecodeAll reaches EOF.
The mixer regression verifies that an initial short read remains streamed and
preserves its first bytes, using actual ALmixer and the OpenAL null driver.

The final actual simulator build generated all 153 Lua embeddings, produced
**594 object files**, and linked through nxbuild in **322.74 seconds (5m23s)**.
Evidence: `decoder-engine-final-1792414839218053965-*`. Its ELF dependencies
resolve locally. The executable is 7,529,256 bytes, SHA-256
`841ad41891e7e35b1267ad75c2c99ca388dd3940852f41c0d0443fd2eca1a495`.
The last signed-read-width correction was checked against the bundled MSVC header
(where ssize_t is long, including Win64). Recompiling the final Linux mpg123
adapter produces an object byte-identical to the final engine's object; evidence:
`decoder-interface-fix/mpg123-final-object-check.json`. The nxbuild environment
was restored byte for byte after the build.

### Initial runtime failure evidence: Lua exit during shutdown

The offscreen smoke application creates a rectangle and its timer calls os.exit(0).
The real simulator aborts with `corrupted double-linked list`. GDB shows os_exit
from the live Lua call stack; exit handlers destroy SolarSimulator/SolarApp/
SolarAppContext, then glibc detects heap corruption when Rtt::Config::Save allocates
its stdio buffer. This is the detection point, not the corrupting operation.
The initial review had not yet established the corrupting operation. The
follow-up investigation below identifies it and the new recipe's role.

`Nexus2D.EngineRuntime.OriginalApplicationExit` preserves this as a failing NXTest.
Pass the actual simulator as argument 2 and the original reproduction main.lua
as argument 3; the test uses that file's directory as its working directory.
Both a configured fixture and a minimal fixture pass. The minimal fixture has
byte-identical Lua contents to the original failing input (SHA-256
`1c8acecb50d2c7fbcc5a5242b9a8995705123afe68a48252470d450705dbda3c`).
The initial controls varied both the application path and working directory.
Follow-up controls below isolate those factors. Missing config files alone
have not been shown to cause the failure.

To run every case, including the failing original scenario:

```sh
NX_EVIDENCE=/media/kcollins/dev/worktree/nxproject-unified-wpoyr_f5
"$NX_EVIDENCE/products/nexus2d-decoder-tests/x86_64-linux/nxbuildEngineTests" \
  /media/kcollins/dev/source/nexus-mobile/Nexus2DEngine.nxproject \
  "$NX_EVIDENCE/products/nexus2d/x86_64-linux/Solar2DSimulator" \
  "$NX_EVIDENCE/decoder-interface-fix/runtime-smoke/main.lua"
```

The final run exits 1, with 24 passes and this one failure. The same final
simulator also aborts in all three direct original-scenario replays, recorded in
`decoder-interface-fix/runtime-smoke/final-replays.json` and `final-replay-*.log`.
Earlier stack/input evidence is in that directory's `gdb.log` and `evidence.json`.
The tests use SDL offscreen video and OpenAL null audio, so do not validate
physical hardware. Shutdown implementation has not been changed or bypassed.
The migration remains incomplete: this failure and the separately recorded
matching-PIC-RTL requirement for the Pascal plugin remain open.

### Platform limits

CoreAudio is compiled/run against an explicit test API double on Linux, testing
changed adapter logic and allocations, not Apple ABI/framework behavior. There is
no Apple or Windows SDK here. DirectX has source review and shared arithmetic-helper
regressions, but has **not** been compiled/run against Windows SDK. No full native
matrix validation is claimed. Actual codec integration tests use the engine's
own compiled mpg123/Tremor objects and existing nexus-lab audio assets.


## Shutdown investigation before correction — 2026-10-19

**Historical diagnosis before the approved corrections below.** The
new nxbuild recipe exposed an existing dependency on static destruction order.
This is not an unexplained allocator failure or a proven decoder defect.

### Exact corrupting operation

1. `main.cpp` owns the application in a global `smart_ptr<SolarApp> app`.
   `Rtt_Freetype.cpp:59` separately owns the font provider in a global smart pointer.
2. In the actual linked executable, the application's global initializer runs
   before the font provider's initializer. Process exit destroys them in reverse
   order. Lua `os.exit(0)` starts that process without the explicit `app = NULL`
   cleanup at the end of main.
3. GDB records the font provider being destroyed and freed. Later,
   `SolarAppContext::~SolarAppContext` calls `setGlyphProvider(NULL)` at line 91.
   The already-destroyed global smart pointer still contains the freed address.
4. `smart_ptr::set_ref` calls `drop_ref` on that freed object. The instruction at
   `0x788291` in the recorded release executable decrements the four bytes at
   object offset 8. That is now part of glibc's free-list metadata. GDB records
   the value changing from `0x8bb750` to `0x8bb74f`.
5. `Config::Save` then asks malloc for a stdio buffer. Malloc detects the damaged
   list and aborts. Config::Save is the detection point, not the source of damage.

Evidence: `glyph-lifetime-gdb.log` records destruction and later reuse of the
same pointer; `glyph-invalid-write-gdb.log` records the exact write and abort.
Those addresses are specific to the recorded binary/run, not stable API values.

### Why the new build exposed it

The old Linux source list declares `Rtt_Freetype.cpp` before `main.cpp`.
The new `Nexus2DEngine.nxproject` compiles that order, but its final link takes
`objects/33/*.o`. `TNXBuildEntry.ExpandSourceFiles` retains FindFirst/FindNext
directory enumeration order. Here main.o is entry 6 and Rtt_Freetype.o is entry
38 in that directory, reversing their relative link order. The actual ELF
`.init_array` confirms application initialization precedes font initialization.
The wildcard enumerator already exists in Nexus HEAD; its use in the new engine
recipe, written during this migration, exposed the lifetime dependency.

A diagnostic relink uses the same 594 compiled objects and link options:

| Control | Original application's result |
| --- | --- |
| Filesystem object order | SIGABRT, corrupted double-linked list |
| Only main.o and Rtt_Freetype.o exchange positions | Exit 0 |

The controls are in `link-order-controls/`, including exact arguments. They are
diagnosis, not an accepted repair or a replacement for the failing NXTest. The
original release executable remains byte-identical, SHA-256
`841ad41891e7e35b1267ad75c2c99ca388dd3940852f41c0d0443fd2eca1a495`.
No claim is made that the historical external build was executed here.

The unsafe ownership arrangement and the context's reset call are present in
upstream import commit `0199bc0b75fd160a3d187e8650219cd8835ffd86`,
2021-12-20, “Initial Linux simulator support (#315)”. These source files have
no local production edits. Nevertheless, the new recipe changed the observed
behavior; this is a migration failure that must be corrected.

The release path matrix isolates cwd: the original input aborts with either its
own directory or the executable directory as cwd; the minimal fixture exits 0
with either cwd. Identical Lua bytes therefore do not imply equal heap layout.
The cause of the corrupting write is established even where it does not produce
an immediate allocator abort. Evidence: `release-path-matrix.json`.

### Additional startup failure found by AddressSanitizer

The actual nxbuild recipe also built all engine sources with AddressSanitizer,
debug information and frame pointers in a separate output root. Build time:
430.97 seconds. Recipe and environment bytes were restored afterward. No engine
implementation copy was patched and no sanitizer diagnostic was suppressed.

All three original runtime scenarios stop earlier with **stack-use-after-scope**
in `LinuxPlatform::GetPreference`. Lines 561 and 564 assign
`localeName.substr(...).c_str()` to resultPointer. Each temporary string dies at
the end of its assignment statement; `value->Set(resultPointer)` at line 593
then reads the expired storage. The actual captured startup call is the country
case. This is a stack read defect, separate from the proven freed-heap write.
These assignments originate in `134c0787b` (2022-02-23, Linux SDL/imgui migration).

Added `Nexus2D.EngineRuntime.LocalePreferenceLifetime`, with an actual Lua fixture
requesting language, country and locale identifier. It runs the real simulator;
the startup path already reaches the failing country query. Under ASan, this
test and the other runtime cases fail before completing their Lua fixtures.
That prevents claiming a clean sanitizer shutdown run; it does not invalidate
the direct GDB evidence from the release engine.

Final NXTest results with the new case:

| Engine | Passed | Failed | Evidence |
| --- | ---: | ---: | --- |
| Unchanged release | 24 | 2 | `nxtests-release.log` |
| AddressSanitizer diagnostic | 22 | 4 | `nxtests-address-sanitizer.log` |

The logs retain all failures. Both Pascal runners report equal allocated/freed
block counts; their final heaptrc summary line is truncated in the captured output.
The release locale case receives nil for language and fails its nonempty-string
assertion. The original shutdown NXTest remains active. The ASan results are not
used to replace or erase the earlier release failure.

### Recommended corrections

- Give SolarAppContext ownership of its font provider, keep the shared accessor
  non-owning, and release the provider after runtime teardown. This removes the
  dependency on C++ global destruction order. Merely nulling an already-destroyed
  smart pointer or rearranging link inputs is not an ownership correction.
- Retain the selected locale text in an owning std::string until value->Set has
  copied it (or copy it while the temporary is still alive). Preserve the current
  locale values; detecting the machine's actual locale is a separate feature.
- Explicitly list the produced objects in the engine recipe in the intended
  order, as the owner suggested. This is a small recipe correction, not a reason
  to redesign the build machinery. The font-provider ownership correction is
  likewise limited to the existing Linux context and font-provider implementation
  (three files); no new shutdown subsystem is proposed.

No production lifetime, thread, bootstrap or locale behavior was changed during
this investigation. Only the additional regression and review documentation
were added. Evidence root for this section:
`/media/kcollins/dev/worktree/nxproject-unified-wpoyr_f5/runtime-heap-investigation-1792416362373527003`.


## Approved lifetime and explicit-link corrections — 2026-10-19

The owner approved the small context-ownership correction, locale-string fix,
and explicit link inputs, avoiding a broader shutdown-system redesign.

- SolarAppContext now owns its glyph provider, constructs it before the runtime,
  and deletes it after runtime/platform teardown. The global accessor is a
  non-owning pointer, cleared before deletion. This changes three Linux files.
- LinuxPlatform::GetPreference retains its selected text in a std::string until
  value->Set copies it. Existing locale values are preserved.
- Nexus2DEngine.nxproject lists all 594 objects explicitly in producer source
  order. The list matches actual outputs exactly, with no duplicates or wildcard
  link inputs. No build-executor redesign was needed.

The production C++ source/header diff is **15 added / 8 removed lines across four
files**. No bootstrap or thread behavior changed.

The actual nxbuild release rebuild passes in **334.28 seconds**,
including all 153 Lua embeddings and 594 native objects. Executable: 7,529,248
bytes, SHA-256 `13ce2a342d794a5ea4900b564a4fadb58f390947af465a933fa1235089056cab`. Build evidence:
`engine-lifetime-corrected-1792417265322627650-*`.

All **27 behavior checks pass against the release engine**, including the
unchanged original application, locale preferences, and new FontProviderLifetime
test, which creates real text and keeps it alive through shutdown. The corrected
objects were also relinked in both the formerly failing filesystem order and the
alternate font-before-app order. The original application, locale fixture and
populated-font fixture all exit 0 in both orders. This independently validates
the ownership repair rather than relying on the explicit link order to hide it.

Control evidence: `runtime-heap-investigation-1792416362373527003/link-order-controls-after-fix/`.
The source snapshots and original failing executable remain in that evidence
root's `before-fix/` directory. Final build/test evidence is in `runtime-heap-investigation-1792417632613682760/`.

### IndexArray finding and separately approved correction

The corrected full ASan engine passes the former locale failure point, then
aborts during startup with `alloc-dealloc-mismatch (malloc vs operator delete)`.
The exact source is `librtt/Core/Rtt_StringHash.h`:

- Reserve at line 48 allocates with Rtt_MALLOC, which maps to malloc in the Linux
  system-allocator configuration.
- The destructor at line 37 uses Rtt_DELETE, which maps to C++ delete.
- StringHash::tryTable constructs and destroys an IndexArray<unsigned int> during
  Lua group-method setup; its 16-byte allocation produces the captured failure.

Both current IndexArray element types are primitive integers (unsigned int and
unsigned short). The surrounding StringHash key allocations use malloc/strndup
with free correctly. The mismatched pair is present in the initial public import
`b4c4f180a0ff0c80f38100a96518fa8165af9991`; that import does not identify the original
defect author. This allocation pair is independent of linker order.

Added actual NXTests `Nexus2D.Memory.IndexArrayUInt32Allocation` and
`Nexus2D.Memory.IndexArrayUInt16Allocation`. They compile the actual IndexArray
header and actual Rtt_Allocator.cpp with the real engine recipe's compiler,
includes/defines and AddressSanitizer. They proved the failure using the unchanged
actual header, then passed using its approved one-line correction. No implementation
copy was patched to obtain either result.

The first release-suite attempt recorded 27 passes and two **probe compile
failures**, because the new fixture omitted Core/Rtt_Build.h. After correcting
that include, the first full ASan NXTest run exposed a second test-command error:
the standalone probe link omitted the C++ runtime library. Both historical logs
are preserved. Adding only -lstdc++ linked the probe but missed the sanitizer's C++ delete
interceptor, causing two false passes against the unchanged header. Symbol
inspection confirmed operator delete was imported from libstdc++, while malloc
was instrumented. Those results are retained as invalid regression coverage.
The NXTest command now uses --driver-mode=g++ so Clang links its C++ sanitizer
runtime. The rebuilt suite confirms both cases compile and fail on the intended
allocation mismatch. The conclusive before-fix run is
`nxtests-address-sanitizer-cxx-driver.log`; saved probe arguments are under
`nxtests-cxx-driver/`.

The separate direct probes also compile and reproduce the mismatch, recorded in
`index-array-*-compile.log`, `index-array-*-run.log`, and
`index-array-isolated-results.json`. No production source was changed to obtain
these reproductions.

The conclusive before-allocator-fix instrumented run is **22 passed / 7 failed**: the two isolated
allocation cases plus five real-engine runtime cases stop on this same allocator
defect. The release engine's 27 behavior checks pass. Both Pascal runners report
equal allocated/freed block counts; their final heaptrc summary line is truncated
in the captured output. Full logs remain in the evidence directory.

The owner approved inclusion after asking why the failure surfaced now. The
one-line correction is applied: Rtt_FREE(fElements) matches Rtt_MALLOC under both
system and custom allocator configurations. Both unchanged native regression
commands now pass against the corrected actual header. Final build and NXTest
results are recorded below.

Why it surfaced now: the new ASan build checks allocation/release pairing. The
first ASan engine stopped earlier at the locale temporary-string error. Once
that was corrected, execution reached this allocation mismatch. The ordinary
release build did not diagnose the mismatched APIs. Unlike the original shutdown
bug, this mismatch does not depend on object link order. The preserved full
engine traces and isolated before/after probes distinguish these causes. No
sanitizer diagnostic was disabled.

The separate Pascal plugin's matching-PIC-RTL requirement and native Apple/Windows
SDK validation remain open. These results do not complete the entire migration.


## Final validation after IndexArray correction — 2026-10-19

- Actual release nxbuild rebuild: **319.76 seconds**, exit 0.
  Original application, locale, populated-font, minimal-exit and configured-exit
  scenarios all exit 0. Release SHA-256: `8188ca8a5ac925023e2aabdfb4749844e424854bfa796ca70eca44aa0b341472`.
- Full instrumented nxbuild rebuild: **421.33 seconds**, exit 0.
  ASan executable SHA-256: `0d9aca32c9f096183189b45d37a2ba1866c77963d4960ea5361ad55a062d0881`.
- Actual NXTest suite against that instrumented executable: **24 passed / 5 failed**.
  Both IndexArray cases pass. All 22 decoder/codec cases pass. The five runtime
  cases fail at exit with LeakSanitizer reports. The former locale use-after-scope
  and malloc/delete mismatch are absent from this run.
- The failing runtime cases are LocalePreferenceLifetime, FontProviderLifetime,
  LuaExitWithConfig, MinimalLuaExit and OriginalApplicationExit. The original,
  minimal and configured direct runs also reach their Lua completion markers,
  then report 252 bytes leaked in four allocations. No sanitizer error was
  disabled or reclassified as a pass.
- Recipe and environment bytes were restored exactly after instrumentation.
  Generated products, commands and complete logs remain under
  `runtime-heap-investigation-1792419607458224576/` in the external worktree. `final-validation-summary.json`
  records the results and artifact identities.

### Remaining console/audio cleanup defects

The completed runtime path now allows LeakSanitizer's exit-time scan to run.
The direct engine traces identify:

| Allocation | Bytes | Observed cleanup gap |
| --- | ---: | --- |
| Console ImGui SDL backend | 120 | Window::~Window does not call ImGui_ImplSDL2_Shutdown |
| Console ImGui OpenGL backend | 96 | Window::~Window does not call ImGui_ImplOpenGL3_Shutdown |
| PlatformNotifier | 32 | QuitOpenALPlayer returns before AttachNotifier(NULL) if audio was never initialized |
| Notifier-associated Lua resource reference count | 4 | Retained by the leaked notifier |

Window's constructor initializes both backends. Its destructor destroys only the
ImGui context and SDL window; the backend shutdown functions contain the matching
releases. The same destructor body is present in commit 134c0787b from February
2022. Runtime::LoadApplication attaches the notifier independently of lazy audio
initialization, while QuitOpenALPlayer's early return bypasses its notifier cleanup.
That return is present in the initial public import. These are source-reviewed
causes consistent with the captured allocation traces; their fixes have not yet
been implemented or validated.

The immediate-exit locale test reports **3,180 bytes in seven allocations**.
It includes the same 252-byte engine leaks plus 2,928 bytes in three allocations
whose traces enter SDL's graphics initialization and libdrm, with some frames in
an unloaded/unsymbolized module. The exact ownership cause of those additional
allocations is not established. They remain part of the failing test; they have
not been dismissed as external-library noise.

The existing runtime NXTests now preserve these failures. Recommended next work:
complete the console window's backend/GL-context teardown with the correct current
contexts, ensure notifier cleanup runs even when audio never initialized, and
investigate the extra immediate-exit SDL/graphics allocations.
Keep this as a small cleanup correction; no shutdown-system redesign is indicated.
The affected Window and PlatformOpenALPlayer implementation files remain unchanged
in this batch. The overall sanitizer result remains a failure until resolved.


## Investigation of the five runtime failures — 2026-10-19

Production implementation is unchanged during this investigation. The exact
previously built sanitizer executable was reused (SHA-256 `0d9aca32c9f096183189b45d37a2ba1866c77963d4960ea5361ad55a062d0881`).
All original runtime failures were reproduced. The immediate-exit locale test
reported the same result in three consecutive runs:

| Existing NXTest | Leak bytes | Allocations |
| --- | ---: | ---: |
| LocalePreferenceLifetime | 3,180 | 7 |
| FontProviderLifetime | 252 | 4 |
| LuaExitWithConfig | 252 | 4 |
| MinimalLuaExit | 252 | 4 |
| OriginalApplicationExit | 252 | 4 |

### Engine console/window cleanup: 216 bytes, plus missing GL context cleanup

GDB observed two SDL GL-context creations. Only the main context was deleted.
Both ImGui backend shutdown calls ran against the main ImGui context. The console
retained its separate 120-byte SDL backend and 96-byte OpenGL backend allocations.

The ordering defect is more specific than the initial source review established:
SolarApp::~SolarApp calls SDL_Quit inside its destructor body, but fConsole/fDlg
are smart-pointer members released after that body. The trace shows SDL_Quit
before Window::~Window. That destructor destroys the ImGui context and window,
but omits both backend shutdowns and SDL_GL_DeleteContext. Thus adding backend
cleanup alone would attempt it after SDL had already shut down.

Recommended correction: explicitly release dialogs while the runtime is still
alive, then the runtime, and release the console before main graphics/SDL teardown.
Window destruction must shut down its own backends and delete its own GL context
with the appropriate contexts current. Restore a surviving context where needed.
DlgBuild::~DlgBuild calls app->Resume(), so moving dialog destruction after
runtime destruction would introduce an invalid dependency. No threading changes
or new shutdown framework are needed for this ownership correction.

The relevant main destructor cleanup lines originate in bc04b0951 (March 2022);
the incomplete Window destructor is present in 134c0787b (February 2022).

### Engine unused-audio cleanup: 36 bytes

GDB stopped in QuitOpenALPlayer with isInitialized=false and a non-null notifier.
The early return skips AttachNotifier(NULL). Runtime startup had allocated the
32-byte PlatformNotifier; that object keeps the 4-byte Lua resource reference
count alive. This branch is present in the initial public import.

A matched pair of unmodified-engine controls confirms the size and trigger:
- Audio unused: 252 bytes in four allocations.
- Load a short WAV successfully and dispose it before exit: 216 bytes in two
  allocations, solely the console ImGui backends.

Recommended correction: ensure the notifier is released on the uninitialized
cleanup path, preserving the existing cleanup order when audio was initialized.

### Installed EGL stack: separate 2,928-byte unload leak

Captured process maps resolve the previous unknown frames to libEGL_mesa.so.0
and libEGL.so.1. The allocation sizes are 2,680 + 144 + 104 bytes. The latter
traces include libdrm. This is independently reproducible without Nexus code:

1. A standalone SDL control creates two contexts and correctly destroys both
   contexts and windows before SDL_Quit: 2,928 bytes leaked. Omitting the second
   context, using late cleanup, or swapping before exit does not change that
   control's leak total.
2. An EGL-only control contains no engine, Lua, ImGui, audio or SDL code. It loads
   libEGL.so.1, queries a device, obtains and initializes a display, successfully
   calls eglTerminate and eglReleaseThread, then dlclose. It leaks the same
   2,928 bytes in three allocations.
3. Five complete EGL-only load/use/unload cycles leak 14,640 bytes in 15
   allocations: the leak accumulates once per cycle.

A diagnostic control that keeps the EGL library loaded produces no leak report;
this is not a correction and has not been adopted by the engine or NXTest. Both
engine exit scenarios have unloaded EGL libraries by the final debugger stop.
The precise reason the delayed engine cases do not report these extra allocations
is not established. Their absence in those reports does not invalidate the
independent reproduction. The exact Mesa/GLVND cleanup implementation responsible
still needs source-level investigation; no matching upstream issue was verified.

The installed versions used are:

```
libdrm2:amd64 2.4.125-1ubuntu0.1~24.04.2
libegl-mesa0:amd64 25.2.8-0ubuntu0.24.04.4
libegl1:amd64 1.7.0-1build1
libsdl2-2.0-0:amd64 2.30.0+dfsg-1ubuntu3.1
```

SDL 2.30.0's [EGL unloading implementation](https://github.com/libsdl-org/SDL/blob/release-2.30.0/src/video/SDL_egl.c#L248)
terminates the display and unloads the libraries; the isolated control exercises
that library lifecycle directly. This establishes the defect outside the engine
and SDL without assuming that it is caused by our missing console-context delete.

Added the actual NXTest `Nexus2D.EngineRuntime.EGLDisplayCleanup`, with fixture
`projects/nxbuild/test/fixtures/engine-memory/egl_display_cleanup.cpp`. It uses
installed EGL headers, the real recipe's C++ compiler/target, and AddressSanitizer.
It checks that the explicit EGL cleanup operations succeed, then fails on the
exit-time leak report. The full rebuilt NXTest suite is **24 passed / 6 failed**:
the original five failures plus this additional reproduction of the same graphics
library defect. No failure was suppressed or marked passing.

### Evidence and proposed scope

Evidence is in `runtime-leak-analysis-1792421062154794143/` under the external worktree, including
`investigation-summary.json`, `results.json`, `cleanup-order-gdb.log`, library
maps, `audio-control/`, `sdl-control/`, `egl-control/` and `nxtests.log`.
Debugger traces stop before LeakSanitizer: they establish call order only and are
not validation passes. The ordinary runs and actual NXTests retain leak checking.

Recommended next implementation batch: the bounded console/window ownership and
unused-audio cleanup corrections. Retain the independent EGL regression and
investigate/report its driver-library cleanup separately. Production source and
installed libraries were not changed in this investigation.


## Approved engine cleanup corrections — 2026-10-19

The owner approved the two engine cleanup fixes and explicitly chose to leave
the installed Mesa/EGL leak alone for now. This supersedes the investigation's
recommendation to pursue that external leak. No installed library, graphics
workaround, sanitizer suppression or expected-failure conversion was added.

### Implementation

- `SolarApp::~SolarApp` selects the main graphics contexts, releases dialogs
  while the runtime is alive, then releases the runtime and console before main
  graphics/SDL cleanup. This preserves the build dialog's `app->Resume()`
  dependency and keeps the console available during runtime destruction.
- `Window::~Window` selects its own GL and ImGui contexts, shuts down both
  backends, deletes its GL context and window, and restores a surviving previous
  context. It never restores the context it has just destroyed.
- `PlatformOpenALPlayer::QuitOpenALPlayer` releases the notifier on its
  uninitialized early-return branch. The initialized-audio path retains its
  existing shutdown and callback order. The only call site is the destructor.

No threading, bootstrap or build-order change is part of these fixes.

### Before/after regression evidence

The existing NXTests already reproduced the unused-audio and console defects.
Two additional real-engine NXTests cover initialized audio and dialog replacement
followed by exit with another dialog open. The dialog fixture retains Lua
listeners and renders text after replacement, exercising surviving contexts and
runtime ownership. The audio fixture is a synthetic 80-frame mono PCM WAV.
Both new fixtures were first run against the saved, unchanged sanitizer engine.

| Scenario | Before: leaked bytes | After: leaked bytes |
| --- | ---: | ---: |
| Font, configured exit, minimal exit, original application | 252 each | 0 each |
| InitializedAudioCleanup | 216 | 0 |
| DialogCleanup | 684 | 0 |
| LocalePreferenceLifetime | 3,180 | 2,928 (accepted external leak) |
| EGLDisplayCleanup (no engine or SDL) | 2,928 | 2,928 (accepted external leak) |

Actual nxbuild builds use the production source files. The full instrumented
build passed in 500.67 seconds. Its rebuilt NXTest runner reports
**30 passed / 2 failed**, with zero unfreed runner blocks. Both failing tests
report exactly **2,928 bytes in three allocations**, from the previously isolated
installed EGL stack. These remain real failures in the raw result; the tests
were not weakened or skipped. The engine-owned leaks are absent.

The normal optimized nxbuild plan/build passed in 395.62 seconds, and all
**seven runtime scenarios passed**, including the two new fixtures.

Debugger traces additionally verify two created/two deleted GL contexts for the
minimal app and four created/four deleted for the dialog case. Each backend shuts
down against its own ImGui context, and all context/window deletes precede
SDL_Quit. Debugger runs stop before LeakSanitizer and are call-order evidence only;
the ordinary NXTests retain complete leak checking.

### Evidence and remaining scope

Evidence under the external worktree:

- `engine-cleanup-fix-1792422634245985635/`: untouched three-file snapshots and hashes,
  `before-new-fixtures/`, rebuilt `nxtests/`, `nxtests.log`, `validation-results.json`,
  `cleanup-order-*.log`, seven `release-*.log` files, `release-results.json`,
  validation scripts and `summary.json`.
- `runtime-heap-investigation-1792422642827040081/`: complete instrumented build, recipe,
  plan/build logs and original/minimal/configured direct runs.
- `engine-cleanup-release-1792423360232720708-evidence.json` and matching plan/build logs:
  normal optimized build command and environment restoration evidence.

Sanitizer executable SHA-256: `380d6e96bba98cad855a2ee250d380f8068f76777fd0103d73bb19feecf86d48`.
Optimized executable SHA-256: `dc48a55aacfa7672c0be580e7e84eeea7e096b472557f92c8f27f48ac1a1b872`.
The actual recipe and compiler environment were restored byte-for-byte after
validation. Native Apple/Windows execution remains unverified. The separate
PIC RTL/plugin work is completed in the follow-up below; the Mesa/EGL leak is
accepted, with no pending repair in this task.


## Standard PIC toolchain and real Pascal plugin — 2026-10-19

The separately approved Linux x86-64 PIC default passed the unchanged three-stage
bootstrap and all 13 shared-library NXTests. The owner approved its promotion to
`/media/kcollins/dev/toolchain/nexus-fpc/x86_64-linux/current`; nxbuild now selects
that compiler and its ordinary PIC RTL. No bootstrap processing was changed.

Using that installation, the final nxbuild self build and all 37 core tests pass.
The actual `nexus-lab/examples/mobile/pascal-plugin/plugin_sample.nxproject` builds
an ELF64 x86-64 shared object with the required `luaopen_plugin_sample` export and
no TEXTREL. The previously validated optimized engine loads that exact library.
The new `Nexus2D.PascalPlugin.ActualEngineInteraction` NXTest passes all 23 Lua
assertions: properties, Pascal methods, events in both directions, listener
removal during dispatch, reset and retained-method lifetime through Lua garbage
collection. The engine timer completes and the process exits with status zero.

The fixture and NXTest driver are under `nexus/projects/nxbuild/test/`; their
recipe is `nxbuildPluginTests.nxproject`. This closes the combined Linux
engine/plugin runtime requirement. It does not change the accepted Mesa leak or
claim native Windows/Apple execution or additional sanitizer coverage. The raw
log retains the minimal fixture's missing optional settings/resource diagnostics.

Evidence is in
`/media/kcollins/dev/worktree/nxproject-final-plugin-zb9y870c/`: `results.json`,
`plugin-runtime-nxtest.log`, `runtime/pascal-plugin-runtime.log`,
`plugin-artifact.log`, `artifact-evidence.json`, and `environment-evidence.json`.
The engine SHA-256 matches the optimized hash above; compiler SHA-256 is
`b6f5b4112caf41bcf4d3515206ebc8422df2fcf4ae44f0e0827ec9517ab30b97`.
The compiler environment was restored byte-for-byte after selecting the external
validation output directory. The unified migration review in
`nexus/work/reviews/nxproject-unified-build-and-dependencies.md` records commands,
toolchain promotion evidence and remaining platform limits.
