PS5_PAYLOAD_SDK ?= /opt/ps5-payload-sdk

include $(PS5_PAYLOAD_SDK)/toolchain/prospero.mk

ELF := eboot.bin
CFLAGS := -O2 -Wall
LDFLAGS := -lSceSystemService

all: $(ELF)

$(ELF): launcher.c
	$(CC) $(CFLAGS) -o $@ launcher.c $(LDFLAGS)

clean:
	rm -f $(ELF)
