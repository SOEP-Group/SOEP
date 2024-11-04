### Read

If [initdb/init_satellites](initdb/init_satellites.sql) is missing, generate it through the [codegen](initdb/codegen.py) script in the same folder. The same goes if new data is to be stored.

### run

```sh
docker-compose up -d
```
###### data for the satellites will be populated as per [initdb/init_satellites](initdb/init_satellites.sql) during compose.

### test connectivity

```sh
psql -h localhost -p 5432 -U postgres -d mytimescaledb

\q
```