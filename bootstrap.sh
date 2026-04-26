#!/usr/bin/env bash
if [ $(uname -o) == "Darwin" ]; then
    colima delete -f
    colima start
    docker build -t dolphian:latest .  
    ./run.sh
elif [ $(uname -o) == "GNU/Linux" ]; then
    echo "GNU/Linux: do nothing"
fi
