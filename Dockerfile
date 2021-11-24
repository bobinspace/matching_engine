FROM debian:10
RUN apt-get update && apt-get -y install build-essential
RUN apt-get -y install \
  cmake \
  iwyu \
  cppcheck \
  && apt-get clean
COPY / /matching_engine
RUN cd /matching_engine \
  && rm -rf build \
  && mkdir -p build \
  && cd build \
  && cmake ../ \
  && make