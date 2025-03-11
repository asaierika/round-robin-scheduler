EXE=allocate
LDLIBS = -lm

$(EXE): main.c
	cc -Wall -o $(EXE) $< $(LDLIBS)

format:
	clang-format -style=file -i *.c

clean: 
	rm -f allocate
