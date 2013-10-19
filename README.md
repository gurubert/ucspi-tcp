ucspi-tcp
=========

http://cr.yp.to/ucspi-tcp.html

What is it?
-----------

tcpserver and tcpclient are easy-to-use command-line tools for building TCP client-server applications.
tcpserver waits for incoming connections and, for each connection, runs a program of your choice. Your program receives environment variables showing the local and remote host names, IP addresses, and port numbers.

tcpserver offers a concurrency limit to protect you from running out of processes and memory. When you are handling 40 (by default) simultaneous connections, tcpserver smoothly defers acceptance of new connections.

tcpserver also provides TCP access control features, similar to tcp-wrappers/tcpd's hosts.allow but much faster. Its access control rules are compiled into a hashed format with cdb, so it can easily deal with thousands of different hosts.

This package includes a recordio tool that monitors all the input and output of a server.

tcpclient makes a TCP connection and runs a program of your choice. It sets up the same environment variables as tcpserver.

This package includes several sample clients built on top of tcpclient: who@, date@, finger@, http@, tcpcat, and mconnect.

tcpserver and tcpclient conform to UCSPI, the UNIX Client-Server Program Interface, using the TCP protocol. UCSPI tools are available for several different networks.

Other command-line TCP tools
----------------------------

The current ucspi-tcp interface is a refinement of the tcpserver/tcpclient interface in my 1991 clientserver package, which replaced the attachport/authtcp interface in my 1989 auth package.
ucspi-tcp now competes with several other programs:

inetd, a root-only TCP server supplied by all UNIX vendors;
xinetd, a replacement for inetd;
the mconnect client supplied as part of SunOS;
the socket program;
faucet and hose, part of the netpipes package; and
the netcat program, which also supports UDP.
Many sites are replacing inetd with tcpserver, for several reasons:

inetd is unreliable under high loads. It cuts off service for 10 minutes if it receives ``too many'' connections in 1 minute.
inetd does not provide effective resource management. It will happily use up all your memory if you are running a popular service.
inetd has trouble with sudden bursts of activity. Its listen() backlog is typically only 5 or 10 and cannot be raised.
