if [ "${1}" = "v4" -o "${1}" = "v6" ]
then
  vers=${1}
  shift
else
  vers="-6"
fi
host=${1-0}
port=${2-17}
exec HOME/command/tcpclient -RHl0 "$vers" -- "$host" "$port" sh -c 'exec cat <&6'
