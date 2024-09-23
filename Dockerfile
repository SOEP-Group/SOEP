# Dockerfile

FROM ubuntu:22.04

# Install dependencies
RUN apt-get update && apt-get install -y \
    g++ \
    cmake \
    libcurl4-openssl-dev \
    libssl-dev \
    libjsoncpp-dev \
    nlohmann-json3-dev

WORKDIR /usr/src/SOEP

COPY . .

RUN if [ ! -f ".env" ]; then \
    echo -e "\033[0;31mERROR: .env file not found.\033[0m"; \
    exit 1; \
    fi

RUN rm -rf build && mkdir build

WORKDIR /usr/src/SOEP/build

RUN cmake .. && cmake --build .

CMD ["./SOEP"]
