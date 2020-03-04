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

struct file_info {
    int fd_cli; // fd for client
    int fd_ser; // fd for server
    int flags; // open mode
    time_t tc;
};

struct global_info {
    char *path_to_cache; // The path to cache file in client.
    int cache_interval;
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
    
    // getattr has 3 arguments.
    int ARG_COUNT = 3;

    // Allocate space for the output arguments.
    void **args = (void **)malloc(ARG_COUNT * sizeof(void *));

    // Allocate the space for arg types, and one extra space for the null
    // array element.
    int arg_types[ARG_COUNT + 1];

    // The path has string length (strlen) + 1 (for the null character).
    int pathlen = strlen(path) + 1;

    // Fill in the arguments
    // The first argument is the path, it is an input only argument, and a char
    // array. The length of the array is the length of the path.
    arg_types[0] =
        (1u << ARG_INPUT) | (1u << ARG_ARRAY) | (ARG_CHAR << 16u) | (uint) pathlen;
    // For arrays the argument is the array pointer, not a pointer to a pointer.
    args[0] = (void *)path;

    // The second argument is the stat structure. This argument is an output
    // only argument, and we treat it as a char array. The length of the array
    // is the size of the stat structure, which we can determine with sizeof.
    arg_types[1] = (1u << ARG_OUTPUT) | (1u << ARG_ARRAY) | (ARG_CHAR << 16u) |
                   (uint) sizeof(struct stat); // statbuf
    args[1] = (void *)statbuf;

    // The third argument is the return code, an output only argument, which is
    // an integer.
    // TODO: fill in this argument type.
    arg_types[2] = (1u << ARG_OUTPUT) | (ARG_INT << 16u);

    // The return code is not an array, so we need to hand args[2] an int*.
    // The int* could be the address of an integer located on the stack, or use
    // a heap allocated integer, in which case it should be freed.
    // TODO: Fill in the argument
    int retcode;
    args[2] = (void *)&retcode;

    // Finally, the last position of the arg types is 0. There is no
    // corresponding arg.
    arg_types[3] = 0;

    // MAKE THE RPC CALL
    int rpc_ret = rpcCall((char *)"getattr", arg_types, args);

    // HANDLE THE RETURN
    // The integer value watdfs_cli_getattr will return.
    int fxn_ret = 0;
    if (rpc_ret < 0) {
        // Something went wrong with the rpcCall, return a sensible return
        // value. In this case lets return, -EINVAL
        fxn_ret = -EINVAL;
    } else {
        // Our RPC call succeeded. However, it's possible that the return code
        // from the server is not 0, that is it may be -errno. Therefore, we
        // should set our function return value to the retcode from the server.
        // TODO: set the function return value to the return code from the server.
        fxn_ret = retcode;
    }

    if (fxn_ret < 0) {
        // Important: if the return code of watdfs_cli_getattr is negative (an
        // error), then we need to make sure that the stat structure is filled
        // with 0s. Otherwise, FUSE will be confused by the contradicting return
        // values.
        memset(statbuf, 0, sizeof(struct stat));
    }

    // Clean up the memory we have allocated.
    free(args);

    // Finally return the value we got from the server.
    return fxn_ret;
}

int watdfs_cli_fgetattr(void *userdata, const char *path, struct stat *statbuf,
                        struct fuse_file_info *fi) {
    // At this point this method is not supported so it returns ENOSYS.
    int ARG_COUNT = 4;

    void **args = (void **)malloc(ARG_COUNT * sizeof(void *));

    int arg_types[ARG_COUNT + 1];

    int pathlen = strlen(path) + 1;

    int filen = sizeof(struct fuse_file_info);

    int retcode;

    arg_types[0] =
        (1u << ARG_INPUT) | (1u << ARG_ARRAY) | (ARG_CHAR << 16u) | (uint) pathlen;
    args[0] = (void *)path;

    arg_types[1] = 
        (1u << ARG_OUTPUT) | (1u << ARG_ARRAY) | (ARG_CHAR << 16u) | (uint) sizeof(struct stat); // statbuf
    args[1] = (void *)statbuf;

    arg_types[2] = 
        (1u << ARG_INPUT) | (1u << ARG_ARRAY) | (ARG_CHAR << 16u) | (uint) filen;
    args[2] = (void *)fi;

    arg_types[3] = (1u << ARG_OUTPUT) | (ARG_INT << 16u);
    args[3] = (void*)&retcode;

    arg_types[4] = 0;

    int rpc_ret = rpcCall((char *)"fgetattr", arg_types, args);

    int fxn_ret = 0;

    if (rpc_ret < 0) {
        fxn_ret = -EINVAL;
    } else {
        fxn_ret = retcode;
    }

    if (fxn_ret < 0) {
        memset(statbuf, 0, sizeof(struct stat));
    }

    free(args);

    return fxn_ret;
    // return -ENOSYS;
}

// CREATE, OPEN AND CLOSE
int watdfs_cli_mknod(void *userdata, const char *path, mode_t mode, dev_t dev) {
    // Called to create a file.

    if (file_is_open(userdata, path)) {

    }

    int fxn_ret = watdfs_cli_mknod_rpc(userdata, path, mode, dev);
    // The rpc call or the server mknod call failed.
    if (fxn_ret < 0) {
        return fxn_ret;
    }

    // Create local file.
    char *full_path = get_full_path(((struct global_info *)userdata)->path_to_cache, path);
    
    int sys_ret = mknod(full_path, mode, dev);
    
    if (sys_ret < 0) {  // Sys call failed.
        fxn_ret = -EINVAL;
    } else {
        fxn_ret = sys_ret;
    }

    free(full_path);
    
    // return -ENOSYS;
    return fxn_ret;
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

    // Set the fi flag for read and write, to transfer the file in download.
    fi->flags = O_RDWR;

    fxn_ret = watdfs_cli_download(userdata, path, fi);

    fi->flags = server_flag;

    if (fxn_ret >= 0) {
        // Add file path to the set of opened files. This should be done when open succeeded.
        (p->opened_files.insert(path);
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

    char *full_path = get_full_path(((global_info *)userdata)->path_to_cache, path);
    
    struct fuse_file_info *fi = (struct fuse_file_info *) malloc(sizeof(struct fuse_file_info));
    
    fi->flags = O_RDWR;
    
    if (!file_is_open(userdata, path)) {
        // Open file and then release.
        int fxn_ret = watdfs_cli_open(userdata, path, fi);
        if (fxn_ret < 0) {
            return fxn_ret;
        }

        fxn_ret = truncate(full_path, newsize);

        if (fxn_ret < 0) {
            return -errno;
        }

        fxn_ret = watdfs_cli_release(userdata, path, fi);

        return fxn_ret;
    }

    int fxn_ret = truncate(full_path, newsize);

    if (fxn_ret < 0) {
        if (errno == EINVAL) {
            // The file is not in write mode.
            return -EMFILE;
        }
        return -errno;
    }

    return fxn_ret;
    // return -ENOSYS;
}

int watdfs_cli_fsync(void *userdata, const char *path,
                     struct fuse_file_info *fi) {
    // Force a flush of file data.
    int fxn_ret = watdfs_cli_upload(userdata, path, fi);

    return fxn_ret;
    // return -ENOSYS;
}

// CHANGE METADATA
int watdfs_cli_utimens(void *userdata, const char *path,
                       const struct timespec ts[2]) {
    // Change file access and modification times.
    char *full_path = get_full_path(((global_info *)userdata)->path_to_cache, path);

    struct fuse_file_info *fi = (struct fuse_file_info *) malloc(sizeof(struct fuse_file_info));
    
    fi->flags = O_RDWR;

    if (!file_is_open(userdata, path)) {
        int fxn_ret = watdfs_cli_open(userdata, path, fi);
        if (fxn_ret < 0) {
            return fxn_ret;
        }

        fxn_ret = utimensat(0, full_path, ts, 0);

        if (fxn_ret < 0) {
            return -errno;
        }

        fxn_ret = watdfs_cli_release(userdata, path, fi);

        return fxn_ret;
    }

    struct stat *statbuf = new struct stat;
    int fxn_ret = watdfs_cli_getattr_rpc(userdata, path, statbuf);

    if (fxn_ret < 0) {
        // Getattr rpc call failed.
        DLOG("Getattr rpc call failed.\n");
        free(full_path);
        delete statbuf;
        return sys_ret;
    }

    int flag = statbuf->st_mode;

    if (flag == O_RDONLY) {
        return -EMFILE;
    }

    fxn_ret = utimensat(0, full_path, ts, 0);

    if (fxn_ret < 0) {
        fxn_ret = -errno;
    }

    return fxn_ret;
    // return -ENOSYS;
}
