# System Manual
By Ziyuan Wu, Student ID: 20822077

1. Design Choices
   - Download: The method for download is watdfs_cli_download.
     - Get the file size on server.
     - Try to open or create the local file, and truncate the file according to the file size.
     - Lock in read mode.
     - Read the file from server to local buf.
     - Unlock.
     - Write from local buf to local file.
     - Update the metadata *st_mtime* and *st_atime* of the file.
   - Upload: The method for upload is watdfs_cli_upload.
     - Get the local file size.
     - Read from local file to buf.
     - Lock in write mode.
     - Truncate the file on server.
     - Write from buf to server.
     - Update *st_mtime* and *st_atime* on server.
     - Unlock.
   - Mutual Exclusion on Client
     - Maintain a set of the opened files, so that we can know if a file has been opened on client.
   - Mutual Exclusion on Server
     - Maintain a mapping from path_name to file_status. In file_status, open_mode indicates if the file is opened in write mode or not, open_number indicates how many clients are currently opening this file.
   - Cache Timeout
     - Freshness interval *t* is stored on client as *cache_interval* in userdata.
     - Each file opened on client is mapped to a *file_info* struture, which maintains Tc for that file.
     - T_client and t_server are determined by the metadata *st_mtime* of the file on client and server.
     - For read calls, if the file is opened in RDONLY mode, they will check read freshness for that file.
     - For write calls, they will check write freshness at the beginnning.
   - Atomic Transfer
     - There are two methods *lock* and *unlock* registered in watdfs_server file.
     - Lock pointer is stored in file_status structure on server.
     - When open_number is 0 after release, which means no client is opening this file, lock will be destroyed.
   - Other Choices
     - Each file opened on client is mapped to a *file_info* struture, which tracks the file descriptors on client and server, and also the open mode of this file (O_RDONLY, O_WRONLY, O_RDWR).
2. Error Codes
   - For write calls, if the file is opened in read only mode, return -EMFILE.
   - For watdfs_cli_open, if the file is already opened, return -EMFILE.
   - If write exlucsion is violated on server, return -EACCES.
4. Test Methods
   - Testing Manually: Run the commands on linux server and client to test all the functions. Open the same file in write mode on 2 clients to test the mutual exclusion on server.
     - Commands: touch file_path --> echo "content" > file_path --> cat file_path --> stat file_path
   - Testing in Python: 

