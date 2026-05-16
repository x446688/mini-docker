# mini-docker

1. Run Makefile
```
sudo make
sudo make install
```
2. Execute the program
```
sudo /usr/bin/mini-docker [ARGS]
```

## Available args:

`-h --help` - Print help menu \
`-m --mount` - Mountpoint \
`-c --command filename` - Execute the command \
`-l --log_file filename ` - Write logs to the file \
`-p --pid_file filename` - PID file used by daemonized app \
`-d --daemon` - Daemonize this application

> Note: The `--mount` option already assumes you have a ready minrootfs environment set up in that directory. Use the bash script `TODO.sh` to generate one if you're unsure. 

## Daemonization:

```
sudo systemctl start mini-container
sudo systemctl status mini-container
```
