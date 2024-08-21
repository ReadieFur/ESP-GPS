# SPDX-FileCopyrightText: © 2022 Uri Shaked <uri@wokwi.com>
# SPDX-License-Identifier: MIT

SOURCES = gps-fake.chip.c
TARGET  = gps-fake.chip.wasm
WOKWI_API = ../wokwi-api.h
JSON_TARGET = gps-fake.chip.json

.PHONY: all
all: $(TARGET) $(JSON_TARGET)

$(TARGET): $(SOURCES) $(WOKWI_API)
	  clang --target=wasm32-unknown-wasi --sysroot /opt/wasi-libc -nostartfiles -Wl,--import-memory -Wl,--export-table -Wl,--no-entry -Werror -o $(TARGET) $(SOURCES)

$(JSON_TARGET): chip.json
	  cp chip.json $(JSON_TARGET)
