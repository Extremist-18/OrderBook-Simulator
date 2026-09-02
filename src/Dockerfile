FROM ubuntu:22.04

RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    git \
    libuv1-dev \
    libssl-dev \
    zlib1g-dev \
    && rm -rf /var/lib/apt/lists/*


WORKDIR /app
COPY . .

RUN mkdir build && cd build && cmake .. && make -j

EXPOSE 8080

CMD ["sh", "-c", "cd build && ./orderbook_server"]