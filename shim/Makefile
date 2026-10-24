CFLAGS ?= -O2 -Wall -Wextra -fPIC
libnvcodec-shim.so: nvcodec-shim.c
	$(CC) $(CFLAGS) -shared -o $@ $< -ldl
clean:
	rm -f libnvcodec-shim.so
