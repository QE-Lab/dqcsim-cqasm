#!/bin/sh

rm -rf dist

docker build --pull -t dqcsim-plugin-wheel . && \
docker run -it --name dqcsim-plugin-wheel dqcsim-plugin-wheel && \
docker cp dqcsim-plugin-wheel:/io/dist/ .

docker rm dqcsim-plugin-wheel
