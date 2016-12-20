

XFLAGS    = -Wall -Wshadow -Wstrict-prototypes -Wmissing-prototypes \
            -DDEBUG -DVERBOSE_DEBUG -Wredundant-decls

INCDIRS   = -I. -I/home/pi/work/tinydrm/usr/include -I/home/pi/work/tinydrm/udrm-kernel/include/uapi

LDFLAGS   = -Wl,--no-as-needed -lrt


CC=gcc
CFLAGS    = ${INCDIRS} ${OFLAGS} ${XFLAGS} ${PFLAGS} ${UFLAGS}
DEPS = base.h device.h gpio.h spi.h backlight.h dmabuf.h regmap.h udrm.h mipi-dbi.h mipi-dbi-spi.h ili9341.h
OBJ =  log.o  device.o gpio.o spi.o backlight.o dmabuf.o regmap.o udrm.o mipi-dbi.o mipi-dbi-spi.o mi0283qt.o

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

mi0283qt: $(OBJ)
	$(CC) -o $@ $^ $(CFLAGS)

.PHONY: clean

clean:
	rm -f *.o
