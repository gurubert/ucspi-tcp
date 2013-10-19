if [ "${1}" = "-4" -o "${1}" = "-6" ]
then
  vers=${1}
  shift
else
  vers="-6"
fi
host=${1-0}
user=${2-}
port=79
echo "$user" | HOME/command/tcpclient -RHl0 "$vers" -- "$host" "$port" sh -c '
  HOME/bin/addcr >&7
  exec HOME/command/delcr <&6
' | cat -v
