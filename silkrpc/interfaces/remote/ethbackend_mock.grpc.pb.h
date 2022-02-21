// Generated by the gRPC C++ plugin.
// If you make any local change, they will be lost.
// source: remote/ethbackend.proto

#include "remote/ethbackend.pb.h"
#include "remote/ethbackend.grpc.pb.h"

#include <grpcpp/impl/codegen/async_stream.h>
#include <grpcpp/impl/codegen/sync_stream.h>
#include <gmock/gmock.h>
namespace remote {

class MockETHBACKENDStub : public ETHBACKEND::StubInterface {
 public:
  MOCK_METHOD3(Etherbase, ::grpc::Status(::grpc::ClientContext* context, const ::remote::EtherbaseRequest& request, ::remote::EtherbaseReply* response));
  MOCK_METHOD3(AsyncEtherbaseRaw, ::grpc::ClientAsyncResponseReaderInterface< ::remote::EtherbaseReply>*(::grpc::ClientContext* context, const ::remote::EtherbaseRequest& request, ::grpc::CompletionQueue* cq));
  MOCK_METHOD3(PrepareAsyncEtherbaseRaw, ::grpc::ClientAsyncResponseReaderInterface< ::remote::EtherbaseReply>*(::grpc::ClientContext* context, const ::remote::EtherbaseRequest& request, ::grpc::CompletionQueue* cq));
  MOCK_METHOD3(NetVersion, ::grpc::Status(::grpc::ClientContext* context, const ::remote::NetVersionRequest& request, ::remote::NetVersionReply* response));
  MOCK_METHOD3(AsyncNetVersionRaw, ::grpc::ClientAsyncResponseReaderInterface< ::remote::NetVersionReply>*(::grpc::ClientContext* context, const ::remote::NetVersionRequest& request, ::grpc::CompletionQueue* cq));
  MOCK_METHOD3(PrepareAsyncNetVersionRaw, ::grpc::ClientAsyncResponseReaderInterface< ::remote::NetVersionReply>*(::grpc::ClientContext* context, const ::remote::NetVersionRequest& request, ::grpc::CompletionQueue* cq));
  MOCK_METHOD3(NetPeerCount, ::grpc::Status(::grpc::ClientContext* context, const ::remote::NetPeerCountRequest& request, ::remote::NetPeerCountReply* response));
  MOCK_METHOD3(AsyncNetPeerCountRaw, ::grpc::ClientAsyncResponseReaderInterface< ::remote::NetPeerCountReply>*(::grpc::ClientContext* context, const ::remote::NetPeerCountRequest& request, ::grpc::CompletionQueue* cq));
  MOCK_METHOD3(PrepareAsyncNetPeerCountRaw, ::grpc::ClientAsyncResponseReaderInterface< ::remote::NetPeerCountReply>*(::grpc::ClientContext* context, const ::remote::NetPeerCountRequest& request, ::grpc::CompletionQueue* cq));
  MOCK_METHOD3(EngineGetPayloadV1, ::grpc::Status(::grpc::ClientContext* context, const ::remote::EngineGetPayloadRequest& request, ::types::ExecutionPayload* response));
  MOCK_METHOD3(AsyncEngineGetPayloadV1Raw, ::grpc::ClientAsyncResponseReaderInterface< ::types::ExecutionPayload>*(::grpc::ClientContext* context, const ::remote::EngineGetPayloadRequest& request, ::grpc::CompletionQueue* cq));
  MOCK_METHOD3(PrepareAsyncEngineGetPayloadV1Raw, ::grpc::ClientAsyncResponseReaderInterface< ::types::ExecutionPayload>*(::grpc::ClientContext* context, const ::remote::EngineGetPayloadRequest& request, ::grpc::CompletionQueue* cq));
  MOCK_METHOD3(EngineNewPayloadV1, ::grpc::Status(::grpc::ClientContext* context, const ::types::ExecutionPayload& request, ::remote::EnginePayloadStatus* response));
  MOCK_METHOD3(AsyncEngineNewPayloadV1Raw, ::grpc::ClientAsyncResponseReaderInterface< ::remote::EnginePayloadStatus>*(::grpc::ClientContext* context, const ::types::ExecutionPayload& request, ::grpc::CompletionQueue* cq));
  MOCK_METHOD3(PrepareAsyncEngineNewPayloadV1Raw, ::grpc::ClientAsyncResponseReaderInterface< ::remote::EnginePayloadStatus>*(::grpc::ClientContext* context, const ::types::ExecutionPayload& request, ::grpc::CompletionQueue* cq));
  MOCK_METHOD3(EngineForkChoiceUpdatedV1, ::grpc::Status(::grpc::ClientContext* context, const ::remote::EngineForkChoiceUpdatedRequest& request, ::remote::EngineForkChoiceUpdatedReply* response));
  MOCK_METHOD3(AsyncEngineForkChoiceUpdatedV1Raw, ::grpc::ClientAsyncResponseReaderInterface< ::remote::EngineForkChoiceUpdatedReply>*(::grpc::ClientContext* context, const ::remote::EngineForkChoiceUpdatedRequest& request, ::grpc::CompletionQueue* cq));
  MOCK_METHOD3(PrepareAsyncEngineForkChoiceUpdatedV1Raw, ::grpc::ClientAsyncResponseReaderInterface< ::remote::EngineForkChoiceUpdatedReply>*(::grpc::ClientContext* context, const ::remote::EngineForkChoiceUpdatedRequest& request, ::grpc::CompletionQueue* cq));
  MOCK_METHOD3(Version, ::grpc::Status(::grpc::ClientContext* context, const ::google::protobuf::Empty& request, ::types::VersionReply* response));
  MOCK_METHOD3(AsyncVersionRaw, ::grpc::ClientAsyncResponseReaderInterface< ::types::VersionReply>*(::grpc::ClientContext* context, const ::google::protobuf::Empty& request, ::grpc::CompletionQueue* cq));
  MOCK_METHOD3(PrepareAsyncVersionRaw, ::grpc::ClientAsyncResponseReaderInterface< ::types::VersionReply>*(::grpc::ClientContext* context, const ::google::protobuf::Empty& request, ::grpc::CompletionQueue* cq));
  MOCK_METHOD3(ProtocolVersion, ::grpc::Status(::grpc::ClientContext* context, const ::remote::ProtocolVersionRequest& request, ::remote::ProtocolVersionReply* response));
  MOCK_METHOD3(AsyncProtocolVersionRaw, ::grpc::ClientAsyncResponseReaderInterface< ::remote::ProtocolVersionReply>*(::grpc::ClientContext* context, const ::remote::ProtocolVersionRequest& request, ::grpc::CompletionQueue* cq));
  MOCK_METHOD3(PrepareAsyncProtocolVersionRaw, ::grpc::ClientAsyncResponseReaderInterface< ::remote::ProtocolVersionReply>*(::grpc::ClientContext* context, const ::remote::ProtocolVersionRequest& request, ::grpc::CompletionQueue* cq));
  MOCK_METHOD3(ClientVersion, ::grpc::Status(::grpc::ClientContext* context, const ::remote::ClientVersionRequest& request, ::remote::ClientVersionReply* response));
  MOCK_METHOD3(AsyncClientVersionRaw, ::grpc::ClientAsyncResponseReaderInterface< ::remote::ClientVersionReply>*(::grpc::ClientContext* context, const ::remote::ClientVersionRequest& request, ::grpc::CompletionQueue* cq));
  MOCK_METHOD3(PrepareAsyncClientVersionRaw, ::grpc::ClientAsyncResponseReaderInterface< ::remote::ClientVersionReply>*(::grpc::ClientContext* context, const ::remote::ClientVersionRequest& request, ::grpc::CompletionQueue* cq));
  MOCK_METHOD2(SubscribeRaw, ::grpc::ClientReaderInterface< ::remote::SubscribeReply>*(::grpc::ClientContext* context, const ::remote::SubscribeRequest& request));
  MOCK_METHOD4(AsyncSubscribeRaw, ::grpc::ClientAsyncReaderInterface< ::remote::SubscribeReply>*(::grpc::ClientContext* context, const ::remote::SubscribeRequest& request, ::grpc::CompletionQueue* cq, void* tag));
  MOCK_METHOD3(PrepareAsyncSubscribeRaw, ::grpc::ClientAsyncReaderInterface< ::remote::SubscribeReply>*(::grpc::ClientContext* context, const ::remote::SubscribeRequest& request, ::grpc::CompletionQueue* cq));
  MOCK_METHOD3(Block, ::grpc::Status(::grpc::ClientContext* context, const ::remote::BlockRequest& request, ::remote::BlockReply* response));
  MOCK_METHOD3(AsyncBlockRaw, ::grpc::ClientAsyncResponseReaderInterface< ::remote::BlockReply>*(::grpc::ClientContext* context, const ::remote::BlockRequest& request, ::grpc::CompletionQueue* cq));
  MOCK_METHOD3(PrepareAsyncBlockRaw, ::grpc::ClientAsyncResponseReaderInterface< ::remote::BlockReply>*(::grpc::ClientContext* context, const ::remote::BlockRequest& request, ::grpc::CompletionQueue* cq));
  MOCK_METHOD3(TxnLookup, ::grpc::Status(::grpc::ClientContext* context, const ::remote::TxnLookupRequest& request, ::remote::TxnLookupReply* response));
  MOCK_METHOD3(AsyncTxnLookupRaw, ::grpc::ClientAsyncResponseReaderInterface< ::remote::TxnLookupReply>*(::grpc::ClientContext* context, const ::remote::TxnLookupRequest& request, ::grpc::CompletionQueue* cq));
  MOCK_METHOD3(PrepareAsyncTxnLookupRaw, ::grpc::ClientAsyncResponseReaderInterface< ::remote::TxnLookupReply>*(::grpc::ClientContext* context, const ::remote::TxnLookupRequest& request, ::grpc::CompletionQueue* cq));
  MOCK_METHOD3(NodeInfo, ::grpc::Status(::grpc::ClientContext* context, const ::remote::NodesInfoRequest& request, ::remote::NodesInfoReply* response));
  MOCK_METHOD3(AsyncNodeInfoRaw, ::grpc::ClientAsyncResponseReaderInterface< ::remote::NodesInfoReply>*(::grpc::ClientContext* context, const ::remote::NodesInfoRequest& request, ::grpc::CompletionQueue* cq));
  MOCK_METHOD3(PrepareAsyncNodeInfoRaw, ::grpc::ClientAsyncResponseReaderInterface< ::remote::NodesInfoReply>*(::grpc::ClientContext* context, const ::remote::NodesInfoRequest& request, ::grpc::CompletionQueue* cq));
};

} // namespace remote

