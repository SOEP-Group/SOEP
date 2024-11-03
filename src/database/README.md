### Read

If [initdb/init_satellites](initdb/init_satellites.sql) is missing, generate it through the [codegen](initdb/codegen.py) scirpt in the same folder. The same goes if new data is to be stored.

### run

```sh
docker-compose up -d
```

### test connectivity

```sh
psql -h localhost -p 5432 -U postgres -d mytimescaledb

\q
```