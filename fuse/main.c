#define FUSE_USE_VERSION 26

#include <stdio.h>
#include <limits.h>
#include <stdlib.h>
#include <dirent.h>
#include <string.h>
#include <errno.h>
#include <locale.h>

#include "parse.h"
#include "log.h"
#include "tag.h"
#include "file.h"
#include "fuse_callback.h"
#include "signal.h"

void ctrlc_int(int a)
{
    printf("exiting signal way!\n");
    exit(EXIT_FAILURE);
}

void install_sighandlers(void)
{
    struct sigaction sa = { 0 };
    sa.sa_handler = ctrlc_int;
    sa.sa_flags = 0;
    sigfillset(&sa.sa_mask);
    sigaction(SIGINT, &sa, NULL);
}

static void set_root_directory(const char *path)
{
    realdirpath = realpath(path, NULL);
    if (NULL == realdirpath) {
        fprintf(stderr, "%s: %s\n", path, strerror(errno));
        exit(EXIT_FAILURE);
    }
    realdir = opendir(realdirpath);
    if (!realdir) {
        fprintf(stderr, "%s: %s\n",
                realdirpath, strerror(errno));
        exit(EXIT_FAILURE);
    }

    char *tag_file;
    asprintf(&tag_file, "%s/%s", path, TAG_FILENAME);
    print_debug("tag filename = %s\n", tag_file);
    parse_tags(tag_file);
    free(tag_file);
}

__attribute__((constructor))
static void init_locale(void)
{
    /* met le programme dans la langue du système */
    setlocale(LC_ALL, "");
}


#ifdef DEBUG
static void manual_tag_for_test_purpose_only(void)
{
    struct file *f = file_get_or_create("log.c");
    struct tag *t = tag_get("paysage");
    tag_file(t, f);

    f = file_get_or_create("main.c");
    tag_file(t, f);
    t = tag_get("nuit");
    tag_file(t, f);
}
#endif

int main(int argc, char *argv[])
{
    int err;

    if (argc < 2) {
        fprintf(stderr, "usage: %s target_directory mountpoint\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    //install_sighandlers();
    /* find the absolute directory because fuse_main()
     * doesn't launch the daemon in the same current directory.
     */
    set_root_directory(argv[1]);
    argv++; argc--;
    #ifdef DEBUG
    manual_tag_for_test_purpose_only();
    #endif

    LOG("starting %s in %s\n", argv[0], realdirpath);
    err = fuse_main(argc, argv, &tag_oper, NULL);
    LOG("stopped %s with return code %d\n", argv[0], err);

    closedir(realdir);
    free(realdirpath);
    printf("exiting normal way!");

    return err;
}