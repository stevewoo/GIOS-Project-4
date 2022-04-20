#include <regex>
#include <mutex>
#include <vector>
#include <string>
#include <thread>
#include <cstdio>
#include <chrono>
#include <errno.h>
#include <csignal>
#include <iostream>
#include <sstream>
#include <fstream>
#include <iomanip>
#include <getopt.h>
#include <unistd.h>
#include <limits.h>
#include <sys/inotify.h>
#include <grpcpp/grpcpp.h>
#include <utime.h>

#include "src/dfs-utils.h"
#include "src/dfslibx-clientnode-p2.h"
#include "dfslib-shared-p2.h"
#include "dfslib-clientnode-p2.h"
#include "proto-src/dfs-service.grpc.pb.h"

using grpc::Status;
using grpc::Channel;
using grpc::StatusCode;
using grpc::ClientWriter;
using grpc::ClientReader;
using grpc::ClientContext;

using namespace std;
using namespace dfs_service;
using std::chrono::milliseconds;
using std::chrono::system_clock;



extern dfs_log_level_e DFS_LOG_LEVEL;

//
// STUDENT INSTRUCTION:
//
// Change these "using" aliases to the specific
// message types you are using to indicate
// a file request and a listing of files from the server.
//
using FileRequestType = file; //FileRequest;
using FileListResponseType = files; //FileList;

DFSClientNodeP2::DFSClientNodeP2() : DFSClientNode() {}
DFSClientNodeP2::~DFSClientNodeP2() {}


grpc::StatusCode DFSClientNodeP2::Store(const std::string &filename) {

    //
    // STUDENT INSTRUCTION:
    //
    // Add your request to store a file here. Refer to the Part 1
    // student instruction for details on the basics.
    //
    // You can start with your Part 1 implementation. However, you will
    // need to adjust this method to recognize when a file trying to be
    // stored is the same on the server (i.e. the ALREADY_EXISTS gRPC response).
    //
    // You will also need to add a request for a write lock before attempting to store.
    //
    // If the write lock request fails, you should return a status of RESOURCE_EXHAUSTED
    // and cancel the current operation.
    //
    // The StatusCode response should be:
    //
    // StatusCode::OK - if all went well
    // StatusCode::DEADLINE_EXCEEDED - if the deadline timeout occurs
    // StatusCode::ALREADY_EXISTS - if the local cached file has not changed from the server version
    // StatusCode::RESOURCE_EXHAUSTED - if a write lock cannot be obtained
    // StatusCode::CANCELLED otherwise
    //
    //

    ClientContext context;

    // set timeout
    context.set_deadline(get_deadline());

    //fileName request;
    fileResponse response;
    //request.set_file_name(filename);
    const string &full_path = WrapPath(filename);

    // check file exists on client TODO: not necessary according to above
    struct stat file_status;   
    if(stat (full_path.c_str(), &file_status) != 0){
        // file not found
        printf("File not found on client: %s\n", full_path.c_str());
        return StatusCode::NOT_FOUND;
    }

    // get server write lock
    StatusCode writeLockCode = this->RequestWriteAccess(full_path);
    if(writeLockCode != StatusCode::OK){
        return StatusCode::RESOURCE_EXHAUSTED;
    }

    ifstream client_file;
    fileSegment chunk;
    unique_ptr<ClientWriter<fileSegment>> writer = service_stub->StoreFile(&context, &response);


    // send file name and mod time
    chunk.set_file_name(filename);
    chunk.set_modified(file_status.st_mtime);

    // get checksum
    uint32_t client_checksum = dfs_file_checksum(full_path, &(this->crc_table));
    if(client_checksum == 0){
        dfs_log(LL_ERROR) << "0 CHECKSUM in Store";
    }
    chunk.set_check_sum(client_checksum);
    writer->Write(chunk);

    printf("Sent filename: %s\n", filename.c_str());

    // get size
    int file_size = file_status.st_size;

    client_file.open(full_path);

    // send in chunks
    int total_bytes_sent = 0;
    while(total_bytes_sent < file_size){

        char buffer[BUFFER_SIZE];
        int bytes_to_send;
        int total_bytes_remaining = file_size - total_bytes_sent;

        // fit chunk into buffer
        (total_bytes_remaining > BUFFER_SIZE) ? (bytes_to_send = BUFFER_SIZE) : (bytes_to_send = total_bytes_remaining);

        client_file.read(buffer, bytes_to_send);
        chunk.set_data(buffer, bytes_to_send);
        writer->Write(chunk);

        total_bytes_sent += bytes_to_send;
    }
    
    client_file.close();
    
    printf("Sent %s to server\n", full_path.c_str());

    writer->WritesDone();
    Status status = writer->Finish();

    if(status.error_code() == StatusCode::DEADLINE_EXCEEDED){
        printf("Store - Deadline exceeded");
        return StatusCode::DEADLINE_EXCEEDED;
    }
    else if(status.error_code() == StatusCode::ALREADY_EXISTS){ // TODO needs to be checked before file transfer?
        return StatusCode::ALREADY_EXISTS;
    }
    if(!status.ok()) {
        printf("Store - not OK");
        return StatusCode::CANCELLED;
    }

    printf("Completed Store\n");

    //printf("status errorcode(): %d\n",  status.error_code());

    return StatusCode::OK;

}


grpc::StatusCode DFSClientNodeP2::RequestWriteAccess(const std::string &filename) {

    //
    // STUDENT INSTRUCTION:
    //
    // Add your request to obtain a write lock here when trying to store a file.
    // This method should request a write lock for the given file at the server,
    // so that the current client becomes the sole creator/writer. If the server
    // responds with a RESOURCE_EXHAUSTED response, the client should cancel
    // the current file storage
    //
    // The StatusCode response should be:
    //
    // OK - if all went well
    // StatusCode::DEADLINE_EXCEEDED - if the deadline timeout occurs
    // StatusCode::RESOURCE_EXHAUSTED - if a write lock cannot be obtained
    // StatusCode::CANCELLED otherwise
    //
    //

    ClientContext context;

    // set timeout
    context.set_deadline(get_deadline());

    file request;
    request.set_file_name(filename);

    dfs_log(LL_DEBUG) << "Sending client_id: " << ClientId();
    request.set_client_id(ClientId());

    writeLock response;

    Status status = service_stub->RequestWriteLock(&context, request, &response);

    if(status.error_code() == StatusCode::RESOURCE_EXHAUSTED){
        return StatusCode::RESOURCE_EXHAUSTED;
    }
    else if(status.error_code() == StatusCode::DEADLINE_EXCEEDED){
        return StatusCode::DEADLINE_EXCEEDED;
    }
    else if(!status.ok()) {
        return StatusCode::CANCELLED;
    }

    dfs_log(LL_DEBUG) << "Locked file on server: " << request.file_name();
    
    
    return StatusCode::OK;


}

grpc::StatusCode DFSClientNodeP2::Fetch(const std::string &filename) {

    //
    // STUDENT INSTRUCTION:
    //
    // Add your request to fetch a file here. Refer to the Part 1
    // student instruction for details on the basics.
    //
    // You can start with your Part 1 implementation. However, you will
    // need to adjust this method to recognize when a file trying to be
    // fetched is the same on the client (i.e. the files do not differ
    // between the client and server and a fetch would be unnecessary.
    //
    // The StatusCode response should be:
    //
    // OK - if all went well
    // DEADLINE_EXCEEDED - if the deadline timeout occurs
    // NOT_FOUND - if the file cannot be found on the server
    // ALREADY_EXISTS - if the local cached file has not changed from the server version
    // CANCELLED otherwise
    //
    // Hint: You may want to match the mtime on local files to the server's mtime
    //

    ClientContext context;

    // set timeout
    context.set_deadline(get_deadline());

    file request;
    request.set_file_name(filename);
    

    const string &full_path = WrapPath(filename);

    // get checksum
    uint32_t client_checksum = dfs_file_checksum(full_path, &(this->crc_table));
    if(client_checksum == 0){
        dfs_log(LL_ERROR) << "0 CHECKSUM in Fetch";
    }
    request.set_check_sum(client_checksum);

    ofstream client_file;
    fileSegment chunk;
    unique_ptr<ClientReader<fileSegment>> reader = service_stub->FetchFile(&context, request);

    // fileHeader headerRequest;
    // headerRequest.set_file_name(filename);
    // fileHeader headerResponse;
    // Status status  = service_stub->SendHeader(&context, headerRequest, &headerResponse);

    // check file exists on client
    // struct stat file_status;   
    // if(stat(full_path.c_str(), &file_status) == 0){ // file already on client
        

    //     // update modified time
        
    // }
    

    // open file to be written
    client_file.open(full_path);
    printf("Fetching %s\n", full_path.c_str());

    while(reader->Read(&chunk)){

        //printf("Reading chunk\n");

        const string &contents = chunk.data();
        client_file << contents;
    }
    client_file.close();

    Status status = reader->Finish();

    if(status.error_code() == StatusCode::NOT_FOUND){
        return StatusCode::NOT_FOUND;
    }
    else if(status.error_code() == StatusCode::DEADLINE_EXCEEDED){
        return StatusCode::DEADLINE_EXCEEDED;
    }
    else if(status.error_code() == StatusCode::ALREADY_EXISTS){ // TODO needs to be checked before file transfer?
        return StatusCode::ALREADY_EXISTS;
    }
    else if(!status.ok()) {
        return StatusCode::CANCELLED;
    }
    
    printf("Completed fetch\n");
    return StatusCode::OK;
}

grpc::StatusCode DFSClientNodeP2::Delete(const std::string &filename) {

    //
    // STUDENT INSTRUCTION:
    //
    // Add your request to delete a file here. Refer to the Part 1
    // student instruction for details on the basics.
    //
    // You will also need to add a request for a write lock before attempting to delete.
    //
    // If the write lock request fails, you should return a status of RESOURCE_EXHAUSTED
    // and cancel the current operation.
    //
    // The StatusCode response should be:
    //
    // StatusCode::OK - if all went well
    // StatusCode::DEADLINE_EXCEEDED - if the deadline timeout occurs
    // StatusCode::RESOURCE_EXHAUSTED - if a write lock cannot be obtained
    // StatusCode::CANCELLED otherwise
    //
    //

    

    

    // request write lock
    StatusCode lockStatusCode = this->RequestWriteAccess(filename);
    if (lockStatusCode != StatusCode::OK) {
        return StatusCode::RESOURCE_EXHAUSTED;
    }
    // if(lockStatus.error_code() == StatusCode::RESOURCE_EXHAUSTED){
    //     return StatusCode::RESOURCE_EXHAUSTED;
    // }

    ClientContext context;

    // set timeout
    context.set_deadline(get_deadline());

    file request;
    request.set_file_name(filename); // file to be deleted

    // check file exists on server
    // if(lockStatus.error_code() == StatusCode::NOT_FOUND){
    //     printf("CLIENT %s - File not found: %s\n", client_id, filename.c_str());
    //     return StatusCode::NOT_FOUND;
    // }
    
    dfs_log(LL_DEBUG) << "DELETE acquired lock";

    // acquired lock - do delete
    fileResponse deleteResponse;
    printf("CLIENT %s - Requesting deletion of: %s\n", client_id, filename.c_str());
    Status status = service_stub->DeleteFile(&context, request, &deleteResponse);

    if(status.error_code() == StatusCode::DEADLINE_EXCEEDED){
        return StatusCode::DEADLINE_EXCEEDED;
    }
    else if(!status.ok()) {
        return StatusCode::CANCELLED;
    }

    return StatusCode::OK;

}

grpc::StatusCode DFSClientNodeP2::List(std::map<std::string,int>* file_map, bool display) {

    //
    // STUDENT INSTRUCTION:
    //
    // Add your request to list files here. Refer to the Part 1
    // student instruction for details on the basics.
    //
    // You can start with your Part 1 implementation and add any additional
    // listing details that would be useful to your solution to the list response.
    //
    // The StatusCode response should be:
    //
    // StatusCode::OK - if all went well
    // StatusCode::DEADLINE_EXCEEDED - if the deadline timeout occurs
    // StatusCode::CANCELLED otherwise
    //
    //

    ClientContext context;

    // set timeout
    context.set_deadline(get_deadline());

    files response;
    listFilesRequest request;

    Status status = service_stub->ListFiles(&context, request, &response);

    printf("Getting list of files:\n");

    for(const fileStatus &file : response.file()){

        if(status.error_code() == StatusCode::DEADLINE_EXCEEDED){
            return StatusCode::DEADLINE_EXCEEDED;
        }

        // https://stackoverflow.com/questions/13542345/how-to-convert-st-mtime-which-get-from-stat-function-to-string-or-char
        time_t modified = (time_t)file.modified();
        
        if(display){
            struct tm local_time;
            localtime_r(&modified, &local_time);
            char time_buffer[80];
            strftime(time_buffer, sizeof(time_buffer), "%c", &local_time);
            printf("File: %s\t Modified: %s\n", file.file_name().c_str(), time_buffer);
        }

        file_map->insert(pair<string,int>(file.file_name(), modified));

    }

    if(status.error_code() == StatusCode::DEADLINE_EXCEEDED){ // for the case where there are no files
        return StatusCode::DEADLINE_EXCEEDED;
    }
    if(!status.ok()) {
        return StatusCode::CANCELLED;
    }
    return status.error_code();
}

grpc::StatusCode DFSClientNodeP2::Stat(const std::string &filename, void* file_status) {

    //
    // STUDENT INSTRUCTION:
    //
    // Add your request to get the status of a file here. Refer to the Part 1
    // student instruction for details on the basics.
    //
    // You can start with your Part 1 implementation and add any additional
    // status details that would be useful to your solution.
    //
    // The StatusCode response should be:
    //
    // StatusCode::OK - if all went well
    // StatusCode::DEADLINE_EXCEEDED - if the deadline timeout occurs
    // StatusCode::NOT_FOUND - if the file cannot be found on the server
    // StatusCode::CANCELLED otherwise
    //
    //

    file request;
    request.set_file_name(filename);

    ClientContext context;

    // set timeout
    context.set_deadline(get_deadline());

    fileStatus response;

    Status status = service_stub->FileStatus(&context, request, &response);

    printf("Getting file status of: %s\n", filename.c_str());

    file_status = &response;

    // int file_size = response.file_size();
    // printf("File size of %s: %d\n", filename.c_str(), file_size);
    if(status.error_code() == StatusCode::DEADLINE_EXCEEDED){
        return StatusCode::DEADLINE_EXCEEDED;
    }
    else if(!status.ok()) {
        return StatusCode::CANCELLED;
    }
    return StatusCode::OK;

}

void DFSClientNodeP2::InotifyWatcherCallback(std::function<void()> callback) {

    //
    // STUDENT INSTRUCTION:
    //
    // This method gets called each time inotify signals a change
    // to a file on the file system. That is every time a file is
    // modified or created.
    //
    // You may want to consider how this section will affect
    // concurrent actions between the inotify watcher and the
    // asynchronous callbacks associated with the server.
    //
    // The callback method shown must be called here, but you may surround it with
    // whatever structures you feel are necessary to ensure proper coordination
    // between the async and watcher threads.
    //
    // Hint: how can you prevent race conditions between this thread and
    // the async thread when a file event has been signaled?
    //

    dfs_log(LL_DEBUG) << "Inotify call";

    mount_mutex.lock();
    dfs_log(LL_DEBUG) << "Locked to do callback.";

    callback();

    dfs_log(LL_DEBUG) << "Unlocking after callback";
    mount_mutex.unlock();
    dfs_log(LL_DEBUG) << "Unlocked.";
}

//
// STUDENT INSTRUCTION:
//
// This method handles the gRPC asynchronous callbacks from the server.
// We've provided the base structure for you, but you should review
// the hints provided in the STUDENT INSTRUCTION sections below
// in order to complete this method.
//
void DFSClientNodeP2::HandleCallbackList() {

    void* tag;

    bool ok = false;

    //
    // STUDENT INSTRUCTION:
    //
    // Add your file list synchronization code here.
    //
    // When the server responds to an asynchronous request for the CallbackList,
    // this method is called. You should then synchronize the
    // files between the server and the client based on the goals
    // described in the readme.
    //
    // In addition to synchronizing the files, you'll also need to ensure
    // that the async thread and the file watcher thread are cooperating. These
    // two threads could easily get into a race condition where both are trying
    // to write or fetch over top of each other. So, you'll need to determine
    // what type of locking/guarding is necessary to ensure the threads are
    // properly coordinated.
    //

    // Block until the next result is available in the completion queue.
    while (completion_queue.Next(&tag, &ok)) {
        {
            //
            // STUDENT INSTRUCTION:
            //
            // Consider adding a critical section or RAII style lock here
            //

            // The tag is the memory location of the call_data object
            AsyncClientData<FileListResponseType> *call_data = static_cast<AsyncClientData<FileListResponseType> *>(tag);

            dfs_log(LL_DEBUG) << "Received completion queue callback";

            // Verify that the request was completed successfully. Note that "ok"
            // corresponds solely to the request for updates introduced by Finish().
            // GPR_ASSERT(ok);
            if (!ok) {
                dfs_log(LL_ERROR) << "Completion queue callback not ok.";
            }

            if (ok && call_data->status.ok()) {

                dfs_log(LL_DEBUG) << "Handling async callback ";
                

                //
                // STUDENT INSTRUCTION:
                //
                // Add your handling of the asynchronous event calls here.
                // For example, based on the file listing returned from the server,
                // how should the client respond to this updated information?
                // Should it retrieve an updated version of the file?
                // Send an update to the server?
                // Do nothing?
                //
                dfs_log(LL_DEBUG) << "Locking...";
                mount_mutex.lock();

                //dfs_log(LL_DEBUG) << "Handling async callback ";

                //this->List()

                // go through each file on server - update client mount appropriately
                for(const fileStatus &serverFileStatus : call_data->reply.file()){

                    // get local file
                    const std::string &full_path = WrapPath(serverFileStatus.file_name());
                    fileStatus clientFileStatus;

                    StatusCode statusCode;

                    struct stat buffer;   
                    if(stat(full_path.c_str(), &buffer) == 0) { // file exists on client

                        dfs_log(LL_DEBUG) << "Found " << serverFileStatus.file_name() << " on client";

                        clientFileStatus.set_allocated_file_name(new string(full_path)); // populate file_status
                        clientFileStatus.set_created(buffer.st_ctime);
                        clientFileStatus.set_modified(buffer.st_mtime); // TODO check these
                        clientFileStatus.set_file_size(buffer.st_size);

                        dfs_log(LL_DEBUG) << "Client modified:" << clientFileStatus.modified();
                        //human_time(clientFileStatus.file_name().c_str(), clientFileStatus.modified());
                        
                        dfs_log(LL_DEBUG) << "Server modified:" << serverFileStatus.modified();
                        //human_time(serverFileStatus.file_name().c_str(), serverFileStatus.modified());

                        // compare modified times
                        if(serverFileStatus.modified() < clientFileStatus.modified()){ // server version more recent - fetch
                            
                            // TODO what if same file, just updated modified time?

                            dfs_log(LL_DEBUG) << "Newer file found on server - fetching: " << serverFileStatus.file_name();
                            statusCode = this->Fetch(serverFileStatus.file_name());
                            if(statusCode != StatusCode::OK){
                                dfs_log(LL_ERROR) << "Error fetching: " << serverFileStatus.file_name();
                            }
                            dfs_log(LL_DEBUG) << "Fetch done: " << serverFileStatus.file_name();

                            // update modified time to match server
                            struct utimbuf modified_time;
                            modified_time.modtime = clientFileStatus.modified();
                            utime(full_path.c_str(), &modified_time);

                        } else if(serverFileStatus.modified() > clientFileStatus.modified()){ // client version most recent - store

                            // 
                            dfs_log(LL_DEBUG) << "Newer file found on client - storing: " << serverFileStatus.file_name();
                            statusCode = this->Store(serverFileStatus.file_name());
                            if(statusCode != StatusCode::OK){
                                dfs_log(LL_ERROR) << "Error storing: " << serverFileStatus.file_name();
                            }
                            dfs_log(LL_DEBUG) << "Store done: " << serverFileStatus.file_name();

                        }

                    }
                    else{ // file does not exist - fetch it

                        dfs_log(LL_DEBUG) << "File does not exist - fetching: " << serverFileStatus.file_name();
                        statusCode = this->Fetch(serverFileStatus.file_name());
                        if(statusCode != StatusCode::OK){
                            dfs_log(LL_ERROR) << "Error fetching: " << serverFileStatus.file_name();
                        }
                        dfs_log(LL_DEBUG) << "Fetch done: " << serverFileStatus.file_name();

                    }
                }

                dfs_log(LL_DEBUG) << "Unlocking...";
                mount_mutex.unlock();
                dfs_log(LL_DEBUG) << "Unlocked.";

            } else {
                dfs_log(LL_ERROR) << "Status was not ok. Will try again in " << DFS_RESET_TIMEOUT << " milliseconds.";
                dfs_log(LL_ERROR) << call_data->status.error_message();
                std::this_thread::sleep_for(std::chrono::milliseconds(DFS_RESET_TIMEOUT));
            }

            // Once we're complete, deallocate the call_data object.
            delete call_data;

            std::this_thread::sleep_for(std::chrono::milliseconds(1000)); // TODO remove - used for easier debugging

            //
            // STUDENT INSTRUCTION:
            //
            // Add any additional syncing/locking mechanisms you may need here

        }

        // Start the process over and wait for the next callback response
        dfs_log(LL_DEBUG) << "Calling InitCallbackList";
        InitCallbackList();

    }
}

/**
 * This method will start the callback request to the server, requesting
 * an update whenever the server sees that files have been modified.
 *
 * We're making use of a template function here, so that we can keep some
 * of the more intricate workings of the async process out of the way, and
 * give you a chance to focus more on the project's requirements.
 */
void DFSClientNodeP2::InitCallbackList() {
    CallbackList<FileRequestType, FileListResponseType>();
}

//
// STUDENT INSTRUCTION:
//
// Add any additional code you need to here
//
std::chrono::system_clock::time_point DFSClientNodeP2::get_deadline(){

    return system_clock::now() + milliseconds(deadline_timeout);
}





