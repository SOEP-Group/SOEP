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

RUN rm -rf build && mkdir build

WORKDIR /usr/src/SOEP/build

RUN cmake .. && cmake --build .

CMD ["./SOEP"]
