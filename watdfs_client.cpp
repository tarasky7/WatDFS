//
// Starter code for CS 454/654
// You SHOULD change this file
//

#include "watdfs_client.h"
#include "debug.h"
INIT_LOG

#include "rpc.h"

#include "watdfs_client_helper.cpp"
#include "watdfs_client_rpc.cpp"
#include <set>
#include <map>
#include <sys/stat.h>
#include <ctime>

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
    std::map<const char *, file_info *> files_info;
};


// SETUP AND TEARDOWN
void *watdfs_cli_init(struct fuse_conn_info *conn, const char *path_to_cache,
                      time_t cache_interval, int *ret_code) {
    // TODO: set up the RPC library by calling `rpcClientInit`.
    rpcClientInit();
    // TODO: check the return code of the `rpcClientInit` call
    // `rpcClientInit` may fail, for example, if an incorrect port was exported.

    // It may be useful to print to stderr or stdout during debugging.
    // Important: Make sure you turn off logging prior to submission!
    // One useful technique is to use pre-processor flags like:
    // # ifdef PRINT_ERR
    // std::cerr << "Failed to initialize RPC Client" << std::endl;
    // #endif
    // Tip: Try using a macro for the above to minimize the debugging code.

    // TODO Initialize any global state that you require for the assignment and return it.
    // The value that you return here will be passed as userdata in other functions.
    // In A1, you might not need it, so you can return `nullptr`.
    // void *userdata = nullptr;

    // TODO: save `path_to_cache` and `cache_interval` (for A3).
    struct global_info *userdata = new struct global_info;
    userdata->path_to_cache = (char *)malloc(strlen(path_to_cache) + 1);
    strcpy(userdata->path_to_cache, path_to_cache);
    userdata->cache_interval = cache_interval;

    // TODO: set `ret_code` to 0 if everything above succeeded else some appropriate
    // non-zero value.
    *ret_code = 0;
    // Return pointer to global state data.
    return (void *)userdata;
}

void watdfs_cli_destroy(void *userdata) {
    // TODO: clean up your userdata state.
    free(userdata);
    // TODO: tear down the RPC library by calling `rpcClientDestroy`.
    rpcClientDestroy();
}

// GET FILE ATTRIBUTES
int watdfs_cli_getattr(void *userdata, const char *path, struct stat *statbuf) {
    // SET UP THE RPC CALL
    //DLOG("Received getattr call...");
    global_info *p = (global_info *)userdata;

    char *full_path = get_full_path(((global_info *)userdata)->path_to_cache, path);
    
    struct fuse_file_info *fi = (struct fuse_file_info *) malloc(sizeof(struct fuse_file_info));
    
    fi->flags = O_RDONLY;

    int fnx_ret = 0;
    if (file_is_open(userdata, path)) {
        if (p->files_info[path]->flags == O_RDONLY) {
            // TODO: read freshness
        }
        
        fxn_ret = stat(full_path, statbuf);

        if (fxn_ret < 0) {
            fxn_ret = -errno;
            memset(statbuf, 0, sizeof(struct stat));
        }

        free(full_path);
        free(fi);

        return fxn_ret;
    }
    else {
        fxn_ret = watdfs_cli_open(userdata, path, fi);
        if (fxn_ret < 0) {
            free(full_path);
            free(fi);
            return fxn_ret;
        }

        fxn_ret = stat(full_path, statbuf);
        
        if (fxn_ret < 0) {
            fxn_ret = -errno;
            memset(statbuf, 0, sizeof(struct stat));
        }

        fxn_ret = watdfs_cli_release(userdata, path, fi);

        free(fi);
        free(full_path);

        return fxn_ret;
    }

    // Finally return the value we got from the server.
    return fxn_ret;
}

int watdfs_cli_fgetattr(void *userdata, const char *path, struct stat *statbuf,
                        struct fuse_file_info *fi) {
    // At this point this method is not supported so it returns ENOSYS.
    return -ENOSYS;
}

// CREATE, OPEN AND CLOSE
int watdfs_cli_mknod(void *userdata, const char *path, mode_t mode, dev_t dev) {
    // Called to create a file.

    global_info *p = (global_info *)userdata;

    if (file_is_open(userdata, path)) {
        if (p->files_info[path]->flags == O_RDONLY) {
            return -EMFILE;
        }

        // TODO: Check freshness.
    }

    struct fuse_file_info *fi = (struct fuse_file_info *) malloc(sizeof(struct fuse_file_info));

    char *full_path = get_full_path(((struct global_info *)userdata)->path_to_cache, path);
    
    fi->flags = O_RDWR;

    int fxn_ret = watdfs_cli_open(userdata, path, fi);

    if (fxn_ret < 0) {
        if (fxn_ret == -EACCES) {
            // File already opened in write mode.
            free(fi);
            free(full_path);
            return fxn_ret;
        }

        // File does not exist on server.
        fxn_ret = watdfs_cli_mknod_rpc(userdata, path, mode, dev);

        if (fxn_ret < 0) {
            free(fi);
            free(full_path);
            return fxn_ret;
        }

        int sys_ret = mknod(full_path, mode, dev);
        
        if (sys_ret < 0) {  // Sys call failed.
            fxn_ret = -EINVAL;
        } else {
            fxn_ret = sys_ret;
        }

        free(full_path);
        free(fi);
        return fxn_ret;
    }
    else {
        // File exists on server.
        fxn_ret = watdfs_cli_release(useradata, path, fi);

        if (fxn_ret >= 0) {
            fxn_ret = -EEXIST;
        }

        free(fi);
        free(full_path);
        return fxn_ret;
    }
}

int watdfs_cli_open(void *userdata, const char *path,
                    struct fuse_file_info *fi) {
    
    if (file_is_open(userdata, path)) {
        return -EMFILE;
    }

    struct global_info *p = (global_info *)userdata;

    // Set flags accordingly.
    int server_flag = fi->flags & O_ACCMODE;

    int fxn_ret = watdfs_cli_open_rpc(userdata, path, fi);

    if (fxn_ret < 0) {
        // Rpc or server open call failed.
        return fxn_ret;
    }

    // Set file info structure.
    p->files_info[path] = new struct file_info;

    p->files_info[path]->fd_ser = fi->fh;

    fxn_ret = watdfs_cli_download(userdata, path, fi);

    fi->flags = server_flag;

    if (fxn_ret >= 0) {
        // Add file path to the set of opened files. This should be done when open succeeded.
        p->opened_files.insert(path);
        p->files_info[path]->flags = fi->flags & O_ACCMODE;
        p->files_info[path]->tc = time(nullptr);
    }

    return fxn_ret;
    // return -ENOSYS;
}

int watdfs_cli_release(void *userdata, const char *path,
                       struct fuse_file_info *fi) {
    
    int fxn_ret = 0;

    // If open in write mode, flush to server.
    if ((fi->flags & O_ACCMODE) != O_RDONLY) {
        fxn_ret = watdfs_cli_upload(userdata, path, fi);
        if (fxn_ret < 0) {
            return fxn_ret;
        }
    }

    fxn_ret = watdfs_cli_release_rpc(userdata, path, fi);

    if (fxn_ret < 0) {
        return fxn_ret;
    }

    struct global_info *p = (global_info *)userdata;

    // Close client file.
    int fd_cli = p->files_info[path]->fd_cli;
    fxn_ret = close(fd_cli);
    if (fxn_ret < 0) {
        return -errno;
    }

    // Update userdata.
    p->opened_files.erase(path);
    p->files_info.erase(path);

    return fxn_ret;
    // return -ENOSYS;
}

// READ AND WRITE DATA
int watdfs_cli_read(void *userdata, const char *path, char *buf, size_t size,
                    off_t offset, struct fuse_file_info *fi) {
    // Read size amount of data at offset of file into buf.
    struct global_info *p = (global_info *)userdata;
    int fd_cli = p->files_info[path]->fd_cli;
    int flag = p->files_info[path]->flags;

    if (flag == O_WRONLY) {
        return -EINVAL;
    }

    if (flag == O_RDONLY) {
        int fxn_ret = check_read_fresh(userdata, path);
        if (fxn_ret < 0) {
            return fxn_ret;
        }
    }

    int nread = pread(fd_cli, buf, size, offset);

    if (nread < 0) {
        nread = -errno;
    }
    
    return nread;
    // return -ENOSYS;
}
int watdfs_cli_write(void *userdata, const char *path, const char *buf,
                     size_t size, off_t offset, struct fuse_file_info *fi) {
    // Write size amount of data at offset of file from buf.
    struct global_info *p = (global_info *)userdata;
    int fd_cli = p->files_info[path]->fd_cli;
    int flags = p->files_info[path]->flags;

    if (flags == O_RDONLY) {
        return -EINVAL;
    }

    int fxn_ret = check_write_fresh(userdata, path);

    if (fxn_ret < 0) {
        return fxn_ret;
    }

    int nwrite = pwrite(fd_cli, buf, size, offset);

    if (nwrite < 0) {
        nwrite = -errno;
    }

    return nwrite;
    // return -ENOSYS;
}

int watdfs_cli_truncate(void *userdata, const char *path, off_t newsize) {
    // Change the file size to newsize.

    // "truncate can be called without opening the file, 
    // but should succeed if the file has the write permissions."
    // What does this mean?

    global_info *p = (global_info *)userdata;

    char *full_path = get_full_path(((global_info *)userdata)->path_to_cache, path);
    
    struct fuse_file_info *fi = (struct fuse_file_info *) malloc(sizeof(struct fuse_file_info));
    
    fi->flags = O_RDWR;
    
    if (!file_is_open(userdata, path)) {
        // Open file and then release.
        int fxn_ret = watdfs_cli_open(userdata, path, fi);
        if (fxn_ret < 0) {
            free(full_path);
            free(fi);
            return fxn_ret;
        }

        fxn_ret = truncate(full_path, newsize);

        if (fxn_ret < 0) {
            free(full_path);
            free(fi);
            return -errno;
        }

        fxn_ret = watdfs_cli_release(userdata, path, fi);
        
        free(full_path);
        free(fi);
        return fxn_ret;
    }

    if (p->files_info[path]->flags == O_RDONLY) {
        free(full_path);
        free(fi);
        return -EMFILE;
    }

    int fxn_ret = truncate(full_path, newsize);

    if (fxn_ret < 0) {
        fxn_ret = -errno;
    }

    // TODO: Write freshness.
    
    free(full_path);
    free(fi);
    return fxn_ret;
    // return -ENOSYS;
}

int watdfs_cli_fsync(void *userdata, const char *path,
                     struct fuse_file_info *fi) {
    // Force a flush of file data.
    int fxn_ret = watdfs_cli_upload(userdata, path, fi);

    if (fxn_ret < 0) {
        return fxn_ret;
    }

    // Update tc.
    ((global_info *)userdata)->files_info[path]->tc = time(nullptr);
    
    return fxn_ret;
    // return -ENOSYS;
}

// CHANGE METADATA
int watdfs_cli_utimens(void *userdata, const char *path,
                       const struct timespec ts[2]) {
    // Change file access and modification times.
    char *full_path = get_full_path(((global_info *)userdata)->path_to_cache, path);

    global_info *p = (global_info *)userdata;

    struct fuse_file_info *fi = (struct fuse_file_info *) malloc(sizeof(struct fuse_file_info));
    
    fi->flags = O_RDWR;

    if (!file_is_open(userdata, path)) {
        int fxn_ret = watdfs_cli_open(userdata, path, fi);
        if (fxn_ret < 0) {
            free(fi);
            free(full_path);
            return fxn_ret;
        }

        fxn_ret = utimensat(0, full_path, ts, 0);

        if (fxn_ret < 0) {
            free(fi);
            free(full_path);
            return -errno;
        }

        fxn_ret = watdfs_cli_release(userdata, path, fi);

        free(fi);
        free(full_path);
        return fxn_ret;
    }

    int flag = p->files_info[path]->flags;

    if (flag == O_RDONLY) {
        free(full_path);
        free(fi);
        delete statbuf;
        return -EMFILE;
    }

    fxn_ret = utimensat(0, full_path, ts, 0);

    if (fxn_ret < 0) {
        fxn_ret = -errno;
    }

    free(full_path);
    free(fi);
    delete statbuf;
    return fxn_ret;
    // return -ENOSYS;
}
