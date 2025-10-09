.POSIX:
.SUFFIXES:
.PHONY: clean run debug release

CC         = c99
CFLAGS     = -std=c99
WARNINGS   = -Wall -Wextra -pedantic -Wno-unused-parameter
LDLIBS     = -lole32 -lcomctl32 -lgdi32 -lshlwapi -ldwmapi -lpathcch -lversion -lwinmm -lshcore

ifdef RELEASE
CFLAGS    += -Oz -DNDEBUG=1 -mwindows
LDFLAGS   += -s
else
CFLAGS    += $(WARNINGS) -ggdb3 -Og -D_DEBUG=1 -DDEBUG=1
endif

cmdtab.exe: src/cmdtab.c res/cmdtab.o
	$(CC) $(CFLAGS) $(LDFLAGS) $(INCLUDE) -o $@ $^ $(LDLIBS)

%.o: %.rc
	windres -I res/ -o $@ $^

clean:
	rm cmdtab.exe res/cmdtab.o || true

run: cmdtab.exe
	pkill cmdtab.exe || true
	gdb --batch --ex=run --args cmdtab.exe

debug: cmdtab.exe
	pkill cmdtab.exe || true
	gdb --ex=start --args cmdtab.exe --autorun

release: clean
	make RELEASE=1
	zip -9 cmdtab-v0.0.0-win-x86_64.zip cmdtab.exe
	@printf '%s\n' $$'\x1b[31mREMEMBER TO UPDATE cmdtab.rc & README.md\x1b[0m'

start:
	schtasks /run /tn "cmdtab elevated autorun"
