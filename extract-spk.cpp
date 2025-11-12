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

static void extractFile(char* path, File* file) {
  //FIXME
  printf("Visiting '%s%s'\n", path, file->name);

  char out_path_buffer[PATH_MAX];
  sprintf(out_path_buffer, "%s%s", path, file->name);
  mkdir_p(out_path_buffer);
  FILE* out = fopen(out_path_buffer, "wb");
  size_t chunk_size = 2 * 1024 * 1024;
  uint8_t* chunk = (uint8_t*)malloc(chunk_size);
  size_t size = file->size;
  off_t offset = 0;
  while (size > 0) {   
    if (size < chunk_size) {
      chunk_size = size;
    }
    file->read(chunk, offset, chunk_size);
    fwrite(chunk, 1, chunk_size, out);
    size -= chunk_size;
    offset += chunk_size;
  }
  free(chunk);
  fclose(out);
}

static void extractFolder(const char* path, Folder* folder) {
  char* subpath = (char*)malloc(strlen(path) + strlen(folder->name) + 1 + 1);
  strcpy(subpath, path);
  strcat(subpath, folder->name);
  strcat(subpath, "/");
  printf("Visiting '%s'\n", subpath);
  for(int i = 0; i < folder->folder_count; i++) {
    extractFolder(subpath, folder->folders[i]);
  }
  for(int i = 0; i < folder->file_count; i++) {
    extractFile(subpath, folder->files[i]);
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

  Folder* root_folder = splitSpkIntoFoldersFromFILE(spk, f);
  extractFolder("./", root_folder);
  freeFolders(root_folder);

  spk_free(spk);


  return 0;
}