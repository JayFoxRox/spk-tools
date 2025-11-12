// Copyright (C) 2018 Jannik Vogel

#define FUSE_USE_VERSION 31
#include "fuse3/fuse.h"

#include <cstdio>
#include <cstring>
#include <cerrno>
#include <cstddef>
#include <cassert>

#include <string>

#include <fcntl.h>

#include "spk.h"

#define PATH_MAX 2048

static Folder* findFolder(Folder* folder, const char* path) {
  const char* slash = strchr(path, '/');
  
  // No more slash, so we are looking at an entry in a folder
  if (slash == NULL) {
    return folder;
  }

  // We skip trailing slashes
  if (slash == path) {
    return findFolder(folder, &slash[1]);
  }

  std::string entryname = std::string(path, slash);

  for(unsigned int i = 0; i < folder->folder_count; i++) {
    Folder* subfolder = folder->folders[i];
    if (std::string(subfolder->name) == entryname) {
      return findFolder(subfolder, &slash[1]);
    }
  }

  return NULL;
}

static File* findFile(Folder* folder, const char* path) {

  const char* filename = path;
  const char* slash = strrchr(path, '/');
  if (slash != NULL) {
    std::string folderpath = std::string(path, slash + 1);
    folder = findFolder(folder, folderpath.c_str());
    if (folder == NULL) {
      return NULL;
    }
    filename = &slash[1];
  }
  
  for(unsigned int i = 0; i < folder->file_count; i++) {
    File* file = folder->files[i];
    if (!strcmp(file->name, filename)) {
      return file;
    }
  }
  return NULL;
  
}

/*
 * Command line options
 *
 * We can't set default values for the char* fields here because
 * fuse_opt_parse would attempt to free() them when the user specifies
 * different values on the command line.
 */
static struct options {
  const char* path;
  int show_help;
} options;

#define OPTION(t, p)                           \
    { t, offsetof(struct options, p), 1 }
static const struct fuse_opt option_spec[] = {
  OPTION("--path=%s", path),
  OPTION("-h", show_help),
  OPTION("--help", show_help),
  FUSE_OPT_END
};

static FILE* f = NULL;
static Folder* root_folder = NULL;


static void *spk_fuse_init(struct fuse_conn_info *conn, struct fuse_config *cfg) {
  cfg->kernel_cache = 1;
  return NULL;
}


//FIXME: Handler which closes spk again?!

static int spk_fuse_getattr(const char *path, struct stat *stbuf, struct fuse_file_info *fi) {
  memset(stbuf, 0, sizeof(struct stat));

  Folder* folder = findFolder(root_folder, &path[1]);

  if (folder == NULL) {
    return -ENOENT;
  }

  const char* slash = strrchr(&path[1], '/');
  const char* entryname = (slash == NULL) ? &path[1] : &slash[1];

  // Path was a folder with trailing slash
  if (entryname[0] == '\0')  {
    stbuf->st_nlink = 1;
    stbuf->st_mode = S_IFDIR | 0755;
    return 0;
  }

  for(unsigned int i = 0; i < folder->folder_count; i++) {
    Folder* subfolder = folder->folders[i];
    if (!strcmp(subfolder->name, entryname)) {
      stbuf->st_nlink = 1;
      stbuf->st_mode = S_IFDIR | 0755;
      return 0;
    }
  }

  for(unsigned int i = 0; i < folder->file_count; i++) {
    File* file = folder->files[i];
    if (!strcmp(file->name, entryname)) {
      stbuf->st_nlink = 1;
      // S_IFREG should be in file->permissions, but we'll re-add for safety
      stbuf->st_mode = S_IFREG | file->permissions;
      stbuf->st_size = file->size; //strlen(options.contents);
      return 0;
    }
  }

  return -ENOENT;
}



static int spk_fuse_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi, enum fuse_readdir_flags flags) {
  std::string dirPath = std::string(&path[1]) + "/";
  Folder* folder = findFolder(root_folder, dirPath.c_str());

  if (folder == NULL) {
    return -ENOENT;
  }

  filler(buf, ".", NULL, 0, FUSE_FILL_DIR_DEFAULTS);
  if (strcmp(path, "/")) {
    filler(buf, "..", NULL, 0, FUSE_FILL_DIR_DEFAULTS);
  }

  for(unsigned int i = 0; i < folder->folder_count; i++) {
    Folder* subfolder = folder->folders[i];
    filler(buf, subfolder->name, NULL, 0, FUSE_FILL_DIR_DEFAULTS);
  }

  for(unsigned int i = 0; i < folder->file_count; i++) {
    File* file = folder->files[i];
    filler(buf, file->name, NULL, 0, FUSE_FILL_DIR_DEFAULTS);
  }

  return 0;
}

static int spk_fuse_open(const char *path, struct fuse_file_info *fi) {
  File* file = findFile(root_folder, &path[1]);

  if (file == NULL) {
    return -ENOENT;
  }

  if ((fi->flags & O_ACCMODE) != O_RDONLY) {
    return -EACCES;
  }

  return 0;
}


static int spk_fuse_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {
  File* file = findFile(root_folder, &path[1]);

  if (file == NULL) {
    return -ENOENT;
  }

  if (offset < file->size) {
    if ((offset + size) > file->size) {
      size = file->size - offset;
    }
    file->read(buf, offset, size);
  } else {
    size = 0;
  }

  return size;
}

static struct fuse_operations spk_fuse_oper = {
  .getattr = spk_fuse_getattr,
  .open    = spk_fuse_open,
  .read    = spk_fuse_read,
  .readdir = spk_fuse_readdir,
  .init    = spk_fuse_init,
};

static void show_help(const char *progname) {
  printf("usage: %s [options] <mountpoint>\n\n", progname);
}

int main(int argc, char *argv[]) {
  struct fuse_args args = FUSE_ARGS_INIT(argc, argv);

  /* Parse options */
  if (fuse_opt_parse(&args, &options, option_spec, NULL) == -1) {
    return 1;
  }

  /* When --help is specified, first print our own file-system
     specific help text, then signal fuse_main to show
     additional help (by adding `--help` to the options again)
     without usage: line (by setting argv[0] to the empty
     string) */
  if (options.show_help) {
    show_help(argv[0]);
    assert(fuse_opt_add_arg(&args, "--help") == 0);
    args.argv[0] = (char*) "";
  } else {
    if (options.path == NULL) {
      printf("Please provide an spk-path using `--path=example.spk`\n");
      return 1;
    }

    f = fopen(options.path, "rb");
    if (f == NULL) {
      printf("Unable to open '%s'\n", options.path);
      return 1;
    }

    Spk* spk = spk_parse(f);
    if (spk == NULL) {
      printf("Unable to parse SPK\n");
      return 1;
    }

    root_folder = splitSpkIntoFoldersFromFILE(spk, f);

    printf("Mounting..\n");
  }

  // Force single-thread to avoid seeking while still reading in callback
  assert(fuse_opt_add_arg(&args, "-s") == 0);

  return fuse_main(args.argc, args.argv, &spk_fuse_oper, NULL);
}

