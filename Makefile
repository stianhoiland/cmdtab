.POSIX:
.SUFFIXES:
.PHONY: clean run debug release zip install start stop

CC         = c99
CFLAGS     = -std=c99
WARNINGS   = -Wall -Wextra -pedantic -Wno-unused-parameter
LDLIBS     = -lole32 -lcomctl32 -lgdi32 -lshlwapi -ldwmapi -lpathcch -lversion -lwinmm -lshcore

GETVERSION = $$(sed -E -e 's,^[[:space:]]*VALUE "FileVersion"\,[[:space:]]*"(.*)"$$,\1,p;d' cmdtab.rc)

PREFIX     = "C:/Users/$$USER/Downloads/cmdtab-v"$(GETVERSION)"-win-x86_64"

ifdef RELEASE
CFLAGS    += -Os -DNDEBUG=1 -mwindows
LDFLAGS   += -s
else
CFLAGS    += $(WARNINGS) -ggdb3 -Og -D_DEBUG=1 -DDEBUG=1
endif

cmdtab.exe: cmdtab.c cmdtab.o
	$(CC) $(CFLAGS) $(LDFLAGS) $(INCLUDE) -o $@ $^ $(LDLIBS)

cmdtab.o: cmdtab.rc
	windres $^ $@

clean:
	rm cmdtab.exe cmdtab.o || true

run: cmdtab.exe
	pkill cmdtab.exe || true
	gdb --batch --ex=run --args cmdtab.exe

debug: cmdtab.exe
	pkill cmdtab.exe || true
	gdb --ex=start --args cmdtab.exe --autorun

release: clean
	make RELEASE=1
	@printf '%s\n' $$'\x1b[31mREMEMBER TO UPDATE cmdtab.rc & README.md\x1b[0m'

zip:
	rm cmdtab-v"$(GETVERSION)"-win-x86_64.zip || true
	zip -9 cmdtab-v"$(GETVERSION)"-win-x86_64.zip cmdtab.exe

install: release stop
	PREFIX=$(PREFIX) && \
	 mkdir -p "$$PREFIX" && \
	 cp cmdtab.exe "$$PREFIX/cmdtab.exe" && \
	 schtasks /create /sc onlogon /rl highest /tn "cmdtab elevated autorun" /tr "$$PREFIX/cmdtab.exe --autorun"

start: stop
	schtasks /run /tn "cmdtab elevated autorun"

stop:
	schtasks /end /tn "cmdtab elevated autorun"
