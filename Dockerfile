FROM ubuntu:22.04

RUN apt-get update && apt-get upgrade -y

RUN apt-get update && apt-get install -y \
    g++ \
    cmake

WORKDIR /usr/src/SOEP

COPY . .

RUN rm -rf build && mkdir build

WORKDIR /usr/src/SOEP/build

RUN cmake .. && cmake --build .

CMD ["./SOEP"]
