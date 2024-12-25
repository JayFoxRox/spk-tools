// Copyright (C) 2018 Jannik Vogel

#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <limits.h>
#include <inttypes.h>

#include <sys/stat.h>

#include "spk.h"

static uint64_t readLength(FILE* f) {
  uint32_t length;
  fread(&length, 4, 1, f);
  if (length == 0xFFFFFFFF) {
    // Spike 2 only?
    uint64_t length;
    fread(&length, 8, 1, f);
    return length;
  }
  return length;
}

const SpkFolder* spk_get_folder(const SpkFolder* folder, const char* path) {
  char* slash = strchr(path, '/');
  const char* subfolder_path;
  const char* subfolder_name;
  if (slash != NULL) {
    subfolder_path = &slash[1];
    char* tmp = strdup(path);
    slash = slash - path + tmp;
    *slash = '\0';
    subfolder_name = tmp;
  } else {
    subfolder_path = NULL;
    subfolder_name = path;
  }
  for(int i = 0; i < folder->folder_count; i++) {
    SpkFolder* subfolder = &folder->folders[i];
    printf("Looking for '%s' vs. '%s' (remaining: '%s')\n", subfolder_name, subfolder->name, subfolder_path);
    if (!strcmp(subfolder->name, subfolder_name)) {
      if (subfolder_path == NULL) {
        printf("Returning subfolder at %p\n", subfolder);
        return subfolder;
      } else {
        free(subfolder_name);
        return spk_get_folder(subfolder, subfolder_path);
      }
    }
  }

  // This looks wrong, but it's correct.
  // We had a subfolder path, so the first part (name) was a copy to cut it.
  if (subfolder_path != NULL) {
    free(subfolder_name);
  }
  return NULL;
}

const SpkFile* spk_get_file(const SpkFolder* folder, const char* path) {
  char* slash = strrchr(path, '/');
//FIXME: What about a folder and just a filename? = no slashes, but valid
  if (slash == NULL) {
    return NULL;
  }
  char* tmp = strdup(path);
  slash = slash - path + tmp;
  *slash = '\0';
  char* folder_name = tmp;
  char* file_name = &slash[1];
  folder = spk_get_folder(folder, folder_name);
  for(int i = 0; i < folder->file_count; i++) {
    SpkFile* file = &folder->files[i];
    if (!strcmp(file->name, file_name)) {
      free(tmp);
      return file;
    }
  }
  free(tmp);
  return NULL;
}

static void initialize_folder(SpkFolder* folder, const char* name) {
  folder->name = name;
  folder->folder_count = 0;
  folder->folders = NULL;
  folder->file_count = 0;
  folder->files = NULL;
}

static void indexPackage(Spk* spk) {
  spk->packages = realloc(spk->packages, sizeof(SpkPackage) * ++spk->package_count);
  SpkPackage* new_package = &spk->packages[spk->package_count - 1];  
  new_package->root_folder = malloc(sizeof(SpkFolder));
  initialize_folder(new_package->root_folder, "");
}

static void indexFile(SpkPackage* package, FILE* f, off_t strs, off_t sdat, size_t length, mode_t permissions, uint8_t* checksum1, uint8_t* checksum2) {
  char path[PATH_MAX];
  fseek(f, strs, SEEK_SET);
  fread(path, sizeof(path), 1, f);

  char* tmp = path;
  SpkFolder* folder = package->root_folder;

  printf("indexing '%s'\n", tmp);

  while(true) {
    char* slash = strchr(tmp, '/');
    if (slash == NULL) {
      printf("filename '%s'\n", tmp);
      // No more path = filename
      folder->files = realloc(folder->files, sizeof(SpkFile) * ++folder->file_count);
      SpkFile* new_file = &folder->files[folder->file_count - 1];
        //FIXME: Create a new file
      new_file->name = strdup(tmp);
      new_file->size = length;
      new_file->permissions = permissions;
      new_file->sdat_offset = sdat;
      break;
    } else {
      *slash = '\0';
      printf("foldername '%s'\n", tmp);
      SpkFolder* subfolder = spk_get_folder(folder, tmp);
      if (subfolder == NULL) {
        folder->folders = realloc(folder->folders, sizeof(SpkFolder) * ++folder->folder_count);
        folder = &folder->folders[folder->folder_count - 1];
        initialize_folder(folder, strdup(tmp));
      } else {
        folder = subfolder;
      }
      tmp = &slash[1];
    }
  }  
}

static void readChunk(Spk* spk, FILE* f) {

  //FIXME: !!!
SpkPackage* package = &spk->packages[0];
  

  static off_t finf = -1;
  static off_t sdat = -1;
  static off_t strs = -1;

  uint8_t magic[4];
  if (fread(magic, 1, 4, f) != 4) {
    return;
  }

  printf("Found '%.4s' at %lld\n", magic, ftell(f) - 4);
  if (memcmp(magic, "SPKS", 4) == 0) {
    //fseek(f, -4, SEEK_CUR);
    uint64_t length = readLength(f);
    uint32_t unk1;
    fread(&unk1, 4, 1, f);
    readChunk(spk, f);
  } else if (memcmp(magic, "SPK0", 4) == 0) {
    uint64_t length = readLength(f);
    off_t next_offset = ftell(f) + length;
    readChunk(spk, f);
    printf("Jumping to %lld\n", next_offset);
    fseek(f, next_offset, SEEK_SET);
  } else if (memcmp(magic, "SIDX", 4) == 0) {
    uint64_t length = readLength(f);
    struct {
      char name[32];
      uint32_t unk0; // version?
      uint32_t unk1;
      uint32_t unk2; // number of files in package?
      uint32_t unk3;
    } sidx;
    fread(&sidx, sizeof(sidx), 1, f);
    printf("Package name is '%.32s' (%d files?)\n", sidx.name, sidx.unk2);
    readChunk(spk, f);
  } else if (memcmp(magic, "SZ64", 4) == 0) {
    // Spike 2 only?
    struct {
      uint32_t unk0;
      uint32_t unk1;
      uint32_t unk2;
    } sz64;
    fread(&sz64, sizeof(sz64), 1, f);
    readChunk(spk, f);
  } else if (memcmp(magic, "STRS", 4) == 0) {
    uint32_t length;
    fread(&length, 4, 1, f);
    strs = ftell(f);
    fseek(f, length, SEEK_CUR);

#if 0
    char* paths = malloc(length);
    fread(paths, length, 1, f);
    unsigned int i = 0;
    unsigned int o = 0;
    while(o < length) {
      char* path = &paths[o];
      if (path[0] == '\0') {
        break;
      }
      printf("[%d] '%s'\n", i, path);
      o += strlen(path) + 1;
      i++;
    }
#endif
    readChunk(spk, f);
  } else if (memcmp(magic, "FI64", 4) == 0) {

    struct {
      uint32_t unk0; // 0 size of this struct
      uint64_t strs_offset; // 4
      uint64_t length; // 12
      uint64_t sdat_offset; // 20
      uint64_t length2; // 28
      uint32_t permissions; // +36 Read as octal
      uint8_t unk1[1];
      uint8_t checksum[20]; //FIXME: Maybe a couple of bytes earlier?
      uint8_t checksum2[16]; //FIXME: Maybe a couple of bytes later?
      uint8_t unk2[3+4];
    } __attribute__((packed)) fi64;

    // sdat is not set at this point, so read-ahead
    off_t offset = ftell(f);
    fseek(f, offset + sizeof(fi64), SEEK_SET);
    readChunk(spk, f);
    fseek(f, offset, SEEK_SET);

    fread(&fi64, sizeof(fi64), 1, f);
    assert(fi64.unk0 == (sizeof(fi64) - 4));
    assert(fi64.length == fi64.length2);

    indexFile(package, f, strs + fi64.strs_offset, sdat + fi64.sdat_offset, fi64.length, fi64.permissions, fi64.checksum, fi64.checksum2);
    
  } else if (memcmp(magic, "FINF", 4) == 0) {

    struct {
      uint32_t unk0; // size of this struct
      uint32_t strs_offset;
      uint32_t length;
      uint32_t sdat_offset;
      uint32_t length2;
      uint32_t permissions; // Read as octal
      uint8_t unk1[1];
      uint8_t checksum[20]; //FIXME: Maybe a couple of bytes earlier?
      uint8_t checksum2[16]; //FIXME: Maybe a couple of bytes later?
      uint8_t unk2[3];
    } __attribute__((packed)) finf;

    // sdat is not set at this point, so read-ahead
    off_t offset = ftell(f);
    fseek(f, offset + sizeof(finf), SEEK_SET);
    readChunk(spk, f);
    fseek(f, offset, SEEK_SET);

    fread(&finf, sizeof(finf), 1, f);
    assert(finf.unk0 == (sizeof(finf) - 4));
    assert(finf.length == finf.length2);

    indexFile(package, f, strs + finf.strs_offset, sdat + finf.sdat_offset, finf.length, finf.permissions, finf.checksum, finf.checksum2);

  } else if (memcmp(magic, "FEND", 4) == 0) {
    uint8_t unk[4];
    fread(unk, 4, 1, f);
    readChunk(spk, f);
  } else if (memcmp(magic, "SDAT", 4) == 0) {

    uint64_t unk = readLength(f);

    sdat = ftell(f);

    //readChunk(spk, f);
  } else if (memcmp(magic, "SEND", 4) == 0) {
    uint8_t unk[8];
    fread(unk, 8, 1, f);
  } else if (memcmp(magic, "SE64", 4) == 0) {
    uint8_t unk[12];
    fread(unk, 12, 1, f);
  } else {
    printf("Unknown chunk '%.4s'\n", magic);
    assert(false);
  }
}

static void extractSPK(Spk* spk, FILE* f, uint64_t offset) {
  off_t previous_offset = ftell(f);
  fseek(f, offset, SEEK_SET);

  uint8_t magic[4];
  fread(magic, 4, 1, f);
  if (memcmp(magic, "SPKS", 4) == 0) {
    fseek(f, -4, SEEK_CUR);
    while(!feof(f)) {
      readChunk(spk, f);
    }
  }

  fseek(f, previous_offset, SEEK_SET);
}

Spk* spk_parse(FILE* f) {
  uint8_t magic[4];

  Spk* spk = malloc(sizeof(Spk));
  spk->package_count = 0;
  spk->packages = NULL;

  indexPackage(spk);

  // Get SEND or SE64 to find start of data
  //FIXME: Maybe this is only done once
  //       WWE-1-35 seems to not have 2 SEND at the end
  fseek(f, 0, SEEK_END);
  while(ftell(f) >= 16) {
    fseek(f, -16, SEEK_CUR);
    printf("Trying magic at %llu\n", ftell(f));
    fread(magic, 4, 1, f);
    if (memcmp(magic, "SE64", 4) == 0) {
      struct {
        uint32_t unk;
        uint64_t offset;
      } __attribute__((packed)) se64;
      if (fread(&se64, 1, sizeof(se64), f) != sizeof(se64)) {
        printf("Incomplete SE64\n");
        break;
      }
      printf("SPKS at 0x%016" PRIX64 "\n", se64.offset);
      extractSPK(spk, f, se64.offset);
      fseek(f, -16, SEEK_CUR);
    } else {
      fread(magic, 4, 1, f);
      if (memcmp(magic, "SEND", 4) == 0) {
        struct {
          uint32_t unk;
          uint32_t offset;
        } send;
        if (fread(&send, 1, sizeof(send), f) != sizeof(send)) {
          printf("Incomplete SEND\n");
          break;
        }
        printf("SPKS at 0x%08" PRIX32 "\n", send.offset);
        extractSPK(spk, f, send.offset);
        fseek(f, -12, SEEK_CUR);
      } else {
        printf("Could not find another SPK end (SEND / SE64): Done.\n");
        break;
      }
    }
  }

  return spk;
}

void spk_free(Spk* spk) {
  free(spk);
}
