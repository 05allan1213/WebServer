#!/bin/bash
BIN=bin/webserver
DEFAULT_LOGDIR=logs
INST=${2:-default}
PIDFILE=webserver_${INST}.pid

color_echo() {
    local color="$1"; shift
    case $color in
        red)    echo -e "\033[31m$@\033[0m" ;;
        green)  echo -e "\033[32m$@\033[0m" ;;
        yellow) echo -e "\033[33m$@\033[0m" ;;
        blue)   echo -e "\033[34m$@\033[0m" ;;
        *)      echo "$@" ;;
    esac
}

start() {
    if [ -f $PIDFILE ] && kill -0 $(cat $PIDFILE) 2>/dev/null; then
        color_echo yellow "[WARN] WebServer ($INST) is already running. PID: $(cat $PIDFILE)"
        exit 1
    fi
    MODE=${4:-bg}
    if [ "$MODE" = "fg" ]; then
        color_echo blue "[INFO] WebServer ($INST) running in foreground."
        $BIN
    else
        color_echo blue "[INFO] WebServer ($INST) running as daemon."
        nohup $BIN >/dev/null 2>&1 &
        CHILD_PID=$!
        echo $CHILD_PID > $PIDFILE
        color_echo green "[OK] WebServer ($INST) started, PID: $CHILD_PID"
    fi
}

stop() {
    if [ -f $PIDFILE ]; then
        PID=$(cat $PIDFILE)
        if kill $PID 2>/dev/null; then
            color_echo blue "[INFO] Stopping WebServer ($INST) (PID: $PID)..."
            rm -f $PIDFILE
        else
            color_echo yellow "[WARN] Process $PID not running. Removing stale PID file."
            rm -f $PIDFILE
        fi
    else
        color_echo yellow "[WARN] No PID file found for instance '$INST'. Is WebServer running?"
    fi
}

status() {
    if [ -f $PIDFILE ]; then
        PID=$(cat $PIDFILE)
        if kill -0 $PID 2>/dev/null; then
            color_echo green "[OK] WebServer ($INST) is running. PID: $PID"
        else
            color_echo red "[ERR] WebServer ($INST) not running, but PID file exists."
        fi
    else
        color_echo yellow "[INFO] WebServer ($INST) is not running."
    fi
}

restart() {
    stop
    sleep 1
    start $1 $2 $3 $4
}

case "$1" in
    start) start $@ ;;
    stop) stop $@ ;;
    restart) restart $@ ;;
    status) status $@ ;;
    *) echo "Usage: $0 {start|stop|restart|status} [instance] [bg|fg]" ;;
esac 