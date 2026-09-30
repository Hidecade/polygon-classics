#!/usr/bin/env bash
set -euo pipefail
cd /work/source/mame
embuilder build sdl3 sdl3_ttf
emmake make -j"${JOBS:-4}" SUBTARGET=starblade SOURCES=src/mame/namco/namcos21_c67.cpp,src/mame/sega/model1.cpp,src/mame/namco/namcos22.cpp,src/mame/sega/model2.cpp REGENIE=1 NOWERROR=1 USE_QTDEBUG=0 NO_USE_MIDI=1 NO_USE_PORTAUDIO=1 NO_USE_PULSEAUDIO=1 NO_USE_PIPEWIRE=1 PYTHON_EXECUTABLE=python3 LDFLAGS=-g2
python3 /work/source/pack-core.py
