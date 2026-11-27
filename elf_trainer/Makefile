PS5_PAYLOAD_SDK ?= /opt/ps5-payload-sdk

include $(PS5_PAYLOAD_SDK)/toolchain/prospero.mk

ELF := GTA6WebTrainer.elf
CFLAGS := -O2 -Wall -Wno-unused-function -pthread
LDFLAGS := -pthread

all: $(ELF)

$(ELF): main.c web_assets.h vehicles_data.h
	$(CC) $(CFLAGS) -o $@ main.c $(LDFLAGS)

clean:
	rm -f $(ELF)
