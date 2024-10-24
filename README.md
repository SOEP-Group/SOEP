# SOEP

Satellite Orbit and Event Predictor (SOEP) is a C++ application that predicts satellite passes, collision risks, orbital decay, and re-entry events. It provides real-time data, visualizations, and a interface for satellite tracking and space object management.

Cmake

```
mkdir build
cd build
cmake ..
cmake --build .
./SOEP
```

Docker

```
docker build -t soep .
docker run -p 8086:8086 --network="host" --rm -it soep
```

## .env File

Make sure you have the .env file in the root directory. You can find the latest .env file in the discord server

## PostgresSQL

[Database](./src/database/)

## Dependencies

```
sudo apt-get update && sudo apt-get install -y g++ cmake libcurl4-openssl-dev libssl-dev libpqxx-dev libjsoncpp-dev
```

## Tests

To run the tests after building the project with cmake:

```
ctest --verbose
```

Or with the executable:

```
./SOEP_tests
```

And for Docker, the tests run automatically during the build process.
