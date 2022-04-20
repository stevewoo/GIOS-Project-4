#include <string>
#include <iostream>
#include <fstream>
#include <cstddef>
#include <sys/stat.h>

#include "dfslib-shared-p2.h"
#include "proto-src/dfs-service.grpc.pb.h"



// Global log level used throughout the system
// Note: this may be adjusted from the CLI in
// both the client and server executables.
// This shouldn't be changed at the file level.
dfs_log_level_e DFS_LOG_LEVEL = LL_ERROR;

//
// STUDENT INSTRUCTION:
//
// Add your additional code here. You may use
// the shared files for anything you need to add outside
// of the main structure, but you don't have to use them.
//
// Just be aware they are always submitted, so they should
// be compilable.
//
void human_time(std::string file_name, time_t modified){

    struct tm local_time;
    localtime_r(&modified, &local_time);
    char time_buffer[80];
    strftime(time_buffer, sizeof(time_buffer), "%c", &local_time);
    //printf("File: %s\t Modified: %s\n", file.file_name().c_str(), time_buffer);

    printf("File: %s\t Modified: %s\n", file_name.c_str(), time_buffer);

}
