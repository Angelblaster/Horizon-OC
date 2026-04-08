#!/bin/sh

SRC="Source/Atmosphere/stratosphere/loader/source/oc"
DEST="build/stratosphere/loader/source/oc"
mkdir -p "dist/atmosphere/kips/"
mkdir -p "$DEST"

cp -r "$SRC"/. "$DEST"/

cd build/stratosphere/loader || exit 1
make -j"$(nproc)"
hactool -t kip1 out/nintendo_nx_arm64_armv8a/release/loader.kip --uncompress=hoc.kip
cd ../../../ # exit
cp build/stratosphere/loader/hoc.kip dist/atmosphere/kips/hoc.kip

cd Source/sys-clk/
./build.sh
cp -r dist/ ../../

cd ../../

cd Source/Horizon-OC-Monitor/
make -j"$(nproc)"
cp Horizon-OC-Monitor.ovl ../../dist/switch/.overlays/Horizon-OC-Monitor.ovl

cd ../../

ROOT="build"
PATCHES="Atmosphere-Patches"

cp "$PATCHES/secmon_memory_layout.hpp" "$ROOT/libraries/libexosphere/include/exosphere/secmon/"
cp "$PATCHES/secmon_emc_access_table_data.inc" "$ROOT/exosphere/program/source/smc/"
cp "$PATCHES/secmon_define_emc_access_table.inc" "$ROOT/exosphere/program/source/smc/"
cp "$PATCHES/secmon_smc_register_access.cpp" "$ROOT/exosphere/program/source/smc/"
cd build/exosphere

make -j"$(nproc)"

cd out/nintendo_nx_arm64_armv8a/release
cp "exosphere.bin" "../../../../../dist/"
