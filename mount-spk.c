// Copyright (C) 2018 Jannik Vogel

#define FUSE_USE_VERSION 31
#include "fuse3/fuse.h"

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <assert.h>

#include "spk.h"

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
static Spk* spk = NULL;
static SpkFolder* spk_folder = NULL;

static void *spk_fuse_init(struct fuse_conn_info *conn, struct fuse_config *cfg) {
  cfg->kernel_cache = 1;
  return NULL;
}

//FIXME: Handler which closes spk again?!

static int spk_fuse_getattr(const char *path, struct stat *stbuf, struct fuse_file_info *fi) {
  memset(stbuf, 0, sizeof(struct stat));

  SpkFolder* folder = spk_get_folder(spk_folder, &path[1]);

  if (!strcmp(path, "/")) {
    folder = spk_folder;
  }

  if (folder != NULL) {
    stbuf->st_nlink = 2;
    stbuf->st_mode = S_IFDIR | 0755;
    return 0;
  }

  SpkFile* file = spk_get_file(spk_folder, &path[1]);
  if (file != NULL) {
    stbuf->st_nlink = 1;
    // S_IFREG should be in file->permissions, but we'll re-add for safety
    stbuf->st_mode = S_IFREG | file->permissions;
    stbuf->st_size = file->size; //strlen(options.contents);
    return 0;
  }

  return -ENOENT;
}

static int spk_fuse_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi, enum fuse_readdir_flags flags) {

  SpkFolder* folder = spk_get_folder(spk_folder, &path[1]);

  if (!strcmp(path, "/")) {
    folder = spk_folder;
  }

  if (folder == NULL) {
    return -ENOENT;
  }

  filler(buf, ".", NULL, 0, 0);
  filler(buf, "..", NULL, 0, 0);

  for(int i = 0; i < folder->folder_count; i++) {
    SpkFolder* subfolder = &folder->folders[i];
    filler(buf, subfolder->name, NULL, 0, 0);
  }

  for(int i = 0; i < folder->file_count; i++) {
    SpkFile* file = &folder->files[i];
    filler(buf, file->name, NULL, 0, 0);
  }

  return 0;
}

static int spk_fuse_open(const char *path, struct fuse_file_info *fi) {
  SpkFile* file = spk_get_file(spk_folder, &path[1]);

  if (file == NULL) {
    return -ENOENT;
  }

  if ((fi->flags & O_ACCMODE) != O_RDONLY) {
    return -EACCES;
  }

  return 0;
}

static int spk_fuse_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {
  SpkFile* file = spk_get_file(spk_folder, &path[1]);

  if (file == NULL) {
    return -ENOENT;
  }

  if (offset < file->size) {
    if ((offset + size) > file->size) {
      size = file->size - offset;
    }

    fseek(f, file->sdat_offset + offset, SEEK_SET);
    size = fread(buf, 1, size, f);
  } else {
    size = 0;
  }

  return size;
}

static struct fuse_operations spk_fuse_oper = {
  .init    = spk_fuse_init,
  .getattr = spk_fuse_getattr,
  .readdir = spk_fuse_readdir,
  .open    = spk_fuse_open,
  .read    = spk_fuse_read,
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

    spk = spk_parse(f);
    if (spk == NULL) {
      printf("Unable to parse SPK\n");
      return 1;
    }

    spk_folder = spk->packages[0].root_folder;

    printf("Mounting..\n");
  }

  // Force single-thread to avoid seeking while still reading in callback
  assert(fuse_opt_add_arg(&args, "-s") == 0);

  return fuse_main(args.argc, args.argv, &spk_fuse_oper, NULL);
}

