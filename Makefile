CC=gcc
#CFLAGS=-I. -std=c11 -Wall -Wextra -Werror -Wstrict-aliasing -pedantic -fmax-errors=5 \
#	-Wunreachable-code -Wcast-align -Wcast-qual -Wdisabled-optimization -Wformat=2 \
#	-Winit-self -Wlogical-op -Wmissing-include-dirs -Wredundant-decls -Wshadow \
#	-Wstrict-overflow=2 -Wswitch-default -Wundef -Wno-unused -Wno-variadic-macros \
#	-Wno-parentheses -fdiagnostics-show-option -O2
CFLAGS=-I. -std=c11 -Wall -Wextra -Werror -Wstrict-aliasing -fmax-errors=5 \
	-Wunreachable-code -Wcast-align -Wcast-qual -Wdisabled-optimization -Wformat=2 \
	-Winit-self -Wlogical-op -Wmissing-include-dirs -Wredundant-decls -Wshadow \
	-Wstrict-overflow=2 -Wswitch-default -Wundef -Wno-unused \
	-Wno-parentheses -fdiagnostics-show-option -g
LIBDIR = /usr/local/lib
LDFLAGS=-lrt -lpthread -L  ${LIBDIR}  -lMsbClientC -ljson-c -luuid
SEROBJS = crc.o emsSerio.o queue.o rx.o serial.o tx.o configure.o parser/parser.a
DECODEOBJS = emsDecode.o configure.o parser/parser.a
CMDOBJS = emsCommand.o configure.o parser/parser.a
MONOBJS = emsMonitor.o itoa.o
MQTTOBJS = emsMqtt.o configure.o mqtt.o parser/parser.a
MSBOBJS = emsMsb.o configure.o msb.o parser/parser.a
SYSTEMDFILES = ems.system
SVNDEV := -D'SVN_REV="$(shell svnversion -n .)"'
CFLAGS+= $(SVNDEV)
CFLAGS+=-I /usr/local/include
BINDIR = /usr/local/bin
CONFDIR = /usr/local/etc
DEPS=ems.h

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

all:	emsSerio emsDecode emsMqtt emsMonitor emsCommand emsMsb emsMonitor


emsSerio: $(SEROBJS)
	$(CC) -o $@ $^ $(CFLAGS) $(LDFLAGS)

emsDecode: $(DECODEOBJS)
	$(CC) -o $@ $^ $(CFLAGS) $(LDFLAGS)

emsMqtt: $(MQTTOBJS)
	$(CC) -o $@ $^ $(CFLAGS) $(LDFLAGS) -lmosquitto

emsMsb: $(MSBOBJS)
	$(CC) -o $@ $^ $(CFLAGS) $(LDFLAGS) -lmosquitto

emsMonitor: $(MONOBJS)
	$(CC) -o $@ $^ $(CFLAGS) $(LDFLAGS)

emsCommand: $(CMDOBJS)
	$(CC) -o $@ $^ $(CFLAGS) $(LDFLAGS)

testmsb: testmsb.c
	$(CC) -o $@ $^ $(CFLAGS) $(LDFLAGS)

clean:
	rm *.o emsSerio emsDecode emsMqtt emsMonitor emsCommand emsMsb

tags:
	etags -l c -o TAGS *.c *.h

parser/parser.a:  parser/file.o parser/transform.o parser/parse.o parser/modify.o
	cd parser && make parser.a

install: emsSerio emsDecode emsMqtt emsCommand emsMsb emsMonitor
	install $? $(BINDIR)
	chmod +s $(addprefix $(BINDIR)/,$?)

install-config: ems.cfg
	install $? $(CONFDIR)

install-tools: emsMonitor
	install emsMonitor $(BINDIR)
	chmod 755 $(BINDIR)/*

install-systemd: $(SYSTEMDFILES)
	cp -i $(SYSTEMDFILES) /etc/systemd/system/
	systemctl daemon-reload

install-all:
	make install
	make install-config
	make install-tools
	make install-systemd
