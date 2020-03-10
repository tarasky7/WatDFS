//
// Starter code for CS 454/654
// You SHOULD change this file
//

#include "watdfs_client.h"
#include "debug.h"

#include "watdfs_client_rpc.cpp"
#include <ctime>
#include <set>
#include <map>

#include "rw_lock.h"

struct file_info {
    int fd_cli; // fd for client, for local call.
    int fd_ser; // fd for server, for rpc call.
    int flags; // open mode
    time_t tc;
};

struct global_info {
    char *path_to_cache; // The path to cache file in client.
    time_t cache_interval;
    std::set<std::string> opened_files;
    std::map<std::string, file_info *> files_info;
};

char *get_full_path(char *client_persist_dir, char *short_path);
bool file_is_open(void *userdata, const char *path);
int watdfs_cli_upload(void *userdata, const char *path, struct fuse_file_info *fi);
int watdfs_cli_download(void *userdata, const char *path, struct fuse_file_info *fi);
int check_read_fresh(void *userdata, const char *path);
int check_write_fresh(void *userdata, const char *path);

char *get_full_path(char *client_persist_dir, const char *short_path) {
    int short_path_len = strlen(short_path);
    int dir_len = strlen(client_persist_dir);
    int full_len = dir_len + short_path_len + 1;

    char *full_path = (char *)malloc(full_len);

    // First fill in the directory.
    strcpy(full_path, client_persist_dir);
    // Then append the path.
    strcat(full_path, short_path);
    DLOG("Full path: %s\n", full_path);

    return full_path;
}

bool file_is_open(void *userdata, const char *path) {
    struct global_info *p = (global_info *)userdata;
    if (p->opened_files.find(path) != p->opened_files.end()) {
        return true;
    }
    return false;
}

int watdfs_cli_download(void *userdata, const char *path, struct fuse_file_info *fi) {

    DLOG("download is called\n");

    global_info *p = (global_info *)userdata;
    char *full_path = get_full_path(((global_info *)userdata)->path_to_cache, path);
    struct stat *statbuf = new struct stat;
    int fxn_ret = watdfs_cli_getattr_rpc(userdata, path, statbuf);
    int fd_cli;     // File descriptor for client file.

    if (fxn_ret < 0) {
        // Getattr rpc call failed.
        DLOG("Getattr rpc call failed.\n");
        free(full_path);
        delete statbuf;
        return fxn_ret;
    }

    off_t file_size = statbuf->st_size;

    fxn_ret = watdfs_cli_open_rpc(userdata, path, fi);

    if (fxn_ret < 0) {
        // Rpc or server open call failed.
        DLOG("rpc open failed\n");
        return fxn_ret;
    }

    // Set file info structure.
    if (p->files_info.find(path) == p->files_info.end()) {
        DLOG("creating new file_info\n");
        p->files_info[path] = new struct file_info;
    }

    DLOG("fi->fh on server %ld\n", fi->fh);
    p->files_info[path]->fd_ser = fi->fh;


    // Make sys open call.
    DLOG("open in download is called\n");
    int file_ret = open(full_path, O_RDWR | O_CREAT, S_IRWXU);
    if (file_ret < 0) {
        // System call failed.
        fxn_ret = -errno;
        free(full_path);
        delete statbuf;
        return fxn_ret;
    }

    fd_cli = file_ret;

    ((global_info *)userdata)->files_info[path]->fd_cli = fd_cli;

    // Truncate the file to the correct size.
    file_ret = truncate(full_path, file_size);

    if (file_ret < 0) {
        free(full_path);
        delete statbuf;
        return -errno;
    }

    char *buf = (char *)malloc((file_size+1) * sizeof(char));

    // fxn_ret = lock(path, RW_READ_LOCK);

    // if (fxn_ret < 0) {
    //     free(full_path);
    //     free(buf);
    //     delete statbuf;
    //     return fxn_ret;
    // }

    // Read from server to buf.
    fxn_ret = watdfs_cli_read_rpc(userdata, path, buf, file_size, 0, fi);

    if (fxn_ret < 0) {
        free(full_path);
        delete statbuf;
        free(buf);
        // unlock(path, RW_READ_LOCK);
        return fxn_ret;
    }
    
    // fxn_ret = unlock(path, RW_READ_LOCK);
    // if (fxn_ret < 0) {
    //     free(full_path);
    //     delete statbuf;
    //     free(buf);
    //     return fxn_ret;
    // }

    // Write from buf to client file.
    fxn_ret = pwrite(fd_cli, buf, file_size, 0);

    if (fxn_ret < 0) {
        free(full_path);
        delete statbuf;
        free(buf);
        return fxn_ret;
    }

    // Update the metadata.
    struct timespec * ts = new struct timespec [2];
    ts[1] = statbuf->st_mtim;
    ts[0] = statbuf->st_atim;
    fxn_ret = utimensat(0, full_path, ts, 0);
    
    free(full_path);
    delete statbuf;
    free(buf);
    return fxn_ret;
}

int watdfs_cli_upload(void *userdata, const char *path, struct fuse_file_info *fi) {
    char *full_path = get_full_path(((global_info *)userdata)->path_to_cache, path);
    struct stat *statbuf = new struct stat;
    int fxn_ret = stat(full_path, statbuf);
    global_info *p = (global_info *)userdata;

    if (fxn_ret < 0) {
        fxn_ret = -errno;
        memset(statbuf, 0, sizeof(struct stat));
        delete statbuf;
        free(full_path);
        return fxn_ret;
    }

    off_t file_size = statbuf->st_size;
    char *buf = (char *)malloc(file_size * sizeof(char));

    // Read from file to buf.
    int fd_cli = p->files_info[path]->fd_cli;
    fxn_ret = pread(fd_cli, buf, file_size, 0);

    if (fxn_ret < 0) {
        fxn_ret = -errno;
        delete statbuf;
        free(full_path);
        free(buf);
        return fxn_ret;
    }

    // fxn_ret = lock(path, RW_WRITE_LOCK);

    // if (fxn_ret < 0) {
    //     free(full_path);
    //     delete statbuf;
    //     free(buf);
    //     return fxn_ret;
    // }

    // First truncate file.
    fxn_ret = watdfs_cli_truncate_rpc(userdata, path, file_size);

    if (fxn_ret < 0) {
        delete statbuf;
        free(full_path);
        free(buf);
        // unlock(path, RW_WRITE_LOCK);
        return fxn_ret;
    }

    // Write to server.
    fxn_ret = watdfs_cli_write_rpc(userdata, path, buf, file_size, 0, fi);

    if (fxn_ret < 0) {
        delete statbuf;
        free(full_path);
        free(buf);
        // unlock(path, RW_WRITE_LOCK);
        return fxn_ret;
    }

    // Update time metadata on server.
    struct timespec * ts = new struct timespec [2];
    ts[1] = statbuf->st_mtim;
    ts[0] = statbuf->st_atim;
    fxn_ret = watdfs_cli_utimens_rpc(userdata, path, ts);

    if (fxn_ret < 0) {
        free(full_path);
        free(buf);
        delete statbuf;
        // unlock(path, RW_WRITE_LOCK);
        return fxn_ret;
    }

    // fxn_ret = unlock(path, RW_WRITE_LOCK);

    delete statbuf;
    free(full_path);
    free(buf);

    return fxn_ret;
}

int check_read_fresh(void *userdata, const char *path) {
    DLOG("read freshness is called\n");
    global_info *p = (global_info *)userdata;
    time_t tc = p->files_info[path]->tc;
    time_t T = time(nullptr);
    time_t t = p->cache_interval;

    if (T - tc < t) {
        return 0;
    }

    char *full_path = get_full_path(p->path_to_cache, path);
    struct stat *statbuf = new struct stat;
    
    int fxn_ret = stat(full_path, statbuf);

    if (fxn_ret < 0) {
        delete statbuf;
        return -errno;
    }
    
    time_t t_client = statbuf->st_mtime;

    fxn_ret = watdfs_cli_getattr(userdata, path, statbuf);

    if (fxn_ret < 0) {
        delete statbuf;
        return fxn_ret;
    }

    time_t t_server = statbuf->st_mtime;

    if (t_client == t_server) {
        // Set tc to current time.
        p->files_info[path]->tc = time(nullptr);
        delete statbuf;
        return 0;
    }

    // Download new file.
    struct fuse_file_info * fi = new struct fuse_file_info;
    fi->fh = p->files_info[path]->fd_ser;
    fi->flags = O_RDONLY;

    fxn_ret = watdfs_cli_download(userdata, path, fi);

    // Set tc to current time.
    p->files_info[path]->tc = time(nullptr);

    delete statbuf;
    delete fi;
    return fxn_ret;
}

int check_write_fresh(void *userdata, const char *path) {
    global_info *p = (global_info *)userdata;
    time_t tc = p->files_info[path]->tc;
    time_t T = time(nullptr);
    time_t t = p->cache_interval;

    if (T - tc < t) {
        return 0;
    }

    char *full_path = get_full_path(p->path_to_cache, path);
    struct stat *statbuf = new struct stat;
    
    int fxn_ret = stat(full_path, statbuf);

    if (fxn_ret < 0) {
        delete statbuf;
        return -errno;
    }
    
    time_t t_client = statbuf->st_mtime;

    fxn_ret = watdfs_cli_getattr(userdata, path, statbuf);

    if (fxn_ret < 0) {
        delete statbuf;
        return fxn_ret;
    }

    time_t t_server = statbuf->st_mtime;

    if (t_client == t_server) {
        // Set tc to current time.
        p->files_info[path]->tc = time(nullptr);
        delete statbuf;
        return 0;
    }

    // Upload new file.
    struct fuse_file_info * fi = new struct fuse_file_info;
    fi->fh = p->files_info[path]->fd_ser;
    fi->flags = O_RDWR;

    fxn_ret = watdfs_cli_upload(userdata, path, fi);

    // Set tc to current time.
    p->files_info[path]->tc = time(nullptr);

    delete statbuf;
    delete fi;
    return fxn_ret;
}