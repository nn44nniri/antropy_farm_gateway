#include "antropy_farm_gateway/service.hpp"
#include <iostream>

namespace antropy::gateway {

GatewayService::GatewayService(Settings settings)
    : settings_(std::move(settings)),
      history_(settings_.algorithm.rolling_window_size),
      receiver_(settings_.lora),
      optimizer_(settings_.algorithm),
      client_(settings_.grpc),
      sqlite_store_(settings_.database.path) {}

void GatewayService::run() {
    receiver_.receive_loop([this](const SensorReading& reading) {
        if (settings_.database.enabled) sqlite_store_.insert_reading(reading);
        history_.push(reading);
        const auto sync = history_.synchronized_sample_count();
        if (sync < settings_.algorithm.min_history_per_sensor) return;

        const auto temp_report = optimizer_.run(history_.temperature_series(), "temperature_c");
        const auto hum_report = optimizer_.run(history_.humidity_series(), "humidity_pct");

        const bool ok1 = client_.push_report(settings_.facility, history_, temp_report);
        const bool ok2 = client_.push_report(settings_.facility, history_, hum_report);
        std::cout << "Processed sensor " << reading.sensor_id
                  << " sync_samples=" << sync
                  << " latest_temp_c=" << reading.temperature_c
                  << " latest_humidity_pct=" << reading.humidity_pct
                  << " uds_temp_sent=" << (ok1 ? 1 : 0)
                  << " uds_humidity_sent=" << (ok2 ? 1 : 0) << std::endl;
    });
}

} // namespace antropy::gateway
