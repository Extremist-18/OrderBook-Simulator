FROM ubuntu:22.04

RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    git \
    libuv1-dev \
    libssl-dev \
    zlib1g-dev \
    pkg-config \
    nlohmann-json3-dev \
    curl \
    && curl -fsSL https://deb.nodesource.com/setup_20.x | bash - \
    && apt-get install -y nodejs \
    && npm install -g ws \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY . .

# RUN npm install ws
RUN rm -rf build && mkdir build && cd build && cmake .. && make -j orderbook_server

EXPOSE 8080
RUN find /app -name "depthFeed.js"
COPY package*.json ./
RUN npm install

CMD ["sh", "-c", "node depthFeed.js & cd build && ./orderbook_server"]
