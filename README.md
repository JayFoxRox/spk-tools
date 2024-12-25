# spk-tools to extract Stern Spike updates

This was originally developed in January or February of 2018 for personal use.
I don't think I've never released this publicly, but by popular demand I'm uploading this now.
This code was privately shared, so copies of it might already exist online.

I've quickly tested to run on some recent game-code updates and the tools still seem to work fine.


## Building & Running

You need CMake; optionally also FUSE3.
These tools have been designed for Linux but they should also work in WSL or MSYS.


### extract-spk

This is the original tool which can be used to extract files.
You should first create a folder for the output, then run the tool from within that folder.

**Example:**

```
mkdir build
cd build
cmake ..
make
mkdir extracted
cd extracted
../extract-spk ~/example.spk
```

### mount-spk

If you have FUSE3, you can also build mount-spk which can be used to mount an SPK file.
This helps to save some space if you have many update files you want to compare / investigate.

**Example:**

```
mkdir build
cd build
cmake ..
make
mkdir mounted
./mount-spk --path=~/example.spk ./mounted
```

Once you are done working with the files you can unmount:

```
umount ./mounted
```
