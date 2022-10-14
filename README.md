## build

__build locally__

```
mkdir build && cd build && cmake .. && make
# run server
./src/server
# run client
./src/client
```

__build docker__

```
docker pull gcc
docker build . -t <tag>
```
