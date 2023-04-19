#!/bin/bash

# getting required libraries
mkdir -p include
mkdir -p include/nlohmann
wget https://github.com/nlohmann/json/releases/download/v3.11.2/json.hpp -O include/nlohmann/json.hpp

# building Report Pass and the external reporter
make 