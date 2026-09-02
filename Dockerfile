FROM ubuntu:22.04

RUN apt-get update && apt-get install -y \
    build-essential cmake git libuv1-dev libssl-dev zlib1g-dev pkg-config nlohmann-json3-dev curl \
    && curl -fsSL https://deb.nodesource.com/setup_20.x | bash - \
    && apt-get install -y nodejs \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY . .

RUN rm -rf build && mkdir build && cd build && cmake .. && make -j orderbook_server
RUN echo '{"name":"feeder","version":"1.0.0","dependencies":{"ws":"^8.14.2"}}' > package.json

RUN npm install

EXPOSE 8080

CMD sh -c "node depthFeed.js & cd build && ./orderbook_server"