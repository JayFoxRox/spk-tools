// Copyright (C) 2018 Jannik Vogel

#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <cassert>
#include <climits>
#include <cinttypes>
#include <string>

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

static SpkPackage* indexPackage(Spk* spk) {
  spk->packages = (SpkPackage*)realloc(spk->packages, sizeof(SpkPackage) * ++spk->package_count);
  SpkPackage* new_package = &spk->packages[spk->package_count - 1];  
  new_package->file_count = 0;
  new_package->files = NULL;
  return new_package;
}

static void indexFile(SpkPackage* package, FILE* f, off_t strs, off_t sdat, size_t length, mode_t permissions, uint8_t* checksum1, uint8_t* checksum2) {
  package->files = (SpkFile*)realloc(package->files, sizeof(SpkFile) * ++package->file_count);
  SpkFile* new_file = &package->files[package->file_count - 1];
    //FIXME: Create a new file
  new_file->strs_offset = strs;
  new_file->size = length;
  new_file->permissions = permissions;
  new_file->sdat_offset = sdat;
}


static void readSidx(Spk* spk, FILE* f) {
  uint8_t magic[4];

  fread(magic, 4, 1, f);
  assert(memcmp(magic, "SIDX", 4) == 0);
  uint64_t length = readLength(f);
  off_t next_offset = ftell(f) + length;
  struct {
    char name[32];
    uint32_t unk0; // version?
    uint32_t unk1;
    uint32_t unk2; // number of files in package?
    uint32_t sdatSize; // sdat size, or 0xFFFFFFFF if SZ64 follows?
  } __attribute__((packed)) sidx;
  fread(&sidx, sizeof(sidx), 1, f);
  printf("Package name is '%.32s' (%d files?)\n", sidx.name, sidx.unk2);

  SpkPackage* package = indexPackage(spk);
  package->name = strndup(sidx.name, 32-3);
  memcpy(package->shortname, &sidx.name[32-3], 3);
  package->shortname[3] = 0;
  package->version.major = (sidx.unk0 >> 0) & 0xFF;
  package->version.minor = (sidx.unk0 >> 8) & 0xFF;
  package->version.patch = (sidx.unk0 >> 16) & 0xFF;
  package->type = (sidx.unk0 >> 24) & 0xFF;
  

  //FIXME: Check if unk3 was 0xFFFFFFFF and then assert SZ64
  if (sidx.sdatSize == 0xFFFFFFFF) {
    fread(magic, 4, 1, f);
    assert(memcmp(magic, "SZ64", 4) == 0);
    // Spike 2 only?
    struct {
      uint32_t chunkSize;
      uint64_t sdatSize;
    } __attribute__((packed)) sz64;
    fread(&sz64, sizeof(sz64), 1, f);
    assert(sz64.chunkSize == 8);
  } 
  
  fread(magic, 4, 1, f);
  assert(memcmp(magic, "STRS", 4) == 0);
  {
    uint32_t length;
    fread(&length, 4, 1, f);
    package->strs = ftell(f);
    fseek(f, length, SEEK_CUR);
  }

  size_t sdatSize = 0;
  for(uint32_t i = 0; i < sidx.unk2; i++) {
    fread(magic, 4, 1, f);

    if (memcmp(magic, "FI64", 4) == 0) {

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

      fread(&fi64, sizeof(fi64), 1, f);
      assert(fi64.unk0 == (sizeof(fi64) - 4));
      assert(fi64.length == fi64.length2);

      indexFile(package, f, fi64.strs_offset, fi64.sdat_offset, fi64.length, fi64.permissions, fi64.checksum, fi64.checksum2);
      
      sdatSize += fi64.length;

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

      fread(&finf, sizeof(finf), 1, f);
      assert(finf.unk0 == (sizeof(finf) - 4));
      assert(finf.length == finf.length2);

      indexFile(package, f, finf.strs_offset, finf.sdat_offset, finf.length, finf.permissions, finf.checksum, finf.checksum2);

      sdatSize += finf.length;

    } else {
      assert(false);
    }
  }

  fread(magic, 4, 1, f);
  assert(memcmp(magic, "FEND", 4) == 0);
  {
    uint8_t unk[4];
    fread(unk, 4, 1, f);
  }

  assert(ftell(f) == next_offset);

  //FIXME: SDAT is a separate chunk, so this should be a separate function

  fread(magic, 4, 1, f);
  assert(memcmp(magic, "SDAT", 4) == 0);
  {
    uint64_t unk = readLength(f);
    package->sdat = ftell(f);
  }

  // Skip rest of data
  printf("Skipping %zu\n", sdatSize);
  fseek(f, sdatSize, SEEK_CUR);


}

static void readSpk0(Spk* spk, FILE* f) {
  uint8_t magic[4];
  fread(magic, 4, 1, f);
  assert(memcmp(magic, "SPK0", 4) == 0);
  uint64_t length = readLength(f);
  off_t next_offset = ftell(f) + length;
  readSidx(spk, f);
  assert(ftell(f) == next_offset);
}


void readChunkHeader(FILE* f, uint8_t* magic, uint64_t* value) {
  fread(magic, 4, 1, f);
  *value = readLength(f);
}

void readSpksData(Spk* spk, FILE* f, uint64_t length) {
  uint32_t chunkCount;
  fread(&chunkCount, 4, 1, f);
  for(uint32_t i = 0; i < chunkCount; i++) {
    readSpk0(spk, f);
  }
}

Spk* spk_parse(FILE* f) {
  uint8_t magic[4];

  Spk* spk = (Spk*)malloc(sizeof(Spk));
  spk->package_count = 0;
  spk->packages = NULL;

  uint64_t value;
  spk->offset = 0;
  readChunkHeader(f, magic, &value);
  if (memcmp(magic, "SPKS", 4) == 0) {
    readSpksData(spk, f, value);
  } else {
    off_t endOfSpks;
    
    fseek(f, -12, SEEK_END);
    endOfSpks = ftell(f);
    readChunkHeader(f, magic, &value);
    if (memcmp(magic, "SEND", 4) == 0) {
      assert(value == 4);
      struct {
        uint32_t offset;
      } __attribute__((packed)) send;
      if (fread(&send, 1, sizeof(send), f) != sizeof(send)) {
        printf("Incomplete SEND\n");
        assert(false);
      }
      printf("SPKS at 0x%08" PRIX32 "\n", send.offset);
      fseek(f, send.offset, SEEK_SET);
    } else {
      fseek(f, -16, SEEK_END);
      endOfSpks = ftell(f);
      readChunkHeader(f, magic, &value);
      if (memcmp(magic, "SE64", 4) == 0) {
        assert(value == 8);
        struct {
          uint64_t offset;
        } __attribute__((packed)) se64;
        if (fread(&se64, 1, sizeof(se64), f) != sizeof(se64)) {
          printf("Incomplete SE64\n");
          assert(false);
        }
        printf("SPKS at 0x%016" PRIX64 "\n", se64.offset);
        fseek(f, se64.offset, SEEK_SET);
      } else {
        printf("Unable to find SPKS, then unable to find SEND / SE64\n");
        return NULL;
      }
    }

    spk->offset = ftell(f);
    readChunkHeader(f, magic, &value);
    assert(memcmp(magic, "SPKS", 4) == 0);
    readSpksData(spk, f, value);
    assert(ftell(f) == endOfSpks);
  }

  return spk;
}

void spk_free(Spk* spk) {
  for(unsigned int i = 0; i < spk->package_count; i++) {
    SpkPackage* package = &spk->packages[i];
    free(package->files);
    free(package->name);
  }
  free(spk->packages);
  free(spk);
}


static char* get_spk_package_foldername(const SpkPackage* package) {
  static char foldername[64];

  if (package->type != 2) {
    sprintf(foldername, "%s-%d_%d_%d", package->name, package->version.major, package->version.minor, package->version.patch);
  } else {
    sprintf(foldername, "%s-%d_%02d_%d", package->name, package->version.major, package->version.minor, package->version.patch);
  }
  
  return foldername;
}


static Folder* createFolder() {
  Folder* folder = new Folder();
  folder->name = NULL;
  folder->folder_count = 0;
  folder->folders = NULL;
  folder->file_count = 0;
  folder->files = NULL;
  return folder;
}


void addFileToFolder(Folder* parent, File* child) {
  parent->files = (File**)realloc(parent->files, sizeof(File*) * ++parent->file_count);
  parent->files[parent->file_count - 1] = child;
}

void addFolderToFolder(Folder* parent, Folder* child) {
  parent->folders = (Folder**)realloc(parent->folders, sizeof(Folder*) * ++parent->folder_count);
  parent->folders[parent->folder_count - 1] = child;
}

Folder* findFolder(Folder* folder, char* name) {
  for(unsigned int i = 0; i < folder->folder_count; i++) {
    Folder* subfolder = folder->folders[i];
    if (!strcmp(subfolder->name, name)) {
      return subfolder;
    }
  }
  return NULL;
}

Folder* addFileInPath(Folder* folder, char* path, File* file) {
  char* tmp = strdup(path);
  char* cursor = tmp;
  while(true) {
    char* slash = strchr(cursor, '/');
    if (slash == NULL) {      
      if (strlen(cursor) > 0) {
        assert(file->name == NULL);
        file->name = strdup(cursor);
      }
      addFileToFolder(folder, file);
      break;
    } else {
      *slash = '\0';
      Folder* subfolder = findFolder(folder, cursor);
      if (subfolder == NULL) {
        subfolder = createFolder();
        subfolder->name = strdup(cursor);
        addFolderToFolder(folder, subfolder);
      }
      folder = subfolder;
      cursor = &slash[1];
    }
  }
  free(tmp);
  return folder;
}

Folder* splitPackageIntoFolders(const SpkPackage* package, SpkReadCb strsRead, SpkReadCb sdatRead) {

  char* folderName = get_spk_package_foldername(package);

  Folder* folder = createFolder();
  folder->name = strdup(folderName);

  for(unsigned int i = 0; i < package->file_count; i++) {
    SpkFile* file = &package->files[i];
    File* abstractFile = new File();
    abstractFile->name = NULL;
    abstractFile->permissions = file->permissions;
    abstractFile->size = file->size;
    abstractFile->read = [=](void* data, off_t offset, size_t length) {
      sdatRead(package, file, data, offset, length);
    };
    char path[MAX_PATH];
    strsRead(package, file, path, 0, MAX_PATH);
    addFileInPath(folder, path, abstractFile); 
  }

  return folder;
}



Folder* splitSpkIntoFolders(const Spk* spk, FileReadCb rawRead, SpkReadCb strsRead, SpkReadCb sdatRead) {
  Folder* root_folder = createFolder();
  root_folder->name = strdup("");

  for(unsigned int i = 0; i < spk->package_count; i++) {
    SpkPackage* package = &spk->packages[i];
    Folder* package_folder = splitPackageIntoFolders(package, strsRead, sdatRead);
    addFolderToFolder(root_folder, package_folder);
  }
  
  const char* headerFilename = "header.tar.gz";

  if (spk->offset > 0) {
    File* headerFile = new File();
    headerFile->name = strdup(headerFilename);
    headerFile->permissions = 0755;
    headerFile->size = spk->offset;
    headerFile->read = [=](void* data, off_t offset, size_t length) {
      rawRead(data, offset, length);
    };
    addFileToFolder(root_folder, headerFile);
  }

  // Export a custom JSON file which contains everything needed to reconstruct this SPK (mainly order of files)
  struct ContentFile : File {
    std::string content;
  };
  std::string content = "";
  content += "{\n";
  if (spk->offset > 0) {
    content += "  \"header\": \"" + std::string(headerFilename) + "\",";
  }
  content += "  \"packages\": {\n";
  for(unsigned int i = 0; i < spk->package_count; i++) {
    if (i > 0) {
      content += ",\n";
    }
    SpkPackage* package = &spk->packages[i];
    content += "    \"" + std::string(package->name) + "\": {\n";
    size_t shortnameLength = strnlen(package->shortname, 3);
    if (shortnameLength > 0) {
      content += "      \"shortname\": \"" + std::string(package->shortname, shortnameLength) + "\",\n";
    }
    std::string type;
    switch(package->type) {
      case 1: type = "SPIKE_1"; break;
      case 2: type = "GAME"; break;
      case 3: type = "SPIKE_2"; break;
      case 4: type = "SPIKE_3"; break;
      default:
        type = "UNKNOWN_" + std::to_string(package->type);
    }
    content += "      \"type\": \"" + type + "\",\n";
    content += "      \"version\": [" + std::to_string(package->version.major) + "," + std::to_string(package->version.minor) + "," + std::to_string(package->version.patch) + "],\n";
    content += "      \"files\": [\n";
    for(unsigned int i = 0; i < package->file_count; i++) {
      if (i > 0) {
        content += ",\n";
      }
      SpkFile* file = &package->files[i];
      char path[MAX_PATH];
      strsRead(package, file, path, 0, MAX_PATH);
      content += "        \"" + std::string(path) + "\"";
    }
    content += "\n"
               "      ]\n";
    content += "    }";
  }
  content += "\n"
            "  }\n";
  content += "}\n";

  ContentFile* metadataFile = new ContentFile();
  metadataFile->content = content;
  metadataFile->name = strdup("metadata.json");
  metadataFile->permissions = 0755;
  metadataFile->size = content.length();
  metadataFile->read = [=](void* data, off_t offset, size_t length) {
    memcpy(data, &metadataFile->content.c_str()[offset], length);
  };
  addFileToFolder(root_folder, metadataFile);

  return root_folder;
}

static void freeFolder(Folder* folder) {
  free(folder->name);
  free(folder->folders);
  free(folder->files);
  delete folder;
}

static void freeFile(File* file) {
  free(file->name);
  delete file;
}

void freeFolders(Folder* root_folder) {
  for(unsigned int i = 0; i < root_folder->folder_count; i++) {
    freeFolders(root_folder->folders[i]);
  }
  for(unsigned int i = 0; i < root_folder->file_count; i++) {
    freeFile(root_folder->files[i]);
  }
  freeFolder(root_folder);
}


// Small helper as we are currently using FILE to load everything
// In the future, there might be mmap support
Folder* splitSpkIntoFoldersFromFILE(const Spk* spk, FILE* f) {
  return splitSpkIntoFolders(spk,
    [=](void* data, off_t offset, size_t length) {
      fseek(f, offset, SEEK_SET);
      fread(data, 1, length, f);
    },
    [=](const SpkPackage* package, const SpkFile* file, void* data, off_t offset, size_t length) {
      fseek(f, package->strs + file->strs_offset + offset, SEEK_SET);
      fread(data, 1, length, f);
    },
    [=](const SpkPackage* package, const SpkFile* file, void* data, off_t offset, size_t length) {
      fseek(f, package->sdat + file->sdat_offset + offset, SEEK_SET);
      fread(data, 1, length, f);
    }
  );
}