# System Manual
By Ziyuan Wu, Student ID: 20822077

1. Design Choices
   - Download: The method for download is `watdfs_cli_download`.
     - Get the file size on server. Invoke rpc getattr call to get *statbuf->st_size*.
     - Try to open the file on server. After rpc open call, store the file descriptor on the server side into *file_info* structure.
     - Try to open or create the local file, and truncate the file according to the file size.
     - Lock in read mode.
     - Read the file from server to local buf through rpc read call.
     - Unlock.
     - Write from local buf to local file.
     - Update the metadata *st_mtime* and *st_atime* of the local file.
   - Upload: The method for upload is `watdfs_cli_upload`.
     - Get the local file size.
     - Read from local file to buf.
     - Lock in write mode.
     - Truncate the file on server through rpc truncate call.
     - Write from buf to server through rpc write call.
     - Update *st_mtime* and *st_atime* on server through rpc utimens call.
     - Unlock.
   - Mutual Exclusion on Client
     - Maintain a *set* structure of the opened files, so that we can know if a file has been opened on client. <br>`std::set<std::string> opened_files`
   - Mutual Exclusion on Server
     - Maintain a mapping from *path_name* to *file_status*. In *file_status*, *open_mode* shows if the file is opened in write mode for at least one client, *open_number* indicates how many clients are currently opening this file.
     - Details for the *file_status* structure and the map:
        ```c++
        struct file_status {
            int open_mode;  // 0 for read, 1 for write.
            int open_number;    // Number of files opened.
            rw_lock_t *lock;
        };
        std::map<std::string, struct file_status *> files_status;
        ```
   - Cache Timeout
     - Freshness interval *t* is stored on client as *cache_interval* in userdata.
     - Each file opened on client is mapped to a *file_info* struture, which maintains Tc for that file.
     - T_client and t_server are determined by the metadata *st_mtime* of the file on client and server.
     - For read calls, if the file is opened in RDONLY mode, they will check read freshness for that file. First we check if `T-Tc<t`. If so, return directly. If not, get *t_client* and *t_server* through local stat call and rpc getattr call. If `t_client == t_server`, update tc to current time and return. If not, download the file in O_RDONLY mode, and update Tc to the current time.
     - For write calls, they will check write freshness at the end of writes, when the file is already open. First we check if `T-Tc<t`. If so, return directly. If not, get *t_client* and *t_server* through local stat call and rpc getattr call. If `t_client == t_server`, update tc to current time and return. If not, upload the file in O_RDWR mode, and update Tc to the current time.
     - The *file_info* structure and *global_info* structure (global_info is stored in userdata) are listed as follows:
        ```c++
        struct file_info {
          int fd_cli; // fd for client, for local call.
          int fd_ser; // fd for server, for rpc call.
          int flags; // open mode
          time_t tc;
        };
        struct global_info {
          char *path_to_cache; // The path to cache file in client.
          time_t cache_interval;
          // Keep track of all the opened files on client.
          std::set<std::string> opened_files; 
          // Store file_info for each opened file on client.
          std::map<std::string, file_info *> files_info;
        };
        ```
   - Atomic Transfer
     - There are two rpc calls *lock* and *unlock* registered in *watdfs_server.cpp* file. The registration process is similar to other rpc calls.
     - Lock pointer `rw_lock_t *` for each file is stored in file_status structure on server. The *file_status* structure is created and `rw_lock_init` is called when the file is opened on server.
     - When open_number is 0 after release, which means no client is opening this file, lock will be destroyed and the *file_status* structure will be deleted.
     - Before each read or write process to or from server, *lock* is called. When the process finishes, *unlock* is called.
   - Other Choices
     - Each file opened on client is mapped to a *file_info* struture, which tracks the file descriptors on both client and server, and also the open mode of this file (O_RDONLY, O_WRONLY, O_RDWR).
2. Error Codes
   - For write calls, if the file is opened in read only mode, return -EMFILE.
   - For watdfs_cli_open, if the file is already opened, return -EMFILE.
   - If write exlucsion is violated on server, return -EACCES.
3. Test Methods
   - Testing manually: Run the commands on linux client to test all the functions.
     - Commands: <br>`touch file_path`<br> `echo "content" > file_path`<br> `cat file_path`<br> `stat file_path`
   - Testing in Python: Run the linux commands in python (os.open(), os.write(file_path, string) etc.) through pytest. Assert the return values of the os system calls to be equal to the expected results.
