
Download built openssl:
`http://www.npcglib.org/~stathis/downloads/openssl-1.0.1u-vs2013.7z`
Rename `lib/*MT.lib` to `lib/*.lib`

Download libevent sources from https://github.com/libevent/libevent
or release `https://codeload.github.com/libevent/libevent/tar.gz/release-2.1.6-beta`

Add
```
#define EVENT__HAVE_OPENSSL 1
```
to `event-config.h`



start `VS2013 x86 Native Tools Command Prompt`
navigate to libevent, run

`nmake OPENSSL_DIR="C:\Users\vlad\Downloads\openssl-1.0.1u-vs2013" -f Makefile.nmake`

