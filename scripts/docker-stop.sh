#!/usr/bin/env bash
set -euo pipefail
docker compose stop antropy_farm_gateway && docker compose rm -f antropy_farm_gateway
