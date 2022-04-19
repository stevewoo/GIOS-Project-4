#include <map>
#include <mutex>
#include <shared_mutex>
#include <chrono>
#include <string>
#include <thread>
#include <errno.h>
#include <iostream>
#include <cstdio>
#include <fstream>
#include <getopt.h>
#include <sys/stat.h>
#include <grpcpp/grpcpp.h>
#include <dirent.h>

#include "src/dfslibx-service-runner.h"
#include "dfslib-shared-p2.h"
#include "proto-src/dfs-service.grpc.pb.h"
#include "src/dfslibx-call-data.h"
#include "dfslib-servernode-p2.h"

using grpc::Status;
using grpc::Server;
using grpc::StatusCode;
using grpc::ServerReader;
using grpc::ServerWriter;
using grpc::ServerContext;
using grpc::ServerBuilder;

using dfs_service::DFSService;

using namespace std;
using namespace dfs_service;
using std::chrono::milliseconds;
using std::chrono::system_clock;
using namespace google::protobuf;


//
// STUDENT INSTRUCTION:
//
// Change these "using" aliases to the specific
// message types you are using in your `dfs-service.proto` file
// to indicate a file request and a listing of files from the server
//
using FileRequestType = file; //FileRequest;
using FileListResponseType = files; //FileList;

extern dfs_log_level_e DFS_LOG_LEVEL;

//
// STUDENT INSTRUCTION:
//
// As with Part 1, the DFSServiceImpl is the implementation service for the rpc methods
// and message types you defined in your `dfs-service.proto` file.
//
// You may start with your Part 1 implementations of each service method.
//
// Elements to consider for Part 2:
//
// - How will you implement the write lock at the server level?
// - How will you keep track of which client has a write lock for a file?
//      - Note that we've provided a preset client_id in DFSClientNode that generates
//        a client id for you. You can pass that to the server to identify the current client.
// - How will you release the write lock?
// - How will you handle a store request for a client that doesn't have a write lock?
// - When matching files to determine similarity, you should use the `file_checksum` method we've provided.
//      - Both the client and server have a pre-made `crc_table` variable to speed things up.
//      - Use the `file_checksum` method to compare two files, similar to the following:
//
//          std::uint32_t server_crc = dfs_file_checksum(filepath, &this->crc_table);
//
//      - Hint: as the crc checksum is a simple integer, you can pass it around inside your message types.
//
class DFSServiceImpl final :
    public DFSService::WithAsyncMethod_CallbackList<DFSService::Service>,
        public DFSCallDataManager<FileRequestType , FileListResponseType> {

private:

    /** The runner service used to start the service and manage asynchronicity **/
    DFSServiceRunner<FileRequestType, FileListResponseType> runner;

    /** Mutex for managing the queue requests **/
    std::mutex queue_mutex;

    /** The mount path for the server **/
    std::string mount_path;

    /** The vector of queued tags used to manage asynchronous requests **/
    std::vector<QueueRequest<FileRequestType, FileListResponseType>> queued_tags;


    /**
     * Prepend the mount path to the filename.
     *
     * @param filepath
     * @return
     */
    const std::string WrapPath(const std::string &filepath) {
        return this->mount_path + filepath;
    }

    /** CRC Table kept in memory for faster calculations **/
    CRC::Table<std::uint32_t, 32> crc_table;

    // lock server mount when being accessed // TODO update with file / client access granularity
    mutex mount_mutex;


public:

    DFSServiceImpl(const std::string& mount_path, const std::string& server_address, int num_async_threads):
        mount_path(mount_path), crc_table(CRC::CRC_32()) {

        this->runner.SetService(this);
        this->runner.SetAddress(server_address);
        this->runner.SetNumThreads(num_async_threads);
        this->runner.SetQueuedRequestsCallback([&]{ this->ProcessQueuedRequests(); });

    }

    ~DFSServiceImpl() {
        this->runner.Shutdown();
    }

    void Run() {
        this->runner.Run();
    }

    /**
     * Request callback for asynchronous requests
     *
     * This method is called by the DFSCallData class during
     * an asynchronous request call from the client.
     *
     * Students should not need to adjust this.
     *
     * @param context
     * @param request
     * @param response
     * @param cq
     * @param tag
     */
    void RequestCallback(grpc::ServerContext* context,
                         FileRequestType* request,
                         grpc::ServerAsyncResponseWriter<FileListResponseType>* response,
                         grpc::ServerCompletionQueue* cq,
                         void* tag) {

        std::lock_guard<std::mutex> lock(queue_mutex);
        this->queued_tags.emplace_back(context, request, response, cq, tag);

    }

    /**
     * Process a callback request
     *
     * This method is called by the DFSCallData class when
     * a requested callback can be processed. You should use this method
     * to manage the CallbackList RPC call and respond as needed.
     *
     * See the STUDENT INSTRUCTION for more details.
     *
     * @param context
     * @param request
     * @param response
     */
    void ProcessCallback(ServerContext* context, FileRequestType* request, FileListResponseType* response) {

        //
        // STUDENT INSTRUCTION:
        //
        // You should add your code here to respond to any CallbackList requests from a client.
        // This function is called each time an asynchronous request is made from the client.
        //
        // The client should receive a list of files or modifications that represent the changes this service
        // is aware of. The client will then need to make the appropriate calls based on those changes.
        //

        dfs_log(LL_DEBUG) << "Handling ProcessCallback for " << request->file_name(); // TODO blank
        dfs_log(LL_DEBUG) << "Handling ProcessCallback for " << request->name();

        listFilesRequest dummy_request;

        Status status = this->ListFiles(context, &dummy_request, response); // TODO should be calling CallbackList?

        if(!status.ok()){
            dfs_log(LL_ERROR) << "Issue in ProcessCallback";
            return;
        }

    }

    /**
     * Processes the queued requests in the queue thread
     */
    void ProcessQueuedRequests() {
        
        dfs_log(LL_DEBUG) << "Starting ProcessQueuedRequests";


        while(true) {

            //
            // STUDENT INSTRUCTION:
            //
            // You should add any synchronization mechanisms you may need here in
            // addition to the queue management. For example, modified files checks.
            //
            // Note: you will need to leave the basic queue structure as-is, but you
            // may add any additional code you feel is necessary.
            //


            // Guarded section for queue
            {
                dfs_log(LL_DEBUG2) << "Waiting for queue guard";
                std::lock_guard<std::mutex> lock(queue_mutex);


                for(QueueRequest<FileRequestType, FileListResponseType>& queue_request : this->queued_tags) {
                    this->RequestCallbackList(queue_request.context, queue_request.request,
                        queue_request.response, queue_request.cq, queue_request.cq, queue_request.tag);
                    queue_request.finished = true;
                }

                // any finished tags first
                this->queued_tags.erase(std::remove_if(
                    this->queued_tags.begin(),
                    this->queued_tags.end(),
                    [](QueueRequest<FileRequestType, FileListResponseType>& queue_request) { return queue_request.finished; }
                ), this->queued_tags.end());

            }
        }
    }

    //
    // STUDENT INSTRUCTION:
    //
    // Add your additional code here, including
    // the implementations of your rpc protocol methods.
    //

    // Status MyMethod(ServerContext* context, const MyMessageType* request, ServerWriter<MySegmentType> *writer) override {

    //      /** code implementation here **/
    //  }


    Status RequestWriteLock(ServerContext *context, const file *request, writeLock *response) override {

        dfs_log(LL_DEBUG) << "Requesting WriteLock for file: " << request->file_name();

        // TODO - lock for individual files
        if(mount_mutex.try_lock()){
            dfs_log(LL_DEBUG) << "Obtained lock";
            return Status::OK;
        }
        else{
            dfs_log(LL_ERROR) << "Server files locked";
            return Status(StatusCode::RESOURCE_EXHAUSTED, "Server files locked");
        }

        //return Status::OK;

    }

    Status StoreFile(ServerContext *context, ServerReader<fileSegment> *reader, fileResponse *response) override {

        dfs_log(LL_DEBUG) << "Calling StoreFile";

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

        mount_mutex.unlock();

        printf("Completed Store\n");

        return Status::OK;

    }

    Status FetchFile(ServerContext *context, const file* request, ServerWriter<fileSegment> *writer) override {

        dfs_log(LL_DEBUG) << "Calling FetchFile";
        
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

        dfs_log(LL_DEBUG) << "Calling DeleteFile";

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

        mount_mutex.unlock();

        return Status::OK;

    }

    

    Status FileStatus(ServerContext* context, const file* request, fileStatus *response) override {

        dfs_log(LL_DEBUG) << "Calling FileStatus";

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

        dfs_log(LL_DEBUG) << "Calling ListFiles";

        mount_mutex.lock();
        //dfs_log(LL_DEBUG) << "Mutex locked";


        // https://stackoverflow.com/a/46105710
        if (auto dir = opendir(mount_path.c_str())) {

            //dfs_log(LL_DEBUG) << "Opened dir";
            while (auto file = readdir(dir)) {

                //dfs_log(LL_DEBUG) << "Opened file";

                // https://piazza.com/class/ky4oj8pjzic6wh?cid=1253
                // https://piazza.com/class/ky4oj8pjzic6wh?cid=1242
                context->AsyncNotifyWhenDone(NULL);

                if (context->IsCancelled()) {

                    mount_mutex.unlock();
                    return Status(StatusCode::DEADLINE_EXCEEDED, "Server abandoned Delete: Deadline exceeded or client cancelled");
                    
                }

                //printf("dirent: %s\n", file->d_name);
                if (file->d_name && file->d_name[0] != '.'&& file->d_type != DT_DIR ){

                    printf("File: %s\n", file->d_name);

                    // add file to response
                    fileStatus *file_meta = response->add_file();

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
            mount_mutex.unlock();
        }

        return Status::OK;
    }


};

//
// STUDENT INSTRUCTION:
//
// The following three methods are part of the basic DFSServerNode
// structure. You may add additional methods or change these slightly
// to add additional startup/shutdown routines inside, but be aware that
// the basic structure should stay the same as the testing environment
// will be expected this structure.
//
/**
 * The main server node constructor
 *
 * @param mount_path
 */
DFSServerNode::DFSServerNode(const std::string &server_address,
        const std::string &mount_path,
        int num_async_threads,
        std::function<void()> callback) :
        server_address(server_address),
        mount_path(mount_path),
        num_async_threads(num_async_threads),
        grader_callback(callback) {}
/**
 * Server shutdown
 */
DFSServerNode::~DFSServerNode() noexcept {
    dfs_log(LL_SYSINFO) << "DFSServerNode shutting down";
}

/**
 * Start the DFSServerNode server
 */
void DFSServerNode::Start() {
    DFSServiceImpl service(this->mount_path, this->server_address, this->num_async_threads);


    dfs_log(LL_SYSINFO) << "DFSServerNode server listening on " << this->server_address;
    service.Run();
}

//
// STUDENT INSTRUCTION:
//
// Add your additional definitions here
//
