# Required Prince of Persia game data

Obtain a complete English VGA DOS data set from Prince of Persia **1.0, 1.1, 1.3 or 1.4** yourself. Original game data are not included in this project. Use files from one release; do not mix versions.

Copy the following 21 files directly into this `PrinceFiles` directory, alongside this README. Keep the uppercase filenames and do not add another subdirectory:

```text
PRINCE.DAT
KID.DAT
VDUNGEON.DAT
VPALACE.DAT
GUARD.DAT
GUARD1.DAT
GUARD2.DAT
FAT.DAT
SKEL.DAT
VIZIER.DAT
SHADOW.DAT
PV.DAT
TITLE.DAT
LEVELS.DAT
MIDISND1.DAT
MIDISND2.DAT
DIGISND1.DAT
DIGISND2.DAT
DIGISND3.DAT
IBM_SND1.DAT
IBM_SND2.DAT
```

The DOS executable is not required. Additional EGA/CGA/MT-32 DAT files are not embedded by this port.

After copying the files, configure and build the project as described in the [main README](../README.md). CMake generates the asset headers from these files automatically. No data is downloaded during the build. Missing or invalid required files stop the build.

You may use another directory by setting `-DPRINCE_FILES=/absolute/path/to/DATs` when configuring CMake. These files are build inputs, not runtime SD-card files: the compiled firmware contains the game assets.

Only this README is tracked by Git in this directory. Game data and other local contents remain ignored. Original game assets retain their original ownership; the source-code licenses do not grant rights to those assets.
