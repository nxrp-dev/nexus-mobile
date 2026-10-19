## How To Build For Native Linux

If you have not done it already you will need to clone the git repository

```
cd ~
git clone https://github.com/nxrp-dev/nexus-mobile.git
```

## Native library prerequisites

The native Linux engine needs the following development packages. Runtime
libraries alone do not supply the headers and linker files needed to compile it.
These are engine API dependencies, independent of the build orchestrator.

| Engine API | Debian/Ubuntu/Linux Mint development package |
| --- | --- |
| SDL2 windowing, input and platform integration | `libsdl2-dev` |
| OpenGL rendering | `libgl-dev` |
| OpenAL audio | `libopenal-dev` |
| FreeType font rendering | `libfreetype-dev` |
| PNG decoding | `libpng-dev` |
| JPEG decoding | `libjpeg-dev` |
| HTTP/network requests through curl | `libcurl4-openssl-dev` |
| OpenSSL cryptography | `libssl-dev` |
| zlib compression | `zlib1g-dev` |

Install these packages and their package-manager dependencies with:

```sh
sudo apt-get install --no-install-recommends \
    libsdl2-dev libgl-dev libopenal-dev libfreetype-dev libpng-dev \
    libjpeg-dev libcurl4-openssl-dev libssl-dev zlib1g-dev
```

Installation requires an administrator account. It is a machine prerequisite,
not an nxbuild task; nxbuild must not install system packages during a build.
The old `setup_dev.sh` also provisions unrelated development tools and is not
needed to supply this library set.


## Direct nxbuild recipe

The Nexus recipe is `Nexus2DEngine.nxproject` at the repository root. From the
sibling Nexus repository, using its configured compiler environment:

```sh
"$NXBUILD" /action=plan /project=../nexus-mobile/Nexus2DEngine.nxproject /targetos=linux /targetcpu=x86_64
"$NXBUILD" /action=build /project=../nexus-mobile/Nexus2DEngine.nxproject /targetos=linux /targetcpu=x86_64
```

This invokes Clang/Clang++ with LLD and bundled Lua tools directly through
nxbuild. It generates embedded Lua inputs and builds the Linux simulator in
`Tools.OutputRoot/nexus2d/x86_64-linux`. Configure Tools.OutputRoot under the
external worktree. The recipe does not install the simulator or system packages.
The historical upstream instructions below describe a separate build path.

Validation status on 2026-10-19: the actual nxbuild engine build passes with
153 Lua embeddings and 594 explicitly ordered object inputs. All 27 release-engine
behavior checks pass, including the original failing shutdown case, locale
preferences and populated-font shutdown. Two specific NXTests then proved an
IndexArray malloc/delete mismatch; its separately approved one-line correction is
applied and both allocation regressions pass. The approved console/window and
unused-audio notifier cleanup fixes are also validated. The final optimized
rebuild and all seven runtime scenarios pass. The instrumented NXTest suite is
30 passed / 2 failed: both remaining failures report only the installed Mesa/EGL
leak accepted by the owner for now. It remains unchanged and visible in the tests.
The standard Linux x86-64 toolchain now defaults to PIC and supplies the matching
RTL. The actual Pascal sample plugin builds, loads in the real engine and passes
a new NXTest with 23 interaction assertions. Native Apple/Windows validation
remains unverified. Before/after evidence is in [NXBUILD_REVIEW.md](NXBUILD_REVIEW.md).

## Historical upstream development instructions

For those who like IDE's the source has been setup for use with CodeLite it can be installed using.
```
sudo apt-get install codelite -y
```

If you have installed CodeLite the workspace file is named "Solar2D.workspace" and is located at ~/nexus-mobile/platform/linux



To build everything run. This will build linux simulator and template and install Solar2D in /usr/local/bin

```
mkdir build
cd build
cmake ..
make -j8
sudo make install
```

You would also need Raspbian binaries to build cross-compiled template for Raspbian Pi. You can download them [here](https://drive.google.com/file/d/1ZysxJdDg-XgU3-jshxUPSewTqBYeA7Qq/view?usp=sharing).

