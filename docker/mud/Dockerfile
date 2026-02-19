FROM ubuntu:22.04

RUN apt-get update && apt-get install -y --no-install-recommends \
    clang \
    g++ \
    make \
    ccache \
    lldb \
    libmysqlclient-dev \
    libboost-filesystem-dev \
    libboost-system-dev \
    libsodium-dev \
    libcurl4-openssl-dev \
    zlib1g-dev \
    libnsl-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

EXPOSE 4000

COPY docker/mud/entrypoint.sh /entrypoint.sh
RUN chmod +x /entrypoint.sh

ENTRYPOINT ["/entrypoint.sh"]
