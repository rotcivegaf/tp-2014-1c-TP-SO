#!/bin/bash

# Declaro variables de entorno
export LD_LIBRARY_PATH=/home/utnso/workspace/victor-tp-so/
echo $LD_LIBRARY_PATH
export ANSISOP_CONFIG=./programa/programa_config
echo $ANSISOP_CONFIG
sudo ln -s ./programa.ansisop /usr/bin/ansisop

# Creo el proyecto ansisop
sudo make allc
