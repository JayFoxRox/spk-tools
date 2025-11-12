# spk-tools to extract Stern Spike updates

This was originally developed in January or February of 2018 for personal use.
I don't think I've ever released this publicly, but by popular demand I'm uploading this now.
This code was privately shared, so copies of it might already exist online.

I've quickly tested to run on some recent game-code updates and the tools still seem to work fine.

In 2025 I've also added pack-spk and added a lot of hacks to the old tools (abusing some C++ to ease development) to also generate additional metadata.
Correctness for Spike 3 was also verified.

## About SPK files

The very first SPK (WWE 1.11) doesn't appear to be SPK, but a tar.gz.
It is assumed that early titles shipped without SPK support.
Early SPKs therefore have a tar.gz header which contains an installer for SPK support.

Later iterations of the SPK format are wrapped in multi-volume squashfs.
If you have files with endings like ".spk.002.000" / ".spk.002.001" you have to extract these first.
`7z x game.spk.002.000` can do it for you. The resulting file is the actual SPK format.

For Spike 3, SPKs are stored in a LUKS2 encrypted ext4 container (which is also using .spk).
This tool can only work with the inner SPK format.


## Building

You need CMake; optionally also FUSE3.
These tools have been designed for Linux but they should also work in WSL or MSYS.

You can build them using this:
```
mkdir build
cd build
cmake ..
make
```


## Tools

### extract-spk

This is the original tool which can be used to extract files.
You should first create a folder for the output, then run the tool from within that folder.

**Example:**

```
mkdir extracted
cd extracted
../extract-spk ~/example.spk
```

### mount-spk

If you have FUSE3, you can also build mount-spk which can be used to mount an SPK file.
This helps to save some space if you have many update files you want to compare / investigate.

**Example:**

```
mkdir mounted
./mount-spk --path=~/example.spk ./mounted
```

Once you are done working with the files you can unmount:

```
umount ./mounted
```

### pack-spk

This is a python3 script that doesn't have to be build.

This tool can create new SPK files, optionally it can also sign them.

**Example:**

```
./pack-spk.py input-folder-with-metadata-json/ output.spk spi_factory_key-1_0_0.key
```

As SPK is full of odd design choices, there might be edge-cases if a section / chunk is somewhere near the 32-bit limits of some fields.
The exact conditions the Stern tools use to switch between generating 32-bit or 64-bit packages is not known.

**WARNING**:
Please avoid distributing your own SPK files, unless you ensure there's no way it could be mistaken for a real Stern SPK file.
There's an ongoing effort to archive official game-updates and unofficial files could lead to confusion.

If you decide to repack a game-update, please modify the package-name / version, or include a header which explains that this isn't an official file.
Note that redistribution of Stern files would violate their copyright.


## Correctness

I have confirmed correctness by regenerating SPK files and confirming these files match exactly.
I did this, by using `mount-spk` on the following official SPK files, then using `pack-spk` to repack into a new SPK file.

In order to generate a valid SPK file, you must have a copy of the factory-key.
See https://github.com/JayFoxRox/stern-spike-dumper

To confirm the correctness as SPK format evolved over time, I've rebuilt the following files successfully.

*(Dates are in ISO 8601 and were sourced from the corresponding update READMEs)*

**Spike 1:**

- 2015-04-17 WWE-1_17.spk
- 2016-05-17 WWE-1_35.spk
- 2016-05-17 WWE_LE-1_35.spk
- 2018-12-11 KISS_LE-1_41_0.spk
- 2019-01-25 WN-1_55_0.spk
- 2019-07-29 GOT_LE-1_37_0.spk
- 2019-10-08 Ghostbusters_le-1_17_0.spk
- 2019-10-08 Ghostbusters_pro-1_17_0.spk
- 2020-12-22 can_crusher-1_02_0.spk
- 2020-12-22 primus-1_04_0.spk
- 2021-03-26 heavy_metal-1_02_0.spk

**Spike 2:**

- 2017-01-11 BAT-0_65.spk
- 2017-03-22 batman-0_70_0.spk
- 2017-03-28 aerosmith_le-1_03_0.spk
- 2017-04-24 batman-0_71_0.spk
- 2017-05-25 batman-0_75_0.spk
- 2017-07-11 star_wars_pro-0_84_0.spk
- 2017-08-14 star_wars_le-0_89_0.spk
- 2017-10-06 star_wars_le-0_92_0.spk
- 2017-10-18 star_wars_le-0_93_0.spk
- 2017-10-27 batman-0_80_0.spk
- 2017-11-28 guardians_le-0_72_0.spk
- 2017-12-11 star_wars_le-1_00_0.spk
- 2017-12-11 star_wars_pro-1_00_0.spk
- 2018-01-09 guardians_le-0_85_0.spk
- 2018-01-30 batman-0_87_0.spk
- 2018-02-13 guardians-0_87_0.spk
- 2018-08-30 deadpool_pro-0_82_0.spk
- 2019-01-07 munsters_pro-0_90_0.spk
- 2019-01-09 iron_maiden_pro-1_06_0.spk
- 2019-01-28 munsters_le-0_91_0.spk
- 2019-04-11 sword_of_rage_pro-0_91_0.spk
- 2019-05-29 sword_of_rage_le-0_94_0.spk
- 2019-08-21 jurassic_park_pro-0_87_0.spk
- 2019-10-07 elvira3-0_83_0.spk
- 2019-10-09 jurassic_park_le-0_90_0.spk
- 2020-08-03 star_wars_elg-1_04_0.spk
- 2021-03-08 elvira3-1_02_0.spk
- 2021-11-18 avengers_infinity_le-1_03_0.spk
- 2021-11-18 iron_maiden_le-1_10_0.spk
- 2021-11-22 deadpool_le-1_06_0.spk
- 2021-12-15 guardians_le-1_09_0.spk
- 2021-12-16 aerosmith-1_10_0.spk
- 2021-12-16 sword_of_rage_le-1_12_0.spk
- 2021-12-19 stranger_things-1_05_0.spk
- 2022-02-03 munsters_le-1_21_0.spk
- 2022-02-09 jurassic_park_the_pin-1_03_0.spk
- 2022-02-22 star_wars_le-1_20_0.spk
- 2022-03-01 mando_le-1_20_0.spk
- 2022-03-02 led_zeppelin_le-1_14_0.spk
- 2022-03-28 turtles_le-1_51_0.spk
- 2022-06-01 rush_le-0_97_0.spk
- 2022-06-23 godzilla_le-0_97_0.spk
- 2024-11-14 foo_fighters_le-1_03_0.spk
- 2024-11-26 beatles-1_27_0.spk
- 2024-11-26 guardians_le-1_14_0.spk
- 2024-11-26 james_bond_60th_le-1_09_0.spk
- 2024-11-26 munsters_le-1_27_0.spk
- 2024-11-26 rush_le-1_18_0.spk
- 2025-07-22 jaws_le-1_01_0.spk
- 2025-07-22 star_wars_le-1_29_0.spk
- 2025-09-30 dungeons_and_dragons_pro-0_97_0.spk
- 2025-10-10 deadpool_le-1_14_0.spk
- 2025-10-10 godzilla_le-1_13_0.spk

**Spike 3:**

- 2025-10-30 star_wars_2025_pro-0_86_0.spk


## Alternatives

There's a similar tool at https://github.com/bdash/spike-spk.
A benefit of it is that it can verify the correctness of files.

Additionally, each Spike game contains the official Stern tool to work with these files in `/usr/local/bin/spk`.
Depending on your environment, you can run these tools using `qemu-arm` / `qemu-aarch64`.
However, the Stern tool will not repack files and some fields remain hidden.