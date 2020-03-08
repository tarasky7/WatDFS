//
// Starter code for CS 454/654
// You SHOULD change this file
//

// *****
// Don't forget to delete the include file.
#include "watdfs_client.h"
// *****
#include "rpc.h"
#include "debug.h"
INIT_LOG

#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include <cstring>
#include <cstdlib>

#include <fuse.h>

#include <map>

struct file_status {
    int open_mode;  // 0 for read, 1 for write.
    int open_number;    // Number of files opened.
    rw_lock_t *lock;
}

std::map<const char *, struct file_status *> files_status;

// Global state server_persist_dir.
char *server_persist_dir = nullptr;

// Important: the server needs to handle multiple concurrent client requests.
// You have to be carefuly in handling global variables, esp. for updating them.
// Hint: use locks before you update any global variable.

// We need to operate on the path relative to the the server_persist_dir.
// This function returns a path that appends the given short path to the
// server_persist_dir. The character array is allocated on the heap, therefore
// it should be freed after use.
// Tip: update this function to return a unique_ptr for automatic memory management.
char *get_full_path(char *short_path) {
    int short_path_len = strlen(short_path);
    int dir_len = strlen(server_persist_dir);
    int full_len = dir_len + short_path_len + 1;

    char *full_path = (char *)malloc(full_len);

    // First fill in the directory.
    strcpy(full_path, server_persist_dir);
    // Then append the path.
    strcat(full_path, short_path);
    DLOG("Full path: %s\n", full_path);

    return full_path;
}

// The server implementation of getattr.
int watdfs_getattr(int *argTypes, void **args) {
    // Get the arguments.
    // The first argument is the path relative to the mountpoint.
    char *short_path = (char *)args[0];
    // The second argument is the stat structure, which should be filled in
    // by this function.
    struct stat *statbuf = (struct stat *)args[1];
    // The third argument is the return code, which should be set be 0 or -errno.
    int *ret = (int *)args[2];

    // Get the local file name, so we call our helper function which appends
    // the server_persist_dir to the given path.
    char *full_path = get_full_path(short_path);

    // Initially we set set the return code to be 0.
    *ret = 0;

    // TODO: Make the stat system call, which is the corresponding system call needed
    // to support getattr. You should use the statbuf as an argument to the stat system call.
    (void)statbuf;

    // Let sys_ret be the return code from the stat system call.
    int sys_ret = stat(full_path, statbuf);

    if (sys_ret < 0) {
        // If there is an error on the system call, then the return code should
        // be -errno.
        *ret = -errno;
    }

    // Clean up the full path, it was allocated on the heap.
    free(full_path);

    //DLOG("Returning code: %d", *ret);
    // The RPC call succeeded, so return 0.
    return 0;
}

int watdfs_fgetattr(int *argTypes, void **args) {
    char *short_path = (char *)args[0];
    struct stat *statbuf = (struct stat *)args[1];
    struct fuse_file_info *fi = (struct fuse_file_info *)args[2];
    int *ret = (int *)args[3];

    char *full_path = get_full_path(short_path);

    *ret = 0;

    int sys_ret = fstat(fi->fh, statbuf);

    if (sys_ret < 0) {
        *ret = -errno;
    }

    free(full_path);

    return 0;
}

int watdfs_mknod(int *argTypes, void **args) {
    char *short_path = (char *)args[0];
    mode_t mode = *(mode_t *)args[1];
    dev_t dev = *(dev_t *)args[2];
    int *ret = (int *)args[3];

    char *full_path = get_full_path(short_path);

    *ret = 0;

    DLOG("mode is %d, dev is %ld\n", mode ,dev);

    int sys_ret = mknod(full_path, mode, dev);

    if (sys_ret < 0) {
        DLOG("ret is %d\n", -errno);
        *ret = -errno;
    }

    free(full_path);

    return 0;
}

int watdfs_open(int *argTypes, void **args) {
    char *short_path = (char *)args[0];
    struct fuse_file_info *fi = (struct fuse_file_info *)args[1];
    int *ret = (int *)args[2];

    char *full_path = get_full_path(short_path);

    *ret = 0;

    if (files_status.find(short_path) != files_status.end() && files_status[short_path]->open_number > 0) {
        // File has been opened.
        int status = files_status[short_path]->open_mode;
        if (status == 0) {
            // Open in read mode.
            if (fi->flags & O_ACCMODE != O_RDONLY) {
                // Current open file in write mode.
                files_status[short_path]->open_mode = 1;
            }
        }
        else {
            if (fi->flags & O_ACCMODE != O_RDONLY) {
                // Conflict.
                free(full_path);
                *ret = -EACCES;
                return 0;
            }
        }
        files_status[short_path]->open_number += 1;
    }
    else {
        // File has not been opened.
        if (files_status.find(short_path) == files_status.end()) {
            files_status[short_path] = new struct file_status;   
            files_status[short_path]->lock = new rw_lock_t;
            // Init lock.
            rw_lock_init(files_status[short_path]->lock); 
        }

        if (fi->flags & O_ACCMODE == O_RDONLY) {
            files_status[short_path]->open_mode = 0;
        }
        else {
            files_status[short_path]->open_mode = 1;
        }

        files_status[short_path]->open_number = 1;
    }

    fi->fh = open(full_path, fi->flags);

    if (fi->fh < 0) {
        *ret = -errno;
    }

    free(full_path);

    return 0;
}

int watdfs_release(int *argTypes, void **args) {
    char *short_path = (char *)args[0];
    struct fuse_file_info *fi = (struct fuse_file_info *)args[1];
    int *ret = (int *)args[2];

    char *full_path = get_full_path(short_path);

    *ret = 0;

    int sys_ret = close(fi->fh);

    if (sys_ret < 0) {
        *ret = -errno;
    }
    else {
        // Only update open number when close call succeed.
        files_status[short_path]->open_number -= 1;
    }

    if (files_status[short_path]->open_number == 0) {
        // Release lock.
        rw_lock_destroy(files_status[short_path]->lock);

        // Delete file_status
        delete files_status[short_path]->lock;
        delete files_status[short_path];
    }

    free(full_path);

    return 0;
}

int watdfs_write(int *argTypes, void **args) {
    char *short_path = (char *)args[0];
    char *buf = (char *)args[1];
    size_t size = *(size_t *)args[2];
    off_t offset = *(off_t *)args[3];
    struct fuse_file_info *fi = (struct fuse_file_info *)args[4];
    int *ret = (int *)args[5];

    DLOG("buf on server is %s, size is %d, offset is %d\n", buf, (int)size, (int)offset);

    char *full_path = get_full_path(short_path);

    *ret = 0;

    int sys_ret = pwrite(fi->fh, buf, size, offset);

    if (sys_ret < 0) {
        *ret = -errno;
    }

    DLOG("written is %d, ret is %d\n", sys_ret, *ret);
    // Set ret to the number of bytes written.
    *ret = sys_ret;

    free(full_path);

    return 0;
}

int watdfs_read(int *argTypes, void **args) {
    char *short_path = (char *)args[0];
    char *buf = (char *)args[1];
    size_t size = *(size_t *)args[2];
    off_t offset = *(off_t *)args[3];
    struct fuse_file_info *fi = (struct fuse_file_info *)args[4];
    int *ret = (int *)args[5];

    char *full_path = get_full_path(short_path);

    *ret = 0;

    int sys_ret = pread(fi->fh, buf, size, offset);

    DLOG("errno is %d\n", errno);
    
    if (sys_ret < 0) {
        *ret = -errno;
    }

    // Set ret to the number of bytes read.
    *ret = sys_ret;

    free(full_path);

    return 0;
}

int watdfs_truncate(int *argTypes, void **args) {
    char *short_path = (char *)args[0];
    off_t size = *(off_t *)args[1];
    int *ret = (int *)args[2];
    
    char *full_path = get_full_path(short_path);

    *ret = 0;

    int sys_ret = truncate(full_path, size);

    if (sys_ret < 0) {
        *ret = -errno;
    }

    free(full_path);

    return 0;
}

int watdfs_fsync(int *argTypes, void **args) {
    char *short_path = (char *)args[0];
    struct fuse_file_info *fi = (struct fuse_file_info *)args[1];
    int *ret = (int *)args[2];

    char *full_path = get_full_path(short_path);

    *ret = 0;

    int sys_ret = fsync(fi->fh);

    if (sys_ret < 0) {
        *ret = -errno;
    }

    free(full_path);

    return 0;
}

int watdfs_utimens(int *argTypes, void **args) {
    char *short_path = (char *)args[0];
    // Might cause problem using this def.
    // The exact ts structure should be timespec[2].
    struct timespec *ts = (struct timespec *)args[1];
    
    int *ret = (int *)args[2];

    char *full_path = get_full_path(short_path);

    *ret = 0;

    int sys_ret = utimensat(AT_FDCWD, full_path, ts, 0);

    if (sys_ret < 0) {
        *ret = -errno;
    }

    DLOG("ret is %d\n", *ret);
    free(full_path);

    return 0;
}

int watdfs_lock(int *argTypes, void **args) {
    char *short_path = (char *)args[0];
    rw_lock_mode_t *mode = (rw_lock_mode_t *)args[1];
    int *ret = (int *)args[2];

    char *full_path = get_full_path(short_path);

    *ret = 0;

    int sys_ret = rw_lock_lock(files_status[short_path]->lock, *mode);

    if (sys_ret < 0) {
        *ret = -errno;
    }
    
    free(full_path);

    return 0;
}

int watdfs_unlock(int *argTypes, void **args) {
    char *short_path = (char *)args[0];
    rw_lock_mode_t *mode = (rw_lock_mode_t *)args[1];
    int *ret = (int *)args[2];

    char *full_path = get_full_path(short_path);

    *ret = 0;

    int sys_ret = rw_lock_unlock(files_status[short_path]->lock, *mode);

    if (sys_ret < 0) {
        *ret = -errno;
    }
    
    free(full_path);

    return 0;
}

// The main function of the server.
int main(int argc, char *argv[]) {
    // argv[1] should contain the directory where you should store data on the
    // server. If it is not present it is an error, that we cannot recover from.
    if (argc != 2) {
        // In general you shouldn't print to stderr or stdout, but it may be
        // helpful here for debugging. Important: Make sure you turn off logging
        // prior to submission!
        // See watdfs_client.c for more details
        // # ifdef PRINT_ERR
        // std::cerr << "Usaage:" << argv[0] << " server_persist_dir";
        // #endif
        return -1;
    }
    // Store the directory in a global variable.
    server_persist_dir = argv[1];

    // TODO: Initialize the rpc library by calling `rpcServerInit`.
    // Important: `rpcServerInit` prints the 'export SERVER_ADDRESS' and
    // 'export SERVER_PORT' lines. Make sure you *do not* print anything
    // to *stdout* before calling `rpcServerInit`.
    //DLOG("Initializing server...");
    rpcServerInit();

    int ret = 0;
    // TODO: If there is an error with `rpcServerInit`, it maybe useful to have
    // debug-printing here, and then you should return.

    // TODO: Register your functions with the RPC library.
    // Note: The braces are used to limit the scope of `argTypes`, so that you can
    // reuse the variable for multiple registrations. Another way could be to
    // remove the braces and use `argTypes0`, `argTypes1`, etc.
    {
        // There are 3 args for the function (see watdfs_client.c for more
        // detail).
        int argTypes[4];
        // First is the path.
        argTypes[0] =
            (1u << ARG_INPUT) | (1u << ARG_ARRAY) | (ARG_CHAR << 16u) | 1u;
        // The second argument is the statbuf.
        argTypes[1] =
            (1u << ARG_OUTPUT) | (1u << ARG_ARRAY) | (ARG_CHAR << 16u) | 1u;
        // The third argument is the retcode.
        argTypes[2] = (1u << ARG_OUTPUT) | (ARG_INT << 16u);
        // Finally we fill in the null terminator.
        argTypes[3] = 0;

        // We need to register the function with the types and the name.
        ret = rpcRegister((char *)"getattr", argTypes, watdfs_getattr);
        if (ret < 0) {
            // It may be useful to have debug-printing here.
            return ret;
        }
    }

    {
        int argTypes[5];
        
        argTypes[0] = 
            (1u << ARG_INPUT) | (1u << ARG_ARRAY) | (ARG_CHAR << 16u) | 1u;

        argTypes[1] = (1u << ARG_INPUT) | (ARG_INT << 16u);

        argTypes[2] = (1u << ARG_INPUT) | (ARG_LONG << 16u);

        argTypes[3] = (1u << ARG_OUTPUT) | (ARG_INT << 16u);

        argTypes[4] = 0;

        ret = rpcRegister((char *)"mknod", argTypes, watdfs_mknod);

        if (ret < 0) {
            // It may be useful to have debug-printing here.
            return ret;
        }
    }

    {
        int argTypes[4];
        
        argTypes[0] = 
            (1u << ARG_INPUT) | (1u << ARG_ARRAY) | (ARG_CHAR << 16u) | 1u;

        argTypes[1] = (1u << ARG_INPUT) | (1u << ARG_OUTPUT) | (1u << ARG_ARRAY) |
            (ARG_CHAR << 16u) | 1u;

        argTypes[2] = (1u << ARG_OUTPUT) | (ARG_INT << 16u);

        argTypes[3] = 0;

        ret = rpcRegister((char *)"open", argTypes, watdfs_open);

        if (ret < 0) {
            // It may be useful to have debug-printing here.
            return ret;
        }
    }

    {
        int argTypes[4];
        
        argTypes[0] = 
            (1u << ARG_INPUT) | (1u << ARG_ARRAY) | (ARG_CHAR << 16u) | 1u;

        argTypes[1] = 
            (1u << ARG_INPUT) | (1u << ARG_ARRAY) | (ARG_CHAR << 16u) | 1u;

        argTypes[2] = (1u << ARG_OUTPUT) | (ARG_INT << 16u);

        argTypes[3] = 0;

        ret = rpcRegister((char *)"release", argTypes, watdfs_release);

        if (ret < 0) {
            // It may be useful to have debug-printing here.
            return ret;
        }
    }

    {
        int argTypes[7];
        
        argTypes[0] = 
            (1u << ARG_INPUT) | (1u << ARG_ARRAY) | (ARG_CHAR << 16u) | 1u;

        argTypes[1] = 
            (1u << ARG_OUTPUT) | (1u << ARG_ARRAY) | (ARG_CHAR << 16u) | 1u;

        argTypes[2] = (1u << ARG_INPUT) | (ARG_LONG << 16u);

        argTypes[3] = (1u << ARG_INPUT) | (ARG_LONG << 16u);

        argTypes[4] = 
            (1u << ARG_INPUT) | (1u << ARG_ARRAY) | (ARG_CHAR << 16u) | 1u;
        
        argTypes[5] = (1u << ARG_OUTPUT) | (ARG_INT << 16u);

        argTypes[6] = 0;

        ret = rpcRegister((char *)"read", argTypes, watdfs_read);

        if (ret < 0) {
            // It may be useful to have debug-printing here.
            return ret;
        }
    }

    {
        int argTypes[7];
        
        argTypes[0] = 
            (1u << ARG_INPUT) | (1u << ARG_ARRAY) | (ARG_CHAR << 16u) | 1u;

        argTypes[1] = 
            (1u << ARG_INPUT) | (1u << ARG_ARRAY) | (ARG_CHAR << 16u) | 1u;

        argTypes[2] = (1u << ARG_INPUT) | (ARG_LONG << 16u);

        argTypes[3] = (1u << ARG_INPUT) | (ARG_LONG << 16u);

        argTypes[4] = 
            (1u << ARG_INPUT) | (1u << ARG_ARRAY) | (ARG_CHAR << 16u) | 1u;
        
        argTypes[5] = (1u << ARG_OUTPUT) | (ARG_INT << 16u);

        argTypes[6] = 0;

        ret = rpcRegister((char *)"write", argTypes, watdfs_write);

        if (ret < 0) {
            // It may be useful to have debug-printing here.
            return ret;
        }
    }

    {
        int argTypes[4];

        argTypes[0] = 
            (1u << ARG_INPUT) | (1u << ARG_ARRAY) | (ARG_CHAR << 16u) | 1u;
        
        argTypes[1] = (1u << ARG_INPUT) | (ARG_LONG << 16u);

        argTypes[2] = (1u << ARG_OUTPUT) | (ARG_INT << 16u);

        argTypes[3] = 0;

        ret = rpcRegister((char *)"truncate", argTypes, watdfs_truncate);

        if (ret < 0) {
            // It may be useful to have debug-printing here.
            return ret;
        }
    }

    {
        int argTypes[4];

        argTypes[0] = 
            (1u << ARG_INPUT) | (1u << ARG_ARRAY) | (ARG_CHAR << 16u) | 1u;
        
        argTypes[1] = 
            (1u << ARG_INPUT) | (1u << ARG_ARRAY) | (ARG_CHAR << 16u) | 1u;

        argTypes[2] = (1u << ARG_OUTPUT) | (ARG_INT << 16u);

        argTypes[3] = 0;

        ret = rpcRegister((char *)"fsync", argTypes, watdfs_fsync);

        if (ret < 0) {
            // It may be useful to have debug-printing here.
            return ret;
        }
    }

    {
        int argTypes[4];

        argTypes[0] = 
            (1u << ARG_INPUT) | (1u << ARG_ARRAY) | (ARG_CHAR << 16u) | 1u;
        
        argTypes[1] = 
            (1u << ARG_INPUT) | (1u << ARG_ARRAY) | (ARG_CHAR << 16u) | 1u;

        argTypes[2] = (1u << ARG_OUTPUT) | (ARG_INT << 16u);

        argTypes[3] = 0;

        ret = rpcRegister((char *)"utimens", argTypes, watdfs_utimens);

        if (ret < 0) {
            // It may be useful to have debug-printing here.
            return ret;
        }
    }

    {
        int argTypes[4];

        argTypes[0] = (1 << ARG_INPUT) | (1 << ARG_ARRAY) | (ARG_CHAR << 16) | 1u;
        
        argTypes[1] = (1 << ARG_INPUT) | (ARG_INT << 16);
        
        argTypes[2] = (1 << ARG_OUTPUT) | (ARG_INT << 16);
        
        argTypes[3] = 0;

        ret = rpcRegister((char *)"lock", argTypes, watdfs_lock);

        if (ret < 0) {
            // It may be useful to have debug-printing here.
            return ret;
        }
    }

    {
        int argTypes[4];

        argTypes[0] = (1 << ARG_INPUT) | (1 << ARG_ARRAY) | (ARG_CHAR << 16) | 1u;
        
        argTypes[1] = (1 << ARG_INPUT) | (ARG_INT << 16);
        
        argTypes[2] = (1 << ARG_OUTPUT) | (ARG_INT << 16);
        
        argTypes[3] = 0;

        ret = rpcRegister((char *)"unlock", argTypes, watdfs_unlock);

        if (ret < 0) {
            // It may be useful to have debug-printing here.
            return ret;
        }
    }

    // TODO: Hand over control to the RPC library by calling `rpcExecute`.
    rpcExecute();

    // rpcExecute could fail so you may want to have debug-printing here, and
    // then you should return.
    return ret;
}
