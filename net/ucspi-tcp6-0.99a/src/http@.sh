if [ "${1}" = "-4" -o "${1}" = "-6" ]
then
  vers=${1}
  shift
else
  vers="-6"
fi
host=${1-0}
path=${2-}
port=${3-80}
args=""
if [ $# -gt 3 ]
then
  shift; shift; shift
  args="$@"
fi
echo "GET /$path HTTP/1.0
Host: $host:$port
" | HOME/command/tcpclient -RHl0 "$vers" $args -- "$host" "$port" sh -c '
  HOME/command/addcr >&7
  exec HOME/command/delcr <&6
' | awk '/^$/ { body=1; next } { if (body) print }'
