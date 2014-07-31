#!/bin/bash

# Clono los repos
git clone https://github.com/sisoputnfrba/so-commons-library
git clone https://github.com/sisoputnfrba/ansisop-parser

# Instalo las commons
sudo apt-get install libcunit1-dev
cd so-commons-library/
sudo make install
cd ..
# Instalo las parser-ansisop
cd ansisop-parser/parser/
sudo make all
sudo make install
cd ..
cd ..
