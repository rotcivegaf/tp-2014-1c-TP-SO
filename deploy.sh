#!/bin/bash

# Declaro variables de entorno
export LD_LIBRARY_PATH=/home/utnso/workspace/victor-tp-so/
echo $LD_LIBRARY_PATH
export ANSISOP_CONFIG=./programa/programa_config
echo $ANSISOP_CONFIG
sudo ln -s /home/utnso/victor-tp-so/programa.ansisop /usr/bin/ansisop

# Creo el proyecto ansisop
sudo make allc

# Hago ejecutables los scripts
chmod +x ./scripts/completo.ansisop
chmod +x ./scripts/consumidor.ansisop
chmod +x ./scripts/deref-prioridad.ansisop
chmod +x ./scripts/facil.ansisop
chmod +x ./scripts/fibo.ansisop
chmod +x ./scripts/for.ansisop
chmod +x ./scripts/forES.ansisop
chmod +x ./scripts/mcd.ansisop
chmod +x ./scripts/pesado.ansisop
chmod +x ./scripts/productor.ansisop
chmod +x ./scripts/segfault.ansisop
chmod +x ./scripts/stackoverflow.ansisop
chmod +x ./scripts/vector.ansisop
chmod +x ./scripts/bench/productor.ansisop
chmod +x ./scripts/bench/starter.ansisop

