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

To run the database:

```
docker run -d \
  --name timescaledb \
  -p 5432:5432 \
  -v ./data:/var/lib/postgresql/data \
  -e POSTGRES_PASSWORD=mysecretpassword \
  -e POSTGRES_DB=mytimescaledb \
  timescale/timescaledb:latest-pg16
```

### Connect to the database image

```
docker exec -it timescaledb psql -U postgres -d mytimescaledb
```

## Dependencies

```
sudo apt-get update && sudo apt-get install -y g++ cmake libcurl4-openssl-dev libssl-dev libpqxx-dev libjsoncpp-dev
```
