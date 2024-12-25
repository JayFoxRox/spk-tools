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

static void mkdir_p(const char *dir) {
  char tmp[PATH_MAX];
  char *p = NULL;
  size_t len;

  snprintf(tmp, sizeof(tmp), "%s",dir);
  len = strlen(tmp);
  for(p = tmp + 1; *p; p++) {
    if(*p == '/') {
      *p = 0;
      printf("Making '%s'\n", tmp);
      mkdir(tmp, S_IRWXU);
      *p = '/';
    }
  }
}

#if 0
void dumpFile(FILE* f, off_t strs, off_t sdat, size_t length, mode_t permissions, uint8_t* checksum1, uint8_t* checksum2) {
  char path_buffer[PATH_MAX];
  fseek(f, strs, SEEK_SET);
  fread(path_buffer, sizeof(path_buffer), 1, f);

  printf("path '%s' (%lld bytes) permissions %o; ", path_buffer, length, permissions);

  printf("CK1: ");
  for(unsigned int i = 0; i < 20; i++) {
    printf("%02X", checksum1[i]);
  }
  printf("; CK2: ");
  for(unsigned int i = 0; i < 16; i++) {
    printf("%02X", checksum2[i]);
  }
  printf("\n");
}
#endif

static void extractFile(FILE* f, char* path, SpkFile* file) {
  //FIXME
  printf("Visiting '%s%s'\n", path, file->name);

  fseek(f, file->sdat_offset, SEEK_SET);

  char out_path_buffer[PATH_MAX];
  sprintf(out_path_buffer, "%s%s", path, file->name);
  mkdir_p(out_path_buffer);
  FILE* out = fopen(out_path_buffer, "wb");
  size_t chunk_size = 4 * 1024 * 1024;
  uint8_t* chunk = malloc(chunk_size);
  size_t length = file->size;
  while (length > 0) {   
    if (length < chunk_size) {
      chunk_size = length;
    }   
    fread(chunk, 1, chunk_size, f);
    fwrite(chunk, 1, chunk_size, out);
    length -= chunk_size;
  }
  free(chunk);
  fclose(out);
}

static void extractFolder(FILE* f, char* path, SpkFolder* folder) {
  char* subpath = malloc(strlen(path) + strlen(folder->name) + 1 + 1);
  strcpy(subpath, path);
  strcat(subpath, folder->name);
  strcat(subpath, "/");
  printf("Visiting '%s'\n", subpath);
  for(int i = 0; i < folder->folder_count; i++) {
    extractFolder(f, subpath, &folder->folders[i]);
  }
  for(int i = 0; i < folder->file_count; i++) {
    extractFile(f, subpath, &folder->files[i]);
  }
  free(subpath);
}

int main(int argc, char* argv[]) {

  if (argc != 2) {
    printf("Please provide an spk-path using `%s example.spk`\n", argv[0]);
    return 1;
  }
  char* path = argv[1];

  uint8_t magic[4];
  FILE* f = fopen(path, "rb");
  if (f == NULL) {
    printf("Unable to open '%s'\n", path);
    return 1;
  }

  Spk* spk = spk_parse(f);
  if (spk == NULL) {
    printf("Unable to parse SPK\n");
    return 1;
  }

  printf("Note that this tool only extracts the spike packages.\n"
         "Older SPK files also include a tar.gz at the file start.\n"
         "Use the UNIX `gunzip` & `tar` utility to extract those.\n");

  for(int i = 0; i < spk->package_count; i++) {
    SpkPackage* package = &spk->packages[i];
    extractFolder(f, "./", package->root_folder);
  }

  SpkFolder* folder = spk->packages[0].root_folder;
  for(int i = 0; i < folder->folder_count; i++) {
    printf("folder in root is '%s'\n", folder->folders[i].name);
  }

  for(int i = 0; i < folder->file_count; i++) {
    printf("file in root is '%s'\n", folder->files[i].name);
  }


  spk_free(spk);
  return 0;
}
