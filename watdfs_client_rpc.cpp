//
// Starter code for CS 454/654
// You SHOULD change this file
//

#include "watdfs_client.h"
#include "debug.h"
INIT_LOG

#include "rpc.h"

// GET FILE ATTRIBUTES
int watdfs_cli_getattr_rpc(void *userdata, const char *path, struct stat *statbuf) {
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

int watdfs_cli_fgetattr_rpc(void *userdata, const char *path, struct stat *statbuf,
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
int watdfs_cli_mknod_rpc(void *userdata, const char *path, mode_t mode, dev_t dev) {
    // Called to create a file.
    int ARG_COUNT = 4;

    void **args = (void **)malloc(ARG_COUNT * sizeof(void *));

    int arg_types[ARG_COUNT + 1];

    int pathlen = strlen(path) + 1;

    int retcode;
    
    arg_types[0] =
        (1u << ARG_INPUT) | (1u << ARG_ARRAY) | (ARG_CHAR << 16u) | (uint) pathlen;
    args[0] = (void *)path;

    arg_types[1] = (1u << ARG_INPUT) | (ARG_INT << 16u);
    args[1] = (void *)&mode;

    arg_types[2] = (1u << ARG_INPUT) | (ARG_LONG << 16u);
    args[2] = (void *)&dev;

    arg_types[3] = (1u << ARG_OUTPUT) | (ARG_INT << 16u);
    args[3] = (void *)&retcode;

    arg_types[4] = 0;

    int rpc_ret = rpcCall((char *)"mknod", arg_types, args);

    int fxn_ret = 0;
    if (rpc_ret < 0) {
        fxn_ret = -EINVAL;
    } else {
        fxn_ret = retcode;
    }

    free(args);

    // return -ENOSYS;
    return fxn_ret;
}
int watdfs_cli_open_rpc(void *userdata, const char *path,
                    struct fuse_file_info *fi) {
    // Called during open.
    // You should fill in fi->fh.
    int ARG_COUNT = 3;

    void **args = (void **)malloc(ARG_COUNT * sizeof(void *));

    int arg_types[ARG_COUNT + 1];

    int pathlen = strlen(path) + 1;

    int filen = sizeof(struct fuse_file_info);

    int retcode;

    arg_types[0] =
        (1u << ARG_INPUT) | (1u << ARG_ARRAY) | (ARG_CHAR << 16u) | (uint) pathlen;
    args[0] = (void *)path;

    arg_types[1] = 
        (1u << ARG_INPUT) | (1u << ARG_OUTPUT) | (1u << ARG_ARRAY) | (ARG_CHAR << 16u) | (uint) filen;
    args[1] = (void *)fi;

    arg_types[2] = (1u << ARG_OUTPUT) | (ARG_INT << 16u);
    args[2] = (void*)&retcode;

    arg_types[3] = 0;

    int rpc_ret = rpcCall((char *)"open", arg_types, args);

    int fxn_ret = 0;

    if (rpc_ret < 0) {
        fxn_ret = -EINVAL;
    } else {
        fxn_ret = retcode;
    }

    free(args);

    return fxn_ret;
    // return -ENOSYS;
}

int watdfs_cli_release_rpc(void *userdata, const char *path,
                       struct fuse_file_info *fi) {
    // Called during close, but possibly asynchronously.
    int ARG_COUNT = 3;

    void **args = (void **)malloc(ARG_COUNT * sizeof(void *));

    int arg_types[ARG_COUNT + 1];

    int pathlen = strlen(path) + 1;

    int filen = sizeof(struct fuse_file_info);

    int retcode;

    arg_types[0] =
        (1u << ARG_INPUT) | (1u << ARG_ARRAY) | (ARG_CHAR << 16u) | (uint) pathlen;
    args[0] = (void *)path;

    arg_types[1] = 
        (1u << ARG_INPUT) | (1u << ARG_ARRAY) | (ARG_CHAR << 16u) | (uint) filen;
    args[1] = (void *)fi;

    arg_types[2] = (1u << ARG_OUTPUT) | (ARG_INT << 16u);
    args[2] = (void*)&retcode;

    arg_types[3] = 0;

    int rpc_ret = rpcCall((char *)"release", arg_types, args);

    int fxn_ret = 0;

    if (rpc_ret < 0) {
        fxn_ret = -EINVAL;
    } else {
        fxn_ret = retcode;
    }

    free(args);

    return fxn_ret;
    // return -ENOSYS;
}

// READ AND WRITE DATA
int watdfs_cli_read_rpc(void *userdata, const char *path, char *buf, size_t size,
                    off_t offset, struct fuse_file_info *fi) {
    // Read size amount of data at offset of file into buf.

    // Remember that size may be greater then the maximum array size of the RPC
    // library.

    int ARG_COUNT = 6;

    void **args = (void **)malloc(ARG_COUNT * sizeof(void *));

    int arg_types[ARG_COUNT + 1];

    int pathlen = strlen(path) + 1;

    char *bufLocal = buf;
    int buflen = 1;

    size_t nleft = size;

    size_t rpcSize = 0;   // Size param for rpcCall.

    int filen = sizeof(struct fuse_file_info);

    int retcode;

    int nread = 0;  // Number of bytes read from rpcCall.

    DLOG("bufSize is %d, readSize is %d\n", buflen, (int)size);

    arg_types[0] =
        (1u << ARG_INPUT) | (1u << ARG_ARRAY) | (ARG_CHAR << 16u) | (uint) pathlen;
    args[0] = (void *)path;

    arg_types[1] = 
        (1u << ARG_OUTPUT) | (1u << ARG_ARRAY) | (ARG_CHAR << 16u) | (uint) buflen;
    args[1] = (void *)bufLocal;

    arg_types[2] = (1u << ARG_INPUT) | (ARG_LONG << 16u);
    args[2] = (void *)&rpcSize;

    arg_types[3] = (1u << ARG_INPUT) | (ARG_LONG << 16u);
    args[3] = (void *)&offset;

    arg_types[4] = 
        (1u << ARG_INPUT) | (1u << ARG_ARRAY) | (ARG_CHAR << 16u) | (uint) filen;
    args[4] = (void *)fi;

    arg_types[5] = (1u << ARG_OUTPUT) | (ARG_INT << 16u);
    args[5] = (void*)&retcode;

    arg_types[6] = 0;

    while (nleft > 0) {
        rpcSize = (nleft > MAX_ARRAY_LEN ? MAX_ARRAY_LEN : nleft);
        buflen = rpcSize;
        arg_types[1] = (1u << ARG_OUTPUT) | (1u << ARG_ARRAY) | (ARG_CHAR << 16u) | (uint) buflen;
        args[1] = (void *)bufLocal;

        int rpc_ret = rpcCall((char *)"read", arg_types, args);
        
        if (rpc_ret < 0) {  // RpcCall failed.
            nread = -EINVAL;
            break;
        } else {
            nread = retcode;
        }

        DLOG("bufSize is %d, nleft is %d, rpcSize is %d, size is %d, read is %d\n", buflen, (int)nleft, (int)rpcSize, (int)size, nread);

        if (nread == 0 || nread < (int)rpcSize) {   // EOF, return the number of bytes actually read.
            nleft -= nread;
            break;
        }
        
        nleft -= nread;

        bufLocal += nread;

        // Update offset.
        offset += nread;
    }

    DLOG("after readSize is %d, nleft is %d, size is %d\n", nread, (int)nleft, (int)size);

    if (nread < 0) {
        return nread;
    }

    return (size - nleft);
    // return -ENOSYS;
}
int watdfs_cli_write_rpc(void *userdata, const char *path, const char *buf,
                     size_t size, off_t offset, struct fuse_file_info *fi) {
    // Write size amount of data at offset of file from buf.

    // Remember that size may be greater then the maximum array size of the RPC
    // library.
    int ARG_COUNT = 6;

    void **args = (void **)malloc(ARG_COUNT * sizeof(void *));

    int arg_types[ARG_COUNT + 1];

    int pathlen = strlen(path) + 1;

    const char *bufLocal = buf;

    int buflen = 1;

    size_t nleft = size;

    size_t rpcSize = 0;   // Size param for rpcCall.

    int filen = sizeof(struct fuse_file_info);

    int retcode;

    int nwrite = 0;  // Number of bytes written into rpcCall.

    DLOG("buf is %s\n", buf);

    arg_types[0] =
        (1u << ARG_INPUT) | (1u << ARG_ARRAY) | (ARG_CHAR << 16u) | (uint) pathlen;
    args[0] = (void *)path;

    arg_types[1] = 
        (1u << ARG_INPUT) | (1u << ARG_ARRAY) | (ARG_CHAR << 16u) | (uint) buflen;
    args[1] = (void *)bufLocal;

    arg_types[2] = (1u << ARG_INPUT) | (ARG_LONG << 16u);
    args[2] = (void *)&rpcSize;

    arg_types[3] = (1u << ARG_INPUT) | (ARG_LONG << 16u);
    args[3] = (void *)&offset;

    arg_types[4] = 
        (1u << ARG_INPUT) | (1u << ARG_ARRAY) | (ARG_CHAR << 16u) | (uint) filen;
    args[4] = (void *)fi;

    arg_types[5] = (1u << ARG_OUTPUT) | (ARG_INT << 16u);
    args[5] = (void*)&retcode;

    arg_types[6] = 0;

    while (nleft > 0) {
        buflen = (nleft > MAX_ARRAY_LEN ? MAX_ARRAY_LEN : nleft);
        arg_types[1] = (1u << ARG_INPUT) | (1u << ARG_ARRAY) | (ARG_CHAR << 16u) |
                   (uint) buflen;
        args[1] = (void *)bufLocal;

        rpcSize = (nleft > MAX_ARRAY_LEN ? MAX_ARRAY_LEN : nleft);
        
        int rpc_ret = rpcCall((char *)"write", arg_types, args);

        if (rpc_ret < 0) {  // RpcCall failed.
            nwrite = -EINVAL;
            break;
        } else {
            nwrite = retcode;
        }

        nleft -= nwrite;

        bufLocal += nwrite;
        // Update offset.
        // Note that if the rpcCall modifies offset, we should use another variable.
        offset += nwrite;
    }

    if (nwrite < 0) {    // Return -EINVAL or -errno.
        return nwrite;
    }

    return (size - nleft);
    // return -ENOSYS;
}
int watdfs_cli_truncate_rpc(void *userdata, const char *path, off_t newsize) {
    // Change the file size to newsize.

    // "truncate can be called without opening the file, 
    // but should succeed if the file has the write permissions."
    // What does this mean?

    int ARG_COUNT = 3;

    void **args = (void **)malloc(ARG_COUNT * sizeof(void *));

    int arg_types[ARG_COUNT + 1];

    int pathlen = strlen(path) + 1;

    int retcode;

    arg_types[0] =
        (1u << ARG_INPUT) | (1u << ARG_ARRAY) | (ARG_CHAR << 16u) | (uint) pathlen;
    args[0] = (void *)path;

    arg_types[1] = (1u << ARG_INPUT) | (ARG_LONG << 16u);
    args[1] = (void *)&newsize;

    arg_types[2] = (1u << ARG_OUTPUT) | (ARG_INT << 16u);
    args[2] = (void*)&retcode;

    arg_types[3] = 0;

    int rpc_ret = rpcCall((char *)"truncate", arg_types, args);

    int fxn_ret = 0;

    if (rpc_ret < 0) {
        fxn_ret = -EINVAL;
    } else {
        fxn_ret = retcode;
    }

    free(args);

    return fxn_ret;
    // return -ENOSYS;
}

int watdfs_cli_fsync_rpc(void *userdata, const char *path,
                     struct fuse_file_info *fi) {
    // Force a flush of file data.
    int ARG_COUNT = 3;

    void **args = (void **)malloc(ARG_COUNT * sizeof(void *));

    int arg_types[ARG_COUNT + 1];

    int pathlen = strlen(path) + 1;

    int filen = sizeof(struct fuse_file_info);

    int retcode;

    arg_types[0] =
        (1u << ARG_INPUT) | (1u << ARG_ARRAY) | (ARG_CHAR << 16u) | (uint) pathlen;
    args[0] = (void *)path;

    arg_types[1] = 
        (1u << ARG_INPUT) | (1u << ARG_ARRAY) | (ARG_CHAR << 16u) | (uint) filen;
    args[1] = (void *)fi;

    arg_types[2] = (1u << ARG_OUTPUT) | (ARG_INT << 16u);
    args[2] = (void*)&retcode;

    arg_types[3] = 0;

    int rpc_ret = rpcCall((char *)"fsync", arg_types, args);

    int fxn_ret = 0;

    if (rpc_ret < 0) {
        fxn_ret = -EINVAL;
    } else {
        fxn_ret = retcode;
    }

    free(args);

    return fxn_ret;
    // return -ENOSYS;
}

// CHANGE METADATA
int watdfs_cli_utimens_rpc(void *userdata, const char *path,
                       const struct timespec ts[2]) {
    // Change file access and modification times.
    int ARG_COUNT = 3;

    void **args = (void **)malloc(ARG_COUNT * sizeof(void *));

    int arg_types[ARG_COUNT + 1];

    int pathlen = strlen(path) + 1;

    int tslen = sizeof(ts[0]) * 2;

    int retcode;

    arg_types[0] =
        (1u << ARG_INPUT) | (1u << ARG_ARRAY) | (ARG_CHAR << 16u) | (uint) pathlen;
    args[0] = (void *)path;

    arg_types[1] = 
        (1u << ARG_INPUT) | (1u << ARG_ARRAY) | (ARG_CHAR << 16u) | (uint) tslen;
    args[1] = (void *)ts;

    arg_types[2] = (1u << ARG_OUTPUT) | (ARG_INT << 16u);
    args[2] = (void*)&retcode;

    arg_types[3] = 0;

    int rpc_ret = rpcCall((char *)"utimens", arg_types, args);

    int fxn_ret = 0;
    if (rpc_ret < 0) {
        fxn_ret = -EINVAL;
    } else {
        fxn_ret = retcode;
    }

    free(args);

    return fxn_ret;
    // return -ENOSYS;
}
