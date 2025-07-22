#!/bin/bash
BIN=bin/webserver
DEFAULT_LOGDIR=logs
INST=${2:-default}
PIDFILE=webserver_${INST}.pid

MAX_RESTARTS=${MAX_RESTARTS:-5}   # 最大自动重启次数
RESTART_INTERVAL=2                # 崩溃后重启间隔（秒）
HEALTH_CHECK_INTERVAL=10          # 健康检查间隔（秒）

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
    MODE=${4:-bg} # 默认后台
    if [ "$MODE" = "fg" ]; then
        color_echo blue "[INFO] WebServer ($INST) running in foreground."
        $BIN
    else
        color_echo blue "[INFO] WebServer ($INST) running as daemon."
        (
            RESTARTS=0
            while [ $RESTARTS -lt $MAX_RESTARTS ]; do
                nohup $BIN >/dev/null 2>&1 &
                CHILD_PID=$!
                color_echo green "[OK] WebServer ($INST) started, PID: $CHILD_PID (restart $RESTARTS/$MAX_RESTARTS)"
                wait $CHILD_PID
                EXIT_CODE=$?
                RESTARTS=$((RESTARTS+1))
                color_echo red "[ERR] WebServer ($INST) crashed (exit $EXIT_CODE), restarting in $RESTART_INTERVAL s..."
                sleep $RESTART_INTERVAL
            done
            color_echo red "[FATAL] WebServer ($INST) reached max restarts ($MAX_RESTARTS), giving up."
            rm -f $PIDFILE
        ) &
        DAEMON_PID=$!
        echo $DAEMON_PID > $PIDFILE
        # 健康检查守护进程
        (
            while true; do
                sleep $HEALTH_CHECK_INTERVAL
                if [ -f $PIDFILE ]; then
                    PID=$(cat $PIDFILE)
                    if ! kill -0 $PID 2>/dev/null; then
                        color_echo red "[HEALTH] WebServer ($INST) daemon not running, attempting restart..."
                        start $INST bg
                        break
                    fi
                fi
            done
        ) &
    fi
}

stop() {
    if [ -f $PIDFILE ]; then
        PID=$(cat $PIDFILE)
        if kill $PID 2>/dev/null; then
            color_echo blue "[INFO] Stopping WebServer ($INST) (daemon PID: $PID)..."
            rm -f $PIDFILE
        else
            color_echo yellow "[WARN] Daemon process $PID not running. Removing stale PID file."
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
            color_echo green "[OK] WebServer ($INST) is running. Daemon PID: $PID"
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