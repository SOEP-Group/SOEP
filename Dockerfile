FROM ubuntu:22.04 as builder

RUN apt-get update && apt-get install -y \
    g++ \
    cmake \
    libcurl4-openssl-dev \
    libssl-dev \
    libpqxx-dev \
    pkg-config

WORKDIR /usr/src/SOEP

COPY . .

RUN if [ ! -f ".env" ]; then \
    echo -e "\033[0;31mERROR: No .env file detected, the program might crash. Ask Shakir for it.\033[0m"; \
    fi

RUN rm -rf build && mkdir build
WORKDIR /usr/src/SOEP/build
RUN cmake .. && cmake --build .
RUN ctest --verbose

FROM ubuntu:22.04

RUN apt-get update && apt-get install -y \
    libcurl4 \
    libssl3  \
    libjsoncpp25 \
    libpqxx-dev

WORKDIR /usr/src/SOEP

COPY --from=builder /usr/src/SOEP/build/SOEP /usr/src/SOEP/SOEP
COPY --from=builder /usr/src/SOEP/build/.env /usr/src/SOEP/.env

EXPOSE 8086

CMD ["./SOEP"]