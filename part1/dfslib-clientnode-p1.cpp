#include <regex>
#include <vector>
#include <string>
#include <thread>
#include <cstdio>
#include <chrono>
#include <errno.h>
#include <iostream>
#include <sstream>
#include <fstream>
#include <csignal>
#include <iomanip>
#include <getopt.h>
#include <unistd.h>
#include <limits.h>
#include <sys/inotify.h>
#include <grpcpp/grpcpp.h>
#include <ctime>
#include <sys/time.h>


#include "dfslib-shared-p1.h"
#include "proto-src/dfs-service.grpc.pb.h"
#include "dfslib-clientnode-p1.h"

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


// 
// STUDENT INSTRUCTION:
//
// You may want to add aliases to your namespaced service methods here.
// All of the methods will be under the `dfs_service` namespace.
//
// For example, if you have a method named MyMethod, add
// the following:
//
//      using dfs_service::MyMethod
//


DFSClientNodeP1::DFSClientNodeP1() : DFSClientNode() {}

DFSClientNodeP1::~DFSClientNodeP1() noexcept {}



StatusCode DFSClientNodeP1::Store(const std::string &filename) {

    //
    // STUDENT INSTRUCTION:
    //
    // Add your request to store a file here. This method should
    // connect to your gRPC service implementation method
    // that can accept and store a file.
    //
    // When working with files in gRPC you'll need to stream
    // the file contents, so consider the use of gRPC's ClientWriter.
    //
    // The StatusCode response should be:
    //
    // StatusCode::OK - if all went well
    // StatusCode::DEADLINE_EXCEEDED - if the deadline timeout occurs
    // StatusCode::NOT_FOUND - if the file cannot be found on the client
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

    ifstream client_file;
    fileSegment chunk;
    unique_ptr<ClientWriter<fileSegment>> writer = service_stub->StoreFile(&context, &response);

    // check file exists
    struct stat file_status;   
    if(stat (full_path.c_str(), &file_status) != 0){
        // file not found
        printf("File not found on client: %s\n", full_path.c_str());
        return StatusCode::NOT_FOUND;
    }

    // send file name
    chunk.set_file_name(filename);
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
    if(!status.ok()) {
        printf("Store - not OK");
        return StatusCode::CANCELLED;
    }

    printf("Completed Store\n");

    //printf("status errorcode(): %d\n",  status.error_code());

    return StatusCode::OK;

}


StatusCode DFSClientNodeP1::Fetch(const std::string &filename) {

    //
    // STUDENT INSTRUCTION:
    //
    // Add your request to fetch a file here. This method should
    // connect to your gRPC service implementation method
    // that can accept a file request and return the contents
    // of a file from the service.
    //
    // As with the store function, you'll need to stream the
    // contents, so consider the use of gRPC's ClientReader.
    //
    // The StatusCode response should be:
    //
    // StatusCode::OK - if all went well
    // StatusCode::DEADLINE_EXCEEDED - if the deadline timeout occurs
    // StatusCode::NOT_FOUND - if the file cannot be found on the server
    // StatusCode::CANCELLED otherwise
    //
    //

    ClientContext context;

    // set timeout
    context.set_deadline(get_deadline());

    file request;
    request.set_file_name(filename);
    const string &full_path = WrapPath(filename);

    ofstream client_file;
    fileSegment chunk;
    unique_ptr<ClientReader<fileSegment>> reader = service_stub->FetchFile(&context, request);

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
    else if(!status.ok()) {
        return StatusCode::CANCELLED;
    }
    
    printf("Completed fetch\n");
    return StatusCode::OK;

}

StatusCode DFSClientNodeP1::Delete(const std::string& filename) {

    //
    // STUDENT INSTRUCTION:
    //
    // Add your request to delete a file here. Refer to the Part 1
    // student instruction for details on the basics.
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
    request.set_file_name(filename); // file to be deleted

    ClientContext context;

    // set timeout
    context.set_deadline(get_deadline());

    fileResponse response;

    Status status = service_stub->DeleteFile(&context, request, &response);

    if(status.error_code() == StatusCode::NOT_FOUND){
        return StatusCode::NOT_FOUND;
    }

    printf("Requesting deletion of: %s\n", filename.c_str());

    if(status.error_code() == StatusCode::DEADLINE_EXCEEDED){
        return StatusCode::DEADLINE_EXCEEDED;
    }
    else if(!status.ok()) {
        return StatusCode::CANCELLED;
    }
    return StatusCode::OK;

}


StatusCode DFSClientNodeP1::Stat(const std::string &filename, void* file_status) {

    //
    // STUDENT INSTRUCTION:
    //
    // Add your request to get the status of a file here. This method should
    // retrieve the status of a file on the server. Note that you won't be
    // tested on this method, but you will most likely find that you need
    // a way to get the status of a file in order to synchronize later.
    //
    // The status might include data such as name, size, mtime, crc, etc.
    //
    // The file_status is left as a void* so that you can use it to pass
    // a message type that you defined. For instance, may want to use that message
    // type after calling Stat from another method.
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

    fileStatusResponse response;

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

StatusCode DFSClientNodeP1::List(std::map<std::string,int>* file_map, bool display) {

    //
    // STUDENT INSTRUCTION:
    //
    // Add your request to list all files here. This method
    // should connect to your service's list method and return
    // a list of files using the message type you created.
    //
    // The file_map parameter is a simple map of files. You should fill
    // the file_map with the list of files you receive with keys as the
    // file name and values as the modified time (mtime) of the file
    // received from the server.
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

    for(const fileResponse &file : response.file()){

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

//
// STUDENT INSTRUCTION:
//
// Add your additional code here, including
// implementations of your client methods
//

system_clock::time_point DFSClientNodeP1::get_deadline(){

    return system_clock::now() + milliseconds(deadline_timeout);
}

