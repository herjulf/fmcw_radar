CFLAGS?=-Wall -g
LDFLAGS?=-static

fmcw_radar:	fmcw_radar.o devtag-allinone.o

install:	fmvc_radar
	strip fmcw_radar
	mkdir -p $(DESTDIR)/$(PREFIX)/bin
	cp -p fmcw_radar $(DESTDIR)/$(PREFIX)/bin
clean:
	rm -f *.o fmcw_radar

