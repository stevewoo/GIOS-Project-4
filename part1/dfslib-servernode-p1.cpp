#include <map>
#include <chrono>
#include <cstdio>
#include <string>
#include <thread>
#include <iostream>
#include <fstream>
#include <errno.h>
#include <dirent.h>
#include <sys/stat.h>
#include <getopt.h>
#include <grpcpp/grpcpp.h>

#include "src/dfs-utils.h"
#include "dfslib-servernode-p1.h"
#include "proto-src/dfs-service.grpc.pb.h"
#include "dfslib-shared-p1.h"

using namespace std;


using grpc::Status;
using grpc::Server;
using grpc::StatusCode;
using grpc::ServerReader;
using grpc::ServerWriter;
using grpc::ServerContext;
using grpc::ServerBuilder;

using dfs_service::DFSService;

// using dfs_service::fileName;
// using dfs_service::fileSegment;

using namespace dfs_service;

using namespace google::protobuf;


//
// STUDENT INSTRUCTION:
//
// DFSServiceImpl is the implementation service for the rpc methods
// and message types you defined in the `dfs-service.proto` file.
//
// You should add your definition overrides here for the specific
// methods that you created for your GRPC service protocol. The
// gRPC tutorial described in the readme is a good place to get started
// when trying to understand how to implement this class.
//
// The method signatures generated can be found in `proto-src/dfs-service.grpc.pb.h` file.
//
// Look for the following section:
//
//      class Service : public ::grpc::Service {
//
// The methods returning grpc::Status are the methods you'll want to override.
//
// In C++, you'll want to use the `override` directive as well. For example,
// if you have a service method named MyMethod that takes a MyMessageType
// and a ServerWriter, you'll want to override it similar to the following:
//
//      Status MyMethod(ServerContext* context,
//                      const MyMessageType* request,
//                      ServerWriter<MySegmentType> *writer) override {
//
//          /** code implementation here **/
//      }
//
class DFSServiceImpl final : public DFSService::Service {

private:

    /** The mount path for the server **/
    std::string mount_path;

    /**
     * Prepend the mount path to the filename.
     *
     * @param filepath
     * @return
     */
    const std::string WrapPath(const std::string &filepath) {
        return this->mount_path + filepath;
    }


public:

    DFSServiceImpl(const std::string &mount_path): mount_path(mount_path) {
    }

    ~DFSServiceImpl() {}

    //
    // STUDENT INSTRUCTION:
    //
    // Add your additional code here, including
    // implementations of your protocol service methods
    //


    Status StoreFile(ServerContext *context, ServerReader<fileSegment> *reader, fileResponse *response) override {

        // get file name
        fileSegment chunk;
        reader->Read(&chunk);
        const string &file_name = chunk.file_name();
        printf("Received: %s\n", file_name.c_str());

        const string &full_path = WrapPath(file_name);

        ofstream server_file;

        // open file to be written
        server_file.open(full_path);
        printf("Storing %s\n", full_path.c_str());

        while(reader->Read(&chunk)){ 

            if (context->IsCancelled()) {
                return Status(StatusCode::DEADLINE_EXCEEDED, "Server abandoned Store: Deadline exceeded or client cancelled");
            }

            //printf("Reading chunk\n");

            const string &contents = chunk.data();
            //printf("Writing %ld bytes to server file:%s\n", strlen(contents.c_str()), full_path.c_str());
            server_file << contents;
        }
        
        printf("Closing %s\n", full_path.c_str());
        server_file.close();

        response->set_file_name(file_name);

        //Status status = reader->Finish();

        printf("Completed Store\n");

        return Status::OK;

    }

    Status FetchFile(ServerContext *context, const file* request, ServerWriter<fileSegment> *writer) override {

        // get file path
        const string &full_path = WrapPath(request->file_name());

        // check file exists
        struct stat file_status;   

        if(stat(full_path.c_str(), &file_status) != 0){
            // file not found
            printf("File not found: %s\n",full_path.c_str());
            return Status(StatusCode::NOT_FOUND, "File not found");
        }
        else{ // send it

            // get size
            int file_size = file_status.st_size;

            fileSegment chunk;

            // input file stream
            ifstream server_file;
            server_file.open(full_path);

            int total_bytes_sent = 0;
            while(total_bytes_sent < file_size){

                if (context->IsCancelled()) {
                    return Status(StatusCode::DEADLINE_EXCEEDED, "Server abandoned Fetch: Deadline exceeded or client cancelled");
                }

                char buffer[BUFFER_SIZE];
                int bytes_to_send;
                int total_bytes_remaining = file_size - total_bytes_sent;
                (total_bytes_remaining > BUFFER_SIZE) ? (bytes_to_send = BUFFER_SIZE) : (bytes_to_send = total_bytes_remaining);

                server_file.read(buffer, bytes_to_send);
                chunk.set_data(buffer, bytes_to_send);
                writer->Write(chunk);

                total_bytes_sent += bytes_to_send;

            }
            server_file.close();
            
            printf("Sent %s to client\n", full_path.c_str());

        }
        return Status::OK;

    }

    Status DeleteFile(ServerContext *context, const file* request, fileResponse *response) override {

        // get file path
        const string &full_path = WrapPath(request->file_name());

        // check file exists
        struct stat buffer;   
        if(stat(full_path.c_str(), &buffer) != 0){
            // file not found
            printf("File not found: %s\n",full_path.c_str());
            return Status(StatusCode::NOT_FOUND, "File not found");
        }
        if (context->IsCancelled()) {
            return Status(StatusCode::DEADLINE_EXCEEDED, "Server abandoned Delete: Deadline exceeded or client cancelled");
        }

        // delete it
        remove(full_path.c_str()); // TODO error check
        printf("removed %s\n",full_path.c_str());
        return Status::OK;

    }

    

    Status FileStatus(ServerContext* context, const file* request, fileResponse *response) override {


        // get file path
        const string &full_path = WrapPath(request->file_name());

        // check file exists
        struct stat buffer;   
        if(stat (full_path.c_str(), &buffer) != 0){
            // file not found
            printf("File not found: %s\n",full_path.c_str());
            return Status(StatusCode::NOT_FOUND, "File not found");
        }
        if (context->IsCancelled()) {
            return Status(StatusCode::DEADLINE_EXCEEDED, "Server abandoned Status: Deadline exceeded or client cancelled");
        }
        
        return Status::OK;


    }

    Status ListFiles(ServerContext* context, const listFilesRequest* request, files *response) override {

        // https://stackoverflow.com/a/46105710
        if (auto dir = opendir(mount_path.c_str())) {
            while (auto file = readdir(dir)) {

                if (context->IsCancelled()) {
                    return Status(StatusCode::DEADLINE_EXCEEDED, "Server abandoned Delete: Deadline exceeded or client cancelled");
                }

                //printf("dirent: %s\n", file->d_name);
                if (file->d_name && file->d_name[0] != '.'&& file->d_type != DT_DIR ){

                    printf("File: %s\n", file->d_name);

                    // add file to response
                    fileResponse *file_meta = response->add_file();

                    // set file name
                    file_meta->set_file_name(file->d_name);

                    // set modified time
                    const string &full_path = WrapPath(file->d_name);

                    struct stat file_status;
                    stat(full_path.c_str(), &file_status); // TODO error check
                    int64 modified_time = file_status.st_mtime;
                    file_meta->set_modified(modified_time);
                    
                }

            }
            closedir(dir);
        }

        return Status::OK;
    }
};

//
// STUDENT INSTRUCTION:
//
// The following three methods are part of the basic DFSServerNode
// structure. You may add additional methods or change these slightly,
// but be aware that the testing environment is expecting these three
// methods as-is.
//
/**
 * The main server node constructor
 *
 * @param server_address
 * @param mount_path
 */
DFSServerNode::DFSServerNode(const std::string &server_address,
        const std::string &mount_path,
        std::function<void()> callback) :
    server_address(server_address), mount_path(mount_path), grader_callback(callback) {}

/**
 * Server shutdown
 */
DFSServerNode::~DFSServerNode() noexcept {
    dfs_log(LL_SYSINFO) << "DFSServerNode shutting down";
    this->server->Shutdown();
}

/** Server start **/
void DFSServerNode::Start() {
    DFSServiceImpl service(this->mount_path);
    ServerBuilder builder;
    builder.AddListeningPort(this->server_address, grpc::InsecureServerCredentials());
    builder.RegisterService(&service);
    this->server = builder.BuildAndStart();
    dfs_log(LL_SYSINFO) << "DFSServerNode server listening on " << this->server_address;
    this->server->Wait();
}

//
// STUDENT INSTRUCTION:
//
// Add your additional DFSServerNode definitions here
//

