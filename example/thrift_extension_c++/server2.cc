// Licensed to the Apache Software Foundation (ASF) under one
// or more contributor license agreements.  See the NOTICE file
// distributed with this work for additional information
// regarding copyright ownership.  The ASF licenses this file
// to you under the Apache License, Version 2.0 (the
// "License"); you may not use this file except in compliance
// with the License.  You may obtain a copy of the License at
//
//   http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing,
// software distributed under the License is distributed on an
// "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
// KIND, either express or implied.  See the License for the
// specific language governing permissions and limitations
// under the License.

// A server to receive EchoRequest and send back EchoResponse.

#include <gflags/gflags.h>
#include "flare/log/logging.h"
#include <flare/rpc/server.h>
#include <flare/rpc/thrift_message.h>
#include <flare/rpc/channel.h>
#include <flare/rpc/thrift_service.h>
#include "gen-cpp/echo_types.h"

DEFINE_int32(port, 8019, "TCP Port of this server");
DEFINE_int32(idle_timeout_s, -1, "Connection will be closed if there is no "
             "read/write operations during the last `idle_timeout_s'");
DEFINE_int32(max_concurrency, 0, "Limit of request processing in parallel");

// Adapt your own thrift-based protocol to use flare
class EchoServiceImpl : public flare::rpc::ThriftService {
public:
    EchoServiceImpl() {
        // Initialize the channel, NULL means using default options. 
        flare::rpc::ChannelOptions options;
        options.protocol = flare::rpc::PROTOCOL_THRIFT;
        if (_channel.Init("0.0.0.0", FLAGS_port , &options) != 0) {
            FLARE_LOG(ERROR) << "Fail to initialize channel";
        }
    }

    void ProcessThriftFramedRequest(flare::rpc::Controller* cntl,
                                    flare::rpc::ThriftFramedMessage* req,
                                    flare::rpc::ThriftFramedMessage* res,
                                    google::protobuf::Closure* done) override {
        // Dispatch calls to different methods
        if (cntl->thrift_method_name() == "Echo") {
            // Proxy request/response to RealEcho, note that as a proxy we 
            // don't need to Cast the messages to native types.
            flare::rpc::Controller cntl;
            flare::rpc::ThriftStub stub(&_channel);
            // TODO: Following Cast<> drops data field from ProxyRequest which
            // does not recognize the field, should be debugged further.
            // FLARE_LOG(INFO) << "req=" << *req->Cast<example::ProxyRequest>();
            stub.CallMethod("RealEcho", &cntl, req, res, NULL);
            done->Run();
        } else if (cntl->thrift_method_name() == "RealEcho") {
            return RealEcho(cntl, req->Cast<example::EchoRequest>(),
                        res->Cast<example::EchoResponse>(), done);
        } else {    
            cntl->SetFailed(flare::rpc::ENOMETHOD, "Fail to find method=%s",
                    cntl->thrift_method_name().c_str());
            done->Run();
        }
    }

    void RealEcho(flare::rpc::Controller* cntl,
                  const example::EchoRequest* req,
                  example::EchoResponse* res,
                  google::protobuf::Closure* done) {
        // This object helps you to call done->Run() in RAII style. If you need
        // to process the request asynchronously, pass done_guard.release().
        flare::rpc::ClosureGuard done_guard(done);

        res->data = req->data + " (RealEcho)";
    }
private:
    flare::rpc::Channel _channel;
};

int main(int argc, char* argv[]) {
    // Parse gflags. We recommend you to use gflags as well.
    google::ParseCommandLineFlags(&argc, &argv, true);

    flare::rpc::Server server;
    flare::rpc::ServerOptions options;
    
    options.thrift_service = new EchoServiceImpl;
    options.idle_timeout_sec = FLAGS_idle_timeout_s;
    options.max_concurrency = FLAGS_max_concurrency;

    // Start the server.
    if (server.Start(FLAGS_port, &options) != 0) {
        FLARE_LOG(ERROR) << "Fail to start EchoServer";
        return -1;
    }

    // Wait until Ctrl-C is pressed, then Stop() and Join() the server.
    server.RunUntilAskedToQuit();
    return 0;
}
