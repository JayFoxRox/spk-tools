// Copyright (C) 2018 Jannik Vogel

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

typedef struct SpkFile_ {
  char* name;
  uint64_t size; // 12
  uint64_t sdat_offset; // 20
  uint64_t length2; // 28
  uint32_t permissions; // +36 Read as octal
  uint8_t unk1[1];
  uint8_t checksum[20]; //FIXME: Maybe a couple of bytes earlier?
  uint8_t checksum2[16]; //FIXME: Maybe a couple of bytes later?
  uint8_t unk2[3+4];
} SpkFile;

typedef struct SpkFolder_ {
  char* name;
  unsigned int folder_count;
  struct SpkFolder_* folders;
  unsigned int file_count;
  struct SpkFile_* files;  
} SpkFolder;

typedef struct SpkPackage_ {
  char* name;
  uint32_t unk0; // version?
  uint32_t unk1;
  uint32_t unk2; // number of files in package?
  uint32_t unk3;
  struct SpkFolder_* root_folder;
} SpkPackage;

typedef struct Spk_ {
  unsigned int package_count;
  struct SpkPackage_* packages;
} Spk;

Spk* spk_parse(FILE* f);
void spk_free(Spk* spk);
const SpkFolder* spk_get_folder(const SpkFolder* folder, const char* path);
const SpkFile* spk_get_file(const SpkFolder* folder, const char* path);
