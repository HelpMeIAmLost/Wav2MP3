/* Wav2MP3
 * ---
 * WAV to MP3 file converter.
 *
 * Features:
 *   - Multi-threading
 *   - LAME encoding
 *
 * Requirements:
 *   MingGW - https://sourceforge.net/projects/mingw/files/Installer/
 *     - GCC C++ compiler
 *     - MSYS Basic System
 *     - MinGW Developer Toolkit
 *   Open a MinGW shell and install the following:
 *     mingw-get install gcc
 *     mingw-get install mingw-utils
 *   Download Yasm.exe and copy to mingw/bin folder
 *   Download LAME encoder source and extract
 *   Go to extract folder and run the following commands:
 *     ./configure --prefix=/mingw --disable-shared --enable-nasm --enable-static
 *     make
 *     make install
 *
 *     Note: If there is a langinfo.h error, go to http://blog.k-tai-douga.com/article/35965219.html
 *       patch -p1 < lame-3.100-parse_c.diff
 *   Install pthreads
 *     mingw-get install pthreads
 *************************
 *   MSys2
 *   GCC Compiler - pacman -S mingw-w64-x86_64-gcc
 *   LAME encoder - pacman -S mingw-w64-x86_64-lame
 *   pacman -S mingw-w64-x86_64-gcc mingw-w64-x86_64-lame
 *
 *
 *
 *
 **************************/

#include <iostream>
#include <string>
#include <algorithm>
#include <stdio.h>
#include <libgen.h>
#include <inttypes.h>
#include <dirent.h>
#ifdef _WIN32
#include <windows.h>
#elif MACOS
#include <sys/param.h>
#include <sys/sysctl.h>
#else
#include <unistd.h>
#endif
#include <pthread.h>    // POSIX thread
#include <lame/lame.h>  // LAME encoder
#include <sys/types.h>
#include <sys/stat.h>

using namespace std;

#define WAV_SIZE 8192
#define MP3_SIZE 8192
#define CHANNEL 1

//void *convFiles(void *threadid);
void *convFiles(void *);
int getNumberOfCores(void);
void swap_endian16(unsigned char[]);
void swap_endian32(unsigned char[]);

struct procInfo {
	//int threadid;
	string wavPath;
	string wavFile;
	unsigned int numChannels;
	unsigned int sampleRate;
	string mp3File;
	unsigned int status;
};

static pthread_mutex_t mutFilesFinished = PTHREAD_MUTEX_INITIALIZER;
static int fileListIndex = 0;
static int numFiles;
struct procInfo *inputInfo = NULL;
static bool start = false;

string *filelist = NULL;

int main(int argc, char *argv[])
{
	pthread_t *l_threads = NULL;
	FILE *wavFile;
	unsigned char fileFormat[4];
    unsigned char l_numChannels[2];
    unsigned char l_sampleRate[4];
	unsigned int l_numthreads;
	int rc, ret;
	intptr_t l_index;
	string l_file;
	string l_path;
	bool l_valid;
	struct stat l_pathinfo;
    DIR *dir;
    struct dirent *entry;
    char *temp1, *temp2;
    size_t found1, found2;

	cout << "Multi-threaded WAV to MP3 Converter v1.0.0" << endl;
	cout << "by Rowen Chumacera" << endl << endl;
	cout << "Using LAME version " << get_lame_version() << endl << endl;

	l_valid = false;

    // For debugging
//    argc = 2;
//    argv[1] = "C:\\git\\Wav2MP3\\test";

	// Check for arguments
	if (argc == 2)
	{
		// Check if the argument is only for one file
		// different member versions of find in the same order as above:
		l_path = argv[1];

		// Validate command-line argument if it contains for \\ or /
		found1 = l_path.rfind('\\');
		found2 = l_path.rfind('/');
		if (found1 != string::npos || found2 != string::npos)
		{
		    replace(l_path.begin(), l_path.end(), '\\', '/');

            if (stat(l_path.c_str(), &l_pathinfo) != 0)
            {
                cout << "cannot access " << l_path << endl;
                exit(-1);
            }
            else if (l_pathinfo.st_mode & S_IFDIR)
            {
                cout << "Path: " << l_path << endl;
                //l_path = l_path + "/*.wav";

                dir = opendir(l_path.c_str());
                if (dir != NULL)
                {
                    l_numthreads = getNumberOfCores();
                    // Check if the number of supported concurrent threads is not well-defined or cannot be computed
                    if (l_numthreads == 0)
                    {
                        l_numthreads = 1;
                    }

                    // For checking the number of concurrent threads possible
                    cout << l_numthreads << " concurrent threads are supported." << endl;

                    // Re-declare the thread pointer to a definite size
                    l_threads = new pthread_t[l_numthreads];
                    //inputInfo = new struct procInfo[l_numthreads];

//                    // Start the threads
//                    for (l_index = 0; l_index < l_numthreads; l_index++)
//                    {
//                        cout << "main() : creating thread, " << l_index << endl;
//                        //rc = pthread_create(&l_threads[l_index], NULL, convFiles, (void *)&inputInfo[l_index]);
//                        //pthread_mutex_lock(&mutFilesFinished);
//                        rc = pthread_create(&l_threads[l_index], NULL, convFiles, (void *)l_index);
//                        //pthread_mutex_unlock(&mutFilesFinished);
//
////                        fileListIndex++;
//
//                        if (rc)
//                        {
//                            cout << "Error:unable to create thread," << rc << endl;
//                            exit(-1);
//                        }
//
//                        //fileListIndex++;
//                        //pthread_join(l_threads[l_index], NULL);
//                        pthread_detach(l_threads[l_index]);
//                    }

                    l_index = 0;
                    while((entry = readdir(dir)) != NULL)
                    {
                        if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0 )
                        {
                            l_file = entry->d_name;
                            found1 = l_file.find(".wav");
                            if (found1 != string::npos)
                            {
                                // Validate WAV file;
                                wavFile = fopen((l_path + '/' + l_file).c_str(), "rb");
                                fseek(wavFile, 8, SEEK_CUR);
                                fread(fileFormat, 1, 4, wavFile);

                                //fileFormat[4] = '\0';
                                //printf ("%s\n", l_file.c_str());
                                if(strcmp((const char *)fileFormat, "WAVE"))
                                {
                                    l_index++;
                                }
                                fclose(wavFile);
                            }
                        }
                    }

                    // Re-declare l_filelist variable
                    //filelist = new string[l_index + 1];
                    inputInfo = new struct procInfo[l_index];

                    l_index = 0;
                    closedir(dir);
                    dir = opendir(l_path.c_str());
                    while((entry = readdir(dir)) != NULL)
                    {
                        if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0 )
                        {
                            l_file = entry->d_name;
                            found1 = l_file.find(".wav");
                            if (found1 != string::npos)
                            {
                                //printf ("%s\n", l_file.c_str());
                                //filelist[l_index] = l_file;
                                l_file = l_path + '/' + l_file;
                                wavFile = fopen(l_file.c_str(), "rb");
                                fseek(wavFile, 8, SEEK_CUR);
                                fread(fileFormat, 1, 4, wavFile);

                                if(strcmp((const char *)fileFormat, "WAVE"))
                                {
                                    inputInfo[l_index].wavPath = l_path;
                                    inputInfo[l_index].wavFile = l_file;
                                    // Skip 10 bytes
                                    fseek(wavFile, 10, SEEK_CUR);
                                    fread(l_numChannels, 1, 2, wavFile);
                                    //l_numChannels[2] = '\0';
                                    //swap_endian16(&l_numChannels[0]);
                                    //swap_endian16(&l_numChannels[0]);
                                    inputInfo[l_index].numChannels = (int)l_numChannels[0];
                                    fread(l_sampleRate, 1, 4, wavFile);
                                    inputInfo[l_index].sampleRate = (int)l_sampleRate[0];
                                    inputInfo[l_index].sampleRate += (int)(l_sampleRate[1] << 8);
                                    inputInfo[l_index].sampleRate += (int)(l_sampleRate[2] << 16);
                                    inputInfo[l_index].sampleRate += (int)(l_sampleRate[3] << 24);
                                    inputInfo[l_index].mp3File = l_file.substr(0, l_file.length() - 3) + "mp3";
                                    inputInfo[l_index].status = 0;
                                    l_index++;
                                }
                                fclose(wavFile);
                            }
                        }
                    }

                    closedir(dir);

                    cout << "Found " << l_index << " WAV files" << endl;
                    numFiles = l_index;

//                    start = true;

                    // Start the threads
                    for (l_index = 0; l_index < l_numthreads; l_index++)
                    {
                        cout << "main() : creating thread, " << l_index << endl;
                        //rc = pthread_create(&l_threads[l_index], NULL, convFiles, (void *)&inputInfo[l_index]);
                        //pthread_mutex_lock(&mutFilesFinished);
                        rc = pthread_create(&l_threads[l_index], NULL, convFiles, (void *)l_index);
                        //pthread_mutex_unlock(&mutFilesFinished);

//                        fileListIndex++;

                        if (rc)
                        {
                            cout << "Error:unable to create thread," << rc << endl;
                            exit(-1);
                        }

                        //fileListIndex++;
                        //(void) pthread_join(l_threads[l_index], NULL);
                        pthread_detach(l_threads[l_index]);
                    }

                    start = true;

                    //while(inputInfo[numFiles].status != 2);
                    while(fileListIndex < numFiles); // cout << start << endl;
                }
            }
            else
            {
                cout << l_path << " is no directory" << endl;
            }

            l_valid = true;
		}
	}

	if(!l_valid)
	{
		cout << "Converts a WAV file or a group of WAV files in a folder to MP3 format using LAME encoding." << endl << endl;
		cout << "Usage:" << endl;
		cout << "Wav2MP3_MT [path-to-WAV-files]" << endl;
		cout << "Wav2MP3_MT [path-and-filename-of-WAV-file]" << endl << endl;
        cout << "Example:" << endl;
        cout << "  C:\\\\PathToWavFiles" << endl;
        cout << "  /usr/local/pathtowavfiles" << endl;
	}
	else
    {
        delete[] l_threads;  // When done, free memory pointed to by l_threads.
//        delete[] inputInfo;
        l_threads = NULL;
//        inputInfo = NULL;

        cout << endl << "Done!" << endl;
    }

	exit(0);
}

void *convFiles(void *threadid)
{
    int read, write;
    FILE *wavFile;
    FILE *mp3File;
    lame_t lameParam;
    int l_threadid = (intptr_t) threadid;
    int l_numChannels, l_sampleRate;

    short int *wavBufferL = NULL, *wavBufferR = NULL;//[WAV_SIZE * 1];
    unsigned char *mp3Buffer = NULL;//[MP3_SIZE * 1];

    while(fileListIndex < numFiles)
    {
        if(start)
        {
//                    pthread_mutex_lock(&mutFilesFinished);
            if(inputInfo[fileListIndex].status != 0 && fileListIndex < numFiles)
            {
                fileListIndex++;

//                if(fileListIndex >= numFiles)
//                {
//                    cout << "Terminating Thread ID " << l_threadid << endl;
//                    start = false;
//                    break;
//                }
            }
            else
            {
                if(fileListIndex < numFiles)
                {
                    pthread_mutex_lock(&mutFilesFinished);

                    wavBufferL = new short int[WAV_SIZE * inputInfo[fileListIndex].numChannels];
                    mp3Buffer = new unsigned char[MP3_SIZE];

                    cout << "fileListIndex: " << fileListIndex << "; Status: " << inputInfo[fileListIndex].status << endl;
                    cout << "Thread ID: " << l_threadid << endl;
                    cout << "Path: " << inputInfo[fileListIndex].wavPath << endl;
                    cout << "WAV File: " << inputInfo[fileListIndex].wavFile << endl;
                    cout << "MP3 File: " << inputInfo[fileListIndex].mp3File << endl;

                    inputInfo[fileListIndex].status = 1;

                    wavFile = fopen(inputInfo[fileListIndex].wavFile.c_str(), "rb");
                    mp3File = fopen(inputInfo[fileListIndex].mp3File.c_str(), "wb");  //output

                    l_numChannels = CHANNEL;
//                    l_sampleRate = 44100;
                    //l_numChannels = inputInfo[fileListIndex].numChannels;
                    l_sampleRate = inputInfo[fileListIndex].sampleRate;

//                    pthread_mutex_unlock(&mutFilesFinished);

                    fseek(wavFile, 44, SEEK_CUR);                                   //skip file header

                    // Set parameters
                    lameParam = lame_init();
                    lame_set_in_samplerate(lameParam, l_sampleRate);
                    lame_set_out_samplerate(lameParam, l_sampleRate);
                    lame_set_num_channels(lameParam, l_numChannels);
                    lame_set_VBR(lameParam, vbr_default);
                    if(l_numChannels == 2)
                    {
                        lame_set_mode(lameParam, STEREO);
                    }
                    else
                    {
                        lame_set_mode(lameParam, MONO);
                    }
    //                lame_set_mode(lameParam, MONO);
                    lame_set_quality(lameParam, 5);  // 0=best 5=good 9=worst
                    lame_init_params(lameParam);

                    do {
                        read = fread(wavBufferL, (l_numChannels * sizeof(short int)), WAV_SIZE, wavFile);

                        if (read == 0)
                            write = lame_encode_flush(lameParam, mp3Buffer, MP3_SIZE);
                        else
                        {
                            if(l_numChannels == 2)
                            {
                                write = lame_encode_buffer(lameParam, wavBufferL, wavBufferR, read, mp3Buffer, MP3_SIZE);
                            }
                            else
                            {
                                write = lame_encode_buffer(lameParam, wavBufferL, NULL, read, mp3Buffer, MP3_SIZE);
                            }
                        }
                        fwrite(mp3Buffer, write, 1, mp3File);
                    } while (read != 0);

                    lame_close(lameParam);
                    fclose(mp3File);
                    fclose(wavFile);

                    inputInfo[fileListIndex].status = 2;
                    //fileListIndex++;
                    //if(fileListIndex == numFiles) break;
                    pthread_mutex_unlock(&mutFilesFinished);
                }
                else
                {
                    //start = false;
                    cout << "Terminating Thread ID " << l_threadid << endl;
                    break;
                }
            }
//                    pthread_mutex_lock(&mutFilesFinished);


        }
        else
        {
            if(fileListIndex < numFiles)
            {
                cout << "Thread ID " << l_threadid << " waiting.." << endl;
            }
            else
            {
                cout << "Terminating Thread ID " << l_threadid << endl;
                //start = false;
                break;
            }
        }

        if(fileListIndex >= numFiles) break;
    }

    //l_inputInfo->done = true;
	pthread_exit(NULL);
	//int read, write;

	//FILE *wav = fopen("file.wav", "rb");
	//FILE *mp3 = fopen("file.mp3", "wb");

	//const int PCM_SIZE = 8192;
	//const int MP3_SIZE = 8192;

	//short int pcm_buffer[PCM_SIZE * 2];
	//unsigned char mp3_buffer[MP3_SIZE];

	//lame_t lame = lame_init();
	//lame_set_in_samplerate(lame, 44100);
	//lame_set_VBR(lame, vbr_default);
	//lame_init_params(lame);

	//do {
	//	read = fread(pcm_buffer, 2 * sizeof(short int), PCM_SIZE, pcm);
	//	if (read == 0)
	//		write = lame_encode_flush(lame, mp3_buffer, MP3_SIZE);
	//	else
	//		write = lame_encode_buffer_interleaved(lame, pcm_buffer, read, mp3_buffer, MP3_SIZE);
	//	fwrite(mp3_buffer, write, 1, mp3);
	//} while (read != 0);

	//lame_close(lame);
	//fclose(mp3);
	//fclose(pcm);

	return NULL;
}

int getNumberOfCores()
{
#ifdef _WIN32
	SYSTEM_INFO sysinfo;
	GetSystemInfo(&sysinfo);
	return sysinfo.dwNumberOfProcessors;
#elif MACOS
	int nm[2];
	size_t len = 4;
	uint32_t count;

	nm[0] = CTL_HW; nm[1] = HW_AVAILCPU;
	sysctl(nm, 2, &count, &len, NULL, 0);

	if (count < 1) {
		nm[1] = HW_NCPU;
		sysctl(nm, 2, &count, &len, NULL, 0);
		if (count < 1) { count = 1; }
	}
	return count;
#else
	return sysconf(_SC_NPROCESSORS_ONLN);
#endif
}

void swap_endian16(unsigned char val[2]) {
    *val = (val[0]<<8) | (val[1]>>8);
}

void swap_endian32(unsigned char val[4]) {
    unsigned char temp[4];
//    unsigned char temp2[2];

    temp[0] = (val[3]);// << 24) & 0xff000000;
    temp[1] = (val[2]);// << 8) & 0x00ff0000;
    temp[2] = (val[1]);// >> 8) & 0x0000ff00;
    temp[3] = (val[0]);// >> 24) & 0x000000ff;

    val[0] = (temp[0]);// << 24) & 0xff000000;
    val[1] = (temp[1]);// << 8) & 0x00ff0000;
    val[2] = (temp[2]);// >> 8) & 0x0000ff00;
    val[3] = (temp[3]);// >> 24) & 0x000000ff;
    // *val = (val[0]<<24) | ((val[1]<<8) & 0x00ff0000) |
    //  ((val[2]>>8) & 0x0000ff00) | (val[3]>>24);
}
//unsigned hardware_concurrency()
//{
//	SYSTEM_INFO info = { { 0 } };
//	GetSystemInfo(&info);
//	return info.dwNumberOfProcessors;
//}
