all: proxy

proxy: proxy.c
	gcc -o proxy proxy.c

clean:
	rm -f proxy
