# xfer
### A file transfer command

##### _Now with encryption!_

Provides a single-line command to transfer files securely\* over the net.

_This program is operated from a command prompt or shell; there is no graphical interface_

#### _What is it for?_
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

#### Some basic use cases...  

 * To listen (server) and receive files: `xfer -l`  
 * To listen (server) and send files: `xfer -l -s -f file1.txt file2.jpg file3.docx...`  
 * To connect (client) and receive: `xfer -c my.host.com`  
 * To connect (client) and send: `xfer -c 1.2.3.4 -s -f file1 file2 file3...`  
 * (linux only) To send entire contents of a directory: `find . 2> /dev/null | xfer -l -s`  
You can pipe file names to the windows version too, but it doesn't have `find`.

_Tip_: use the -v option (verbose) to see more detail.

Invoking the program with no options will yield some usage help text.


##### \* "securely":  
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


### Binary installation

#### Windows:
    Go to the [releases page](https://github.com/DFPercush/xfer/releases) and download the latest xfer.exe.
    Place it in a folder that is in your PATH, or add the folder to your PATH.

#### Linux:
    Binary distributions are not provided at this time. Please compile from source. (see below)


### COMPILING FROM SOURCE

#### Linux:
 * Make sure you have the `cmake` package, as well as `gcc` or `gcc-c++`.
 * The clang compiler is also a valid option if you which to tell CMake to use it.

    git clone --recursive --depth 1 https://github.com/DFPercush/xfer`
    cd xfer
    cmake -B /output/dir -S .
    cmake --build /output/dir --config Release


#### Windows / Visual studio:
You will need [CMake](https://cmake.org) to compile this project.
In order to build OpenSSL, you will also need to install
the [Netwide Assembler (NASM)](https://www.nasm.us/), and [strawberry perl](http://www.strawberryperl.com/).
The commands `cmake`, `perl` and `nasm` should be in your PATH. 

`git clone --recursive --depth 1 https://github.com/DFPercush/xfer`

* Open `cmake-gui` from the start menu. Set the source folder to the xfer folder you just cloned.
* The build/binary folder can be wherever you want the program to be built.
* Click `Configure`, then `Generate`. Choose the Visual Studio version you have installed.
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

