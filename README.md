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
docker run --rm -e N2YO_API_KEY="<env_key>" soep
```
