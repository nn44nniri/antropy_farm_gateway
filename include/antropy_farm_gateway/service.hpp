#pragma once
#include "antropy_farm_gateway/grpc_client.hpp"
#include "antropy_farm_gateway/lora_receiver.hpp"
#include "antropy_farm_gateway/optimizer.hpp"
#include "antropy_farm_gateway/ring_buffer.hpp"
#include "antropy_farm_gateway/settings.hpp"
#include "antropy_farm_gateway/sqlite_store.hpp"

namespace antropy::gateway {

class GatewayService {
  public:
    explicit GatewayService(Settings settings);
    void run();

  private:
    Settings settings_;
    SensorHistory history_;
    LoraReceiver receiver_;
    Optimizer optimizer_;
    GrpcClient client_;
    SqliteStore sqlite_store_;
};

} // namespace antropy::gateway
