# xfer
A simple, session based, tcp peer file transfer program written in C++.
Operates from the command line.
Either end can connect or host the connection.
Sending and receiving files is treated separately from establishing the connection, so 
the party with more knowledge and access to their firewall will typically host. 
If both parties need to send files to the other, two separate sessions will be required, as 
this program only knows whether it should act as sender or receiver.

The files will be saved in the receiver's current working directory. 
Relative paths are truncated. If you need to transfer a directory structure,
zip or tar it first. 

Don't run this program from a system folder like C:\Windows or /usr/bin
If I need to explain why, you probably shouldn't be using this.

Some basic use cases...
To listen and receive files: xfer -l
To listen and send files: xfer -l -s -f file1 file2 file3...
To connect and receive: xfer -c my.host.com 
To connect and send: xfer -c 1.2.3.4 -s -f file1 file2 file3...

Tip: use the -v option (verbose) to see more detail.

Invoking the program with no options will yield some usage help text.

This program does not use UPnP or any kind of router/firewall magic. (yet?)
If both parties are behind firewalls that they are unable to configure, 
another solution may be required.

Once all files have been sent, the connection is closed and program terminates.
It doesn't have much in the way of security, except for this single-connection design.
When a connection is accepted, the listening port is immediately closed before any
data transfer begins. This means that, if anyone hijacks the session, you'll know
about it, because the legitimate peer will be unable to connect. "It wasn't me!"

No files from the host machine will be made available
unless you explicitly list them on the command line. The sender determines which files will
be sent. There is no way for a receiver/peer to request a certain file.

The transfer mode is always binary. No newline conversions are present. The headers use a single LF.

If you get missing DLL errors on Windows binary, please install the Visual C++ runtime from https://www.microsoft.com/en-us/download/details.aspx?id=48145
