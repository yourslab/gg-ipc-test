#include <aws/crt/Api.h>
#include <aws/ipc/GGIpcClient.h>
#include <cstdio>
#include <iostream>

using namespace Aws::Eventstreamrpc;

class TestLifecycleHandler : public ConnectionLifecycleHandler {
public:
  TestLifecycleHandler() {
    isConnected = false;
    lastErrorCode = AWS_OP_ERR;
  }

  void OnConnectCallback() override {
    std::cout << "OnConnectCallback" << std::endl;
    isConnected = true;
  }

  void OnDisconnectCallback(int errorCode) override {
    std::cout << "OnDisconnectCallback: " << errorCode << std::endl;
    lastErrorCode = errorCode;
  }

  bool OnErrorCallback(int errorCode) override {
    std::cout << "OnErrorCallback: " << errorCode << std::endl;
    std::cout << aws_error_str(errorCode) << std::endl;
    lastErrorCode = errorCode;
    return true;
  }

public:
  int isConnected;
  int lastErrorCode;
};

class CustomSubscribeHandler : public Ipc::SubscribeToTopicStreamHandler {
  void OnStreamEvent(Ipc::SubscriptionResponseMessage *response) override {
    auto binaryMessage = response->GetBinaryMessage();
    Aws::Crt::JsonObject serializedResponse;

    if (binaryMessage.has_value()) {
      binaryMessage.value().SerializeToJsonObject(serializedResponse);
    }
    std::cout << "This is the response "
              << serializedResponse.View().WriteReadable() << std::endl;
  }
};

void test_publish(Aws::Crt::String topic) {
  Aws::Crt::Allocator *allocator = Aws::Crt::g_allocator;
  Aws::Crt::ApiHandle apiHandle(allocator);
  Aws::Crt::Io::TlsContextOptions tlsCtxOptions =
      Aws::Crt::Io::TlsContextOptions::InitDefaultClient();
  Aws::Crt::Io::TlsContext tlsContext(tlsCtxOptions,
                                      Aws::Crt::Io::TlsMode::CLIENT, allocator);

  Aws::Crt::Io::TlsConnectionOptions tlsConnectionOptions =
      tlsContext.NewConnectionOptions();

  Aws::Crt::Io::EventLoopGroup eventLoopGroup(0, allocator);

  UnixSocketResolver unixSocketResolver(eventLoopGroup, 8, allocator);

  Aws::Crt::Io::ClientBootstrap clientBootstrap(eventLoopGroup,
                                                unixSocketResolver, allocator);
  clientBootstrap.EnableBlockingShutdown();

  TestLifecycleHandler lifecycleHandler;
  std::cout << "creating client" << std::endl;
  Ipc::GreengrassIpcClient client(clientBootstrap);
  std::cout << "attempting connect" << std::endl;
  auto connectedStatus =
      client.Connect(lifecycleHandler);
  std::cout << "get connect status" << std::endl;
  connectedStatus.get();

  std::cout << "create operation" << std::endl;
  // Ipc::PublishToTopicOperation operation = client.NewPublishToTopic();
  Aws::Crt::String messageString("Hello World");
  Aws::Crt::Vector<uint8_t> messageData(
      {messageString.begin(), messageString.end()}, allocator);
  Ipc::BinaryMessage binaryMessage(allocator);
  binaryMessage.SetMessage(messageData);
  Ipc::PublishMessage publishMessage(allocator);
  publishMessage.SetBinaryMessage(binaryMessage);
  // Ipc::PublishMessage publishMessage(allocator);
  std::cout << "create request" << std::endl;
  Ipc::PublishToTopicRequest publishRequest(allocator);
  publishRequest.SetTopic(topic);
  publishRequest.SetPublishMessage(publishMessage);
  CustomSubscribeHandler streamHandler;
  Ipc::SubscribeToTopicOperation subscribeOperation =
      client.NewSubscribeToTopic(streamHandler);
  // Ipc::PublishMessage publishMessage(allocator);
  Ipc::SubscribeToTopicRequest subscribeRequest(allocator);
  subscribeRequest.SetTopic(topic);
  std::cout << "subscribing" << std::endl;
  auto subscribeActivate =
      subscribeOperation.Activate(subscribeRequest, nullptr);
  subscribeActivate.wait();
  std::cout << "getting response" << std::endl;
  auto futureResponse = subscribeOperation.GetOperationResult();
  auto response = futureResponse.get();
  if (response) {
    std::cout << "Sent successfully" << std::endl;
    Ipc::SubscribeToTopicResponse *subscribeToTopicResponse =
        static_cast<Ipc::SubscribeToTopicResponse *>(
            response.GetOperationResponse());
  } else {
    auto errorType = response.GetResultType();
    if (errorType == OPERATION_ERROR) {
      /* User can cast this to whatever "Error" type they desire
      e.g. auto *error = static_cast<Ipc::UnauthorizedError*>(response.GetOperationError());
      Although, normally, we are only concerned about the error message.
      */
      auto *error = response.GetOperationError();
      std::cout << error->GetMessage().value() << std::endl;
    } else {
      std::cout << response.GetRpcError() << std::endl;
    }
  }

  auto publishOperation = client.NewPublishToTopic();
  std::cout << "publishing" << std::endl;
  auto publishActivate = publishOperation.Activate(publishRequest, nullptr);
  publishActivate.wait();
  std::cout << "getting response" << std::endl;
  auto publishFutureResponse = publishOperation.GetOperationResult();
  auto publishResponse = publishFutureResponse.get();
  if (publishResponse) {
    std::cout << "Sent successfully" << std::endl;
  } else {
    auto errorType = publishResponse.GetResultType();
    if (errorType == RPC_ERROR) {
      std::cout << publishResponse.GetRpcError() << std::endl;
    } else {
      auto *error = publishResponse.GetOperationError();
      (void)error;
    }
  }

  // Sleep so that we give some time to receive
  std::this_thread::sleep_for(std::chrono::seconds(20));

  client.Close();
}

int main(int argc, char *argv[]) {
  printf("arguments:\n");
  for (int i = 0; i < argc; i++) {
    printf("%s\n", argv[i]);
  }

  test_publish(Aws::Crt::String("topic"));
  return 0;
}
