ifeq ($(OS),Windows_NT)
EXE=extension.exe
EXT=sqlite3_mod_regexp.dll
RM=cmd /c del
LDFLAG=
else
EXE=extension
EXT=sqlite3_mod_regexp.so
RM=rm
LDFLAG=-fPIC
endif

SQLITE_DIR=../../internal/sqlite-amalgamation-3081101

all : $(EXE) $(EXT)

$(EXE) : extension.go
	go build $<

$(EXT) : sqlite3_mod_regexp.c $(SQLITE_DIR)/sqlite3.c
	gcc -I$(SQLITE_DIR) $(LDFLAG) -shared -o $@ $<

clean :
	@-$(RM) $(EXE) $(EXT)
