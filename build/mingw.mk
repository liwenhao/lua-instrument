
CC = gcc
CFLAGS = -O2 -DLUA_BUILD_AS_DLL -DLUA_LIB -Iexternal/lua/include -Iexternal/visa/include
LDFLAGS = -Lexternal/lua/lib -llua52

lvisa.dll: lvisa.o
	@echo "LN" $@
	@gcc -s -shared -o $@ $< $(LDFLAGS)

lvisa.o: src/lvisa.c
	@echo "CC" $@
	@$(CC) -c $(CFLAGS) $<

clean:
	rm lvisa.o lvisa.dll
