FROM gcc

RUN sed -i 's/deb.debian.org/mirrors.ustc.edu.cn/g' /etc/apt/sources.list
RUN apt update
RUN apt install -y cmake libhiredis-dev redis-server iproute2
RUN mkdir -p /usr/src

COPY . /usr/src/proxy_server

RUN set -ex; \
  cd /usr/src/proxy_server; \
  mkdir -p Build; cd Build; cmake ..; make

WORKDIR /usr/src/proxy_server
ENTRYPOINT ["/bin/sh", "./launch.sh"]
