#define FUSE_USE_VERSION 26
#include <fuse.h>

#include "tagioc.h"

#include "./file_descriptor.h"
#include "./sys.h"
#include "./util.h"
#include "./tag.h"
#include "./file.h"
#include "./path.h"
#include "./db.h"

static bool file_matches_tags(
    struct file *f, struct hash_table *selected_tags)
{
    return db_file_match_tags(f->id, selected_tags);
}

static int tag_open(
    const char *user_path, struct fuse_file_info *fi, int is_tag)
{
    struct path *path = path_create(user_path, is_tag);
    struct file_descriptor *fd;
    fd = fd_open(path, fi->flags, is_tag);
    if (IS_ERR(fd))
        return PTR_ERR(fd);
    fi->fh = (uint64_t) fd;
    return 0;
}

static int tag_open_file(const char *user_path, struct fuse_file_info *fi)
{
    return tag_open(user_path, fi, 0);
}

static int tag_open_dir(const char *user_path, struct fuse_file_info *fi)
{
    return tag_open(user_path, fi, 1);
}

static int tag_release_generic(const char *user_path, struct fuse_file_info *fi)
{
    struct file_descriptor *fd = (struct file_descriptor*) fi->fh;
    return fd_close(fd);
}

static int getattr_intra(struct path *p, struct stat *stbuf)
{
    int res;

    /* try to stat the actual file */
    if ((res = stat(p->realpath, stbuf)) < 0 || S_ISDIR(stbuf->st_mode)) {
        if (res < 0) {
            print_log(_("error ::%s :: %s\n"), p->realpath, strerror(errno));
        }
        /* if the file doesn't exist, check if it's a tag
           (or the root) directory and stat the main directory instead */
        if (!strcmp(p->virtpath, "/") ||
            (tag_exist(p->filename) &&
             !ht_has_entry(p->selected_tags, p->filename))) {
            res = stat(realdirpath, stbuf);
        } else {
            res = -ENOENT;
        }
    } else {
        /* if the file exist check that it have the selected tags*/
        struct file *f = file_get_or_create(p->filename);
        if (ht_entry_count(p->selected_tags) > 0) {
            if (!file_matches_tags(f, p->selected_tags)) {
                res = -ENOENT;
            }
        } else if (!strcmp(p->filename, TAG_FILENAME)) {
            /* tag file is special and have "almost infinite" size */
            stbuf->st_size = (off_t) LONG_MAX;
        }
    }
    return res;
}

/* get attributes */
static int tag_getattr(const char *user_path, struct stat *stbuf)
{
    int res;
    print_log(_("getattr '%s'\n"), user_path);
    struct path *path = path_create(user_path, 0);
    res = getattr_intra(path, stbuf);
    print_log(_("getattr returning '%s'\n"), strerror(-res));
    path_delete(path);
    return res;
}

static int tag_fgetattr(
    const char *user_path, struct stat *stbuf, struct fuse_file_info *fi)
{
    struct file_descriptor *fd = (struct file_descriptor*)fi->fh;
    struct path *path = fd->path;
    return getattr_intra(path, stbuf);
}

static int path_unlink(struct path *fd)
{
    struct file *f;

    f = file_get(fd->filename);
    if (NULL == f) {
        return -ENOENT;
    }
    void remove_tag(const char *filename, void *tag, void *file)
    {
        untag_file(tag, file);
    }
    ht_for_each(fd->selected_tags, &remove_tag, f);
    return 0;
}

static int tag_unlink(const char *user_path)
{
    int res = 0;
    int slash_count = get_character_count(user_path, '/');

    print_log("unlink '%s'\n", user_path);
    if (slash_count <= 1)
        return -EPERM;

    struct path *fd = path_create(user_path, 0);
    res = path_unlink(fd);

    path_delete(fd);
    return res;
}

static int tag_rmdir(const char *user_path)
{
    int res = 0;
    int slash_count = get_character_count(user_path, '/');
    print_log(_("rmdir '%s'\n"), user_path);
    if (slash_count > 1)
        return -EPERM;

    char *tag = basenamedup(user_path);
    struct tag *t = tag_get(tag);
    if (NULL == t) {
        res = -ENOENT;
    } else {
        tag_remove(t);
    }
    free(tag);
    return res;
}

static int tag_mkdir(const char *user_path, mode_t mode)
{
    int res = 0;
    print_log(_("mkdir '%s'\n"), user_path);
    int slash_count = get_character_count(user_path, '/');
    if (slash_count > 1)
        return  -EPERM;

    char *filename = basenamedup(user_path);
    struct tag *t;
    t = tag_get(filename);
    if (INVALID_TAG != t) {
        res = -EEXIST;
    } else {
        tag_get_or_create(filename);
    }
    free(filename);
    return res;
}

static void readdir_list_files(
    void *buf, struct path *dirpath, fuse_fill_dir_t filler)
{
    struct dirent *dirent;

    rewinddir(realdir);
    while ((dirent = readdir(realdir)) != NULL) {
        int res;
        struct stat stbuf;
        /* only list files, don't list directories */
        if (dirent->d_type == DT_DIR)
            continue;

        /* only files whose contents matches tags in path */
        struct path p;
        path_set(&p, dirpath->virtpath, dirent->d_name, dirpath->selected_tags);
        res = getattr_intra(&p, &stbuf);
        path_free(&p);
        if (res < 0)
            continue;
        filler(buf, dirent->d_name, NULL, 0);
    }
}

/* list files within directory */
static int tag_readdir(
    const char *user_path, void *buf, fuse_fill_dir_t filler,
    off_t offset, struct fuse_file_info *fi)
{
    struct file_descriptor *fd = (struct file_descriptor*) fi->fh;
    struct path *path = fd->path;

    print_log(_("\n\nreaddir '%s'\n"), path->virtpath);
    filler(buf, ".", NULL, 0);
    filler(buf, "..", NULL, 0);

    if (path->depth > 0)
        db_list_remaining_tags(path->selected_tags, buf, filler);
    else
        db_list_all_tags(buf, filler);
    readdir_list_files(buf, path, filler);

    int res = 0;
    print_log(_("readdir returning %s\n\n\n"), strerror(-res));
    return res;
}

static int read_tag_file(char *buffer, size_t len, off_t off)
{
    int res = 0;
    FILE *tagfile = tmpfile();
    tag_db_dump(tagfile);
    rewind(tagfile);
    res = fseek(tagfile, off, SEEK_SET);
    if (res < 0) {
        res = -errno;
        goto out;
    }
    res = fread(buffer, 1, len, tagfile);
    if (res < 0) {
        res = -errno;
    } else if (res < len) {
        print_debug("res = %d ;; len = %d\n", res, len);
        for (int i = res; i < len; ++i) {
            buffer[i] = 0;
        }
    }
  out:
    fclose(tagfile);
    if (res < 0)
        print_log(_("read_tag returning '%s'\n"), strerror(-res));
    else
        print_log(_("read_tag returning success (read %d)\n"), res);
    return res;
}

/* read the content of the file */
static int tag_read(
    const char *path, char *buffer, size_t len,
    off_t off, struct fuse_file_info *fi)
{
    int res = 0;
    struct file_descriptor *fd = (struct file_descriptor*) fi->fh;

    if (fd->is_tag_file)
        return read_tag_file(buffer, len, off);

    print_log(_("read '%s' for %ld bytes starting at offset %ld\n"), path, len, off);
    res = fd_read(fd, buffer, len, off);
    if (res < 0)
        print_log(_("read returning '%s'\n"), strerror(-res));
    else
        print_log(_("read returning success (read %d)\n"), res);
    return res;
}

static int tag_write(
    const char *path, const char *buffer, size_t len,
    off_t off, struct fuse_file_info *fi)
{
    int res = 0;
    struct file_descriptor *fd = (struct file_descriptor*) fi->fh;

    if (fd->is_tag_file)
        return -EPERM;

    print_log(_("write '%s' for %ld bytes starting at offset %ld\n"), path, len, off);
    res = fd_write(fd, buffer, len, off);
    if (res < 0)
        print_log(_("write returning '%s'\n"), strerror(-res));
    else
        print_log(_("write returning success (wrote %d)\n"), res);
    return res;
}

static int tag_mknod(const char *user_path, mode_t mode, dev_t device)
{
    int res;
    char *realpath = path_realpath(user_path);
    if (access(realpath, F_OK) == 0) {
        /* this condition may allow user to have a more comprehensible error
           message when a (hidden) corresponding directory exist
           in the 'remote' target directory */
        res = -EREMOTE;
    } else {
        res = mknod(realpath, mode, device);
    }
    free(realpath);
    return res;
}

static int tag_truncate(const char *user_path, off_t length)
{
    int res;
    if (!strcmp(user_path, "/.tags") || !strcmp(user_path, ".tags"))
        return -EPERM;
        
    char *realpath = path_realpath(user_path);
    res = truncate(realpath, length);
    free(realpath);
    return res;
}

static int tag_chmod(const char *user_path, mode_t mode)
{
    int res;
    char *realpath = path_realpath(user_path);
    res = chmod(realpath, mode);
    free(realpath);
    return res;
}

static int tag_chown(const char *user_path, uid_t user, gid_t group)
{
    int res;
    char *realpath = path_realpath(user_path);
    res = chown(realpath, user, group);
    free(realpath);
    return res;
}

static int tag_utime(const char *user_path, struct utimbuf *times)
{
    int res;
    char *realpath = path_realpath(user_path);
    res = utime(realpath, times);
    free(realpath);
    return res;
}

static int path_link(struct path *ffrom, struct path *fto)
{
    int res;
    struct stat stbuf;
    struct file *f;

    res = getattr_intra(ffrom, &stbuf);
    if (res < 0)
        return res;
    f = file_get_or_create(ffrom->filename);
    void tag_this_file(const char *tagname, void *tag, void* file)
    {
        tag_file(tag, file);
    }
    ht_for_each(fto->selected_tags, &tag_this_file, f);
    return res;
}

static int tag_link(const char *from, const char *to)
{
    int res = 0;
    print_log(_("link file from:'%s' - to:'%s'\n"), from, to);

    struct path *ffrom = path_create(from, 0);
    struct path *fto = path_create(to, 0);

    res = path_link(ffrom, fto);

    path_delete(ffrom);
    path_delete(fto);

    print_log(_("link returning '%s'\n"), strerror(-res));
    return res;
}

static int tag_rename(const char *from, const char *to)
{
    int res = 0;
    struct path *ffrom = path_create(from, 0);
    struct path *fto = path_create(to, 0);

    if (ht_entry_count(ffrom->selected_tags) != 1 ||
        ht_entry_count(fto->selected_tags) != 1)
        res =  -EPERM;
    else {
        path_unlink(ffrom);
        path_link(ffrom, fto);
    }

    path_delete(ffrom);
    path_delete(fto);

    return res;
}

static int ioctl_read_tags(struct file_descriptor *fd, struct tag_ioctl *data)
{
    struct file *f = file_get(fd->path->filename);
    if (NULL == f)
        return -ENOENT;

    int len;
    char *str = file_get_tags_string(f, &len);
    print_debug(_("file tag string : %s\n"), str);
    strncpy(data->buf, str, BUFSIZE);
    data->buf[BUFSIZE] = '\0';

    print_debug(_("copied data : %s\n"), data->buf);
    if (len > BUFSIZE)
        data->size = BUFSIZE;
    else
        data->size = len;
    free(str);
    return 0;
}

static int tag_ioctl(
    const char *path, int cmd, void *arg,
    struct fuse_file_info *fi, unsigned int flags, void *data)
{
    (void) flags;
    struct file_descriptor *fd = (struct file_descriptor*) fi->fh;

    if (fd->is_tag || fd->is_tag_file)
        return -EINVAL;

    if (flags & FUSE_IOCTL_COMPAT)
        return -ENOSYS;

    switch (cmd) {
    case TAG_IOC_READ_TAGS:
        return ioctl_read_tags(fd, data);
        break;
    }
    return -EINVAL;
}

static struct fuse_operations tag_oper = {
    .open = tag_open_file,
    .release = tag_release_generic,

    .opendir = tag_open_dir,
    .releasedir = tag_release_generic,

    .getattr = tag_getattr,
    .fgetattr = tag_fgetattr,

    .read = tag_read,
    .readdir = tag_readdir,
    .write = tag_write,

    .link = tag_link,
    .unlink = tag_unlink,
    .rmdir = tag_rmdir,

    .mknod = tag_mknod,
    .mkdir = tag_mkdir,
    .truncate = tag_truncate,
    .chmod = tag_chmod,
    .chown = tag_chown,
    .utime = tag_utime,
    .rename = tag_rename,

    .ioctl = tag_ioctl,

    .flag_nullpath_ok = 1,
};

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

    tagdbpath = append_dir(path, TAG_FILENAME);
    print_debug(_("tag filename = %s\n"), tagdbpath);
    parse_tags_db(tagdbpath, tag_oper.getattr);
}

static void save_tag_db(void)
{
    char *output_db_filename;
    output_db_filename = aasprintf("%s.new", tagdbpath);
    FILE *f = fopen(output_db_filename, "w");
    if (NULL != f) {
        tag_db_dump(f);
        fclose(f);
    }
    free(output_db_filename);
}

int main(int argc, char *argv[])
{
    int err;

    if (argc < 2) {
        fprintf(stderr, _("usage: %s target_directory mountpoint\n"), argv[0]);
        exit(EXIT_FAILURE);
    }


    /* find the absolute directory because fuse_main()
     * doesn't launch the daemon in the same current directory.
     */
    setlocale(LC_ALL, "");
    set_root_directory(argv[1]);

    argv++; argc--;

    print_log(_("starting %s in %s\n"), argv[0], realdirpath);
    err = fuse_main(argc, argv, &tag_oper, NULL);
    print_log(_("stopped %s with return code %d\n"), argv[0], err);

    save_tag_db();

    closedir(realdir);
    free(realdirpath);
    free(tagdbpath);
    printf(_("exiting normal way!"));

    return err;
}
