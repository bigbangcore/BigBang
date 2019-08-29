#!/bin/bash

sudo apt install -y git
git clone https://github.com/bigbangcore/BigBang.git
sudo apt-get update -qq;
sudo apt install -y g++ cmake libboost-all-dev openssl libssl1.0-dev libreadline-dev libncurses5-dev pkg-config libsodium-dev libprotobuf-dev protobuf-compiler;
cd BigBang
./INSTALL.sh