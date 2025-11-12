// Copyright (C) 2018 Jannik Vogel

#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <functional>
#include <memory>

#define MAX_PATH 2048

typedef struct SpkFile_ {
  uint64_t strs_offset;
  uint64_t size; // 12
  uint64_t sdat_offset; // 20
  uint64_t length2; // 28
  uint32_t permissions; // +36 Read as octal
  uint8_t unk1[1];
  uint8_t checksum[20]; //FIXME: Maybe a couple of bytes earlier?
  uint8_t checksum2[16]; //FIXME: Maybe a couple of bytes later?
  uint8_t unk2[3+4];
} SpkFile;

typedef struct SpkPackage_ {
  char* name;
  char shortname[4];
  struct {
    uint8_t major;
    uint8_t minor;
    uint8_t patch;
  } version;
  uint8_t type;
  uint32_t unk1;
  uint32_t unk2; // number of files in package?
  uint32_t unk3;

  // Custom data for easier parsing
  off_t strs; //FIXME: Add size
  off_t sdat; //FIXME: Add size
  unsigned int file_count;
  struct SpkFile_* files;

} SpkPackage;

typedef struct Spk_ {
  unsigned int package_count;
  struct SpkPackage_* packages;

  // Custom data
  off_t offset;
} Spk;

Spk* spk_parse(FILE* f);
void spk_free(Spk* spk);

using SpkReadCb = std::function<void(const SpkPackage* package, const SpkFile* file, void* data, off_t offset, size_t length)>;
using FileReadCb = std::function<void(void* data, off_t offset, size_t length)>;

struct File {
  char* name;
  uint16_t permissions;
  FileReadCb read;
  size_t size;
  virtual ~File() = default;
};

typedef struct Folder_ {
  char* name;
  unsigned int folder_count;
  struct Folder_** folders;
  unsigned int file_count;
  File** files;
} Folder;

Folder* splitPackageIntoFolders(const SpkPackage* package, SpkReadCb strsRead, SpkReadCb sdatRead);
Folder* splitSpkIntoFolders(const Spk* spk, FileReadCb rawRead, SpkReadCb strsRead, SpkReadCb sdatRead);
void freeFolders(Folder* root_folder);

Folder* splitSpkIntoFoldersFromFILE(const Spk* spk, FILE* f);