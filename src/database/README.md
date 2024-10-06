### run

```sh
docker-compose up -d
```

### test connectivity

```sh
psql -h localhost -p 5432 -U postgres -d mytimescaledb

\q
```