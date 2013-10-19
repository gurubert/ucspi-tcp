if [ "${1}" = "-4" -o "${1}" = "-6" ]
then
  vers=${1}
  shift
else
  vers="-6"
fi
host=${1-0}
port=11
HOME/command/tcpclient -RHl0 "$vers" -- "$host" "$port" sh -c 'exec HOME/command/delcr <&6' | cat -v
