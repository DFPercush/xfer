xfer - A file transfer command

Now with encryption!

Provides a single-line command to transfer files securely* over the net.
(Requires basic knowledge of how to use a command line.)

When you just need to send a file or two between two computers on your network, or over the internet,
but you don't want to send your data through a third party service, and can't be bothered
to set up a full FTP or HTTP server, or install some flavor of remote desktop software.

- Either sender or receiver can connect, or host the connection, depending on who has more
  knowledge and access to their firewall. If both parties need to send files to the other,
  two separate sessions will be required.

- The files will be saved relative to the receiver's current working directory,
  with the parent directory token .. explicitly forbidden.
  Don't run this program from a system folder like C:\Windows or /usr/bin,
  unless you're trying to overwrite critical system files and are extremely trusting.

Some basic use cases...
To listen and receive files: xfer -l
To listen and send files: xfer -l -s -f file1.txt file2.jpg file3.docx...
To connect and receive: xfer -c my.host.com 
To connect and send: xfer -c 1.2.3.4 -s -f file1 file2 file3...

Tip: use the -v option (verbose) to see more detail.

Invoking the program with no options will yield some usage help text.


* "securely":
    Although there are many similarities, this program does not use the official TLS standard
  or check any certificates. It uses OpenSSL to encrypt the data, just like many mainstream
  secure programs, but does not conform to a strict transfer protocol standard.There is no proof of 
  identity between parties. It only guarantees that whatever is sent, is accurately received, and 
  not viewable by any third party during transit. Encryption keys are generated new for each session
  and not stored anywhere on the file system.
    The idea is to be in communication with the other party in a live setting, and verify the
  connection via phone or text/chat. It only accepts one connection before closing the port,
  so you will know if that's the right person by their confirmation or IP address.

Note: The OpenSSL library on the windows build is linked statically, to avoid dependence on extra DLLs.
      This allows a single .exe file to be distributed without requiring other files packaged with it.
      This contributes most of the size of the program.

This program does not use UPnP or any kind of router/firewall magic. (yet?)
If both parties are behind firewalls that they are unable to configure, 
another solution may be required.

No files from the host machine will be made available unless you explicitly list them on 
the command line. The sender determines which files will be sent. There is no way for a
receiver to request a certain file.

The transfer mode is always binary. No newline conversions are present. The headers use a single LF ('\n').

If you get missing DLL errors on Windows binary, please install the Visual C++ runtime from https://www.microsoft.com/en-us/download/details.aspx?id=48145
