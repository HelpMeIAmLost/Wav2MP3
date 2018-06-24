# Wav2MP3
Multi-threaded Wav to MP3 encoder using LAME for Windows/Linux

## Requirements
*  MingGW - https://sourceforge.net/projects/mingw/files/Installer/
   *  GCC C++ compiler
   *  MSYS Basic System
   *  MinGW Developer Toolkit
   *  MinGW-get
   Open a MinGW shell and install the following:
     ```
	 mingw-get install gcc
	 mingw-get install mingw-utils
	 ```
	
*  LAME encoder source files (https://sourceforge.net/projects/lame/files/lame/)
   Go to extract folder and run the following commands:
     ```
     ./configure --prefix=/mingw --disable-shared --enable-nasm --enable-static
     make
     make install
     ```
   Note: If there is a langinfo.h error, go to http://blog.k-tai-douga.com/article/35965219.html
     ```
     patch -p1 < lame-3.100-parse_c.diff
     ```
*  pthreads
   `mingw-get install pthreads`

*  Compile using `make`   
   
## Usage
### For Windows
   `C:\\\\PathToWavFiles`
   
### For Linux
   `/usr/local/pathtowavfiles`
   
## Notes
This application encodes WAV files to mono, 44,100 Hz MP3 files and saves the encoded files in the same user-defined directory.