# xfer
### A file transfer command

##### _Now with encryption! _

Provides a single-line command to transfer files securely\* over the net.

_This program is operated from a command prompt or shell; there is no graphical interface_

##### _What is it for?_
When you just need to send a file or two between two computers on your network, or over the internet,
but you don't want to send your data through a third party service, and can't be bothered
to set up a full FTP or HTTP server, or install some flavor of remote desktop software.

Either sender or receiver can connect, or host the connection, depending on who has more
knowledge and access to their firewall. If both parties need to send files to the other,
two separate sessions will be required.

The files will be saved relative to the receiver's current working directory,
with the parent directory token `..` explicitly forbidden.
Don't run this program from a system folder like `C:\Windows` or `/usr/bin`,
unless you're trying to overwrite critical system files and are extremely trusting.

Some basic use cases...  

 * To listen (server) and receive files: `xfer -l`  
 * To listen (server) and send files: `xfer -l -s -f file1.txt file2.jpg file3.docx...`  
 * To connect (client) and receive: `xfer -c my.host.com`  
 * To connect (client) and send: `xfer -c 1.2.3.4 -s -f file1 file2 file3...`  

_Tip_: use the -v option (verbose) to see more detail.

Invoking the program with no options will yield some usage help text.


\* "securely":  
    Although there are many similarities, this program does not use the official TLS standard
  or check any certificates. It uses OpenSSL to encrypt the data, just like many mainstream
  secure programs, but does not conform to a strict transfer protocol standard.There is no proof of 
  identity between parties. It only guarantees that whatever is sent, is accurately received, and 
  not viewable by any third party during transit. Encryption keys are generated new for each session
  and not stored anywhere on the file system.
    The idea is to be in communication with the other party in a live setting, and verify the
  connection via phone or text/chat. It only accepts one connection before closing the port,
  so you will know if that's the right person by their confirmation or IP address.

_Note_: The OpenSSL library on the windows build is linked statically, to avoid dependence on extra DLLs.
This allows a single .exe file to be distributed without requiring other files packaged with it.
This contributes most of the size of the program.

This program does not use UPnP or any kind of router/firewall magic. (yet?)
If both parties are behind firewalls that they are unable to configure, 
another solution may be required.

No files from the host machine will be made available unless you explicitly list them on 
the command line. The sender determines which files will be sent. There is no way for a
receiver to request a certain file.

The transfer mode is always binary. No newline conversions are present. The headers use a single LF ('\n').



### COMPILING FROM SOURCE

#### Linux:
 * Get the packages `gcc-c++` and `openssl-devel` from your package manager.

 * `git clone https://github.com/DFPercush/xfer`

 * `make`

 That's about it. I'm not in the habit of using configure scripts, so if 
something is weird about your system that makes this not compile, post
    an issue about it and we'll go from there.

#### Windows / Visual studio:
* First, you must install the [OpenSSL libraries](https://wiki.openssl.org/index.php/Compilation_and_Installation). That is a topic unto itself, but, as in my case, if you also want to build those from source, you will need a perl interpreter, as well as the [Netwide Assembler (NASM)](https://www.nasm.us/). I used [strawberry perl](http://www.strawberryperl.com/), which is free and requires no registration, but the official guide recommends [Active Perl](http://www.activestate.com/ActivePerl).

* This project expects the OpenSSL libraries to be in a parallel folder named `openssl-32` or `openssl-64`, i.e., if you clone this project into `Source\Repos\xfer`, there should be a `Source\Repos\openssl-64` with the files `libcrypto_static.lib` and `libssl_static.lib`. You can use the dynamic versions without the `_static` if you want, but you'll have to drag a couple of DLLs around wherever you put this.
* If you have git for windows, clone this repo, or just download the zip file
  * `git clone https://github.com/DFPercush/xfer`

* Open `xfer.sln` in visual studio.
* Press `F7` or Build solution. I suggest using Release / x64 mode for better performance.

### INSTALLATION

#### Linux:

* `sudo cp ./bin/xfer /usr/bin`  
       ... or somewhere your shell can find it

#### Windows 10:
You can either copy `xfer.exe` to a known path like Windows\system32, or modify your `%PATH%` environment variable with these steps:

* Click on the windows/start button and start typing "advanced system settings" - open it
* Click the button at the bottom "Environment Variables"
* Select the variable "Path" and click the Edit button.
* Click "New" and put the full path of the folder where xfer.exe is located.  
This can be copied and pasted from the address bar in File Explorer

   Now you can use the command 'xfer' from the command prompt.

  If you get missing DLL errors on Windows binary, please install
  [The latest Visual C++ redistributable](https://support.microsoft.com/en-us/help/2977003/the-latest-supported-visual-c-downloads)

