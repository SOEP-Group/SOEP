FROM ubuntu:22.04 AS builder

RUN apt-get update && apt-get install -y --no-install-recommends \
    gcc \
    g++ \
    make \
    cmake \
    libcurl4-openssl-dev \
    libssl-dev \
    libpqxx-dev \
    python3 \
    python3-dev \
    python3-pip \
    pkg-config \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /usr/src/SOEP

COPY . .

RUN if [ ! -f ".env" ]; then \
    echo -e "\033[0;31mERROR: No .env file detected, the program might crash. Ask Shakir for it.\033[0m"; \
    fi

RUN rm -rf build && mkdir build && cd build \
    && cmake .. -DCMAKE_BUILD_TYPE=Release \
    && cmake --build . --config Release \
    && strip SOEP \
    && ctest --verbose

FROM ubuntu:22.04 AS runtime

RUN apt-get update && apt-get install -y --no-install-recommends \
    libcurl4 \
    libssl3  \
    libjsoncpp25 \
    python3 \
    python3-dev \
    python3-pip \
    libpqxx-dev \
    && apt-get purge --auto-remove -y \
    && rm -rf /var/lib/apt/lists/* /tmp/* /var/tmp/*

RUN pip3 install --no-cache-dir sgp4


WORKDIR /usr/src/SOEP


COPY --from=builder /usr/src/SOEP/build/SOEP /usr/src/SOEP/SOEP
COPY --from=builder /usr/src/SOEP/build/.env /usr/src/SOEP/.env
COPY --from=builder /usr/src/SOEP/resources /usr/src/SOEP/resources
COPY --from=builder /usr/src/SOEP/bin/sgp4_module.cpython-310-x86_64-linux-gnu.so /usr/src/SOEP/sgp4_module.cpython-310-x86_64-linux-gnu.so

ENV PYTHONPATH=/usr/src/SOEP


EXPOSE 8086

CMD ["./SOEP"]