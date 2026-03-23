FROM debian:bookworm-slim

RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential cmake pkg-config git \
    libsqlite3-dev libboost-all-dev ca-certificates && \
    rm -rf /var/lib/apt/lists/*

WORKDIR /opt/antropy_farm_gateway
COPY . .
RUN cmake -S . -B build && cmake --build build -j$(nproc)

RUN printf '#!/bin/sh\nexec /opt/antropy_farm_gateway/build/antropy_farm_gateway /opt/antropy_farm_gateway/config/settings.json\n' > /usr/local/bin/antropy_farm_gateway && chmod +x /usr/local/bin/antropy_farm_gateway

CMD ["/usr/local/bin/antropy_farm_gateway"]
