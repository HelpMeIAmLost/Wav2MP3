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
 *     - MinGW-get
 *
 *     Open a MinGW shell and install the following:
 *       mingw-get install gcc
 *       mingw-get install mingw-utils
 *   LAME encoder source files (https://sourceforge.net/projects/lame/files/lame/)
 *     Go to extract folder and run the following commands:
 *       ./configure --prefix=/mingw --disable-shared --enable-nasm --enable-static
 *       make
 *       make install
 *
 *       Note: If there is a langinfo.h error, go to http://blog.k-tai-douga.com/article/35965219.html
 *         patch -p1 < lame-3.100-parse_c.diff
 *   pthreads
 *     mingw-get install pthreads
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

void *convFiles(void *);
int getNumberOfCores(void);

struct procInfo
{
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

int main(int argc, char *argv[])
{
    pthread_t *l_threads = NULL;
    FILE *wavFile;
    unsigned char fileFormat[4];
    unsigned char l_numChannels[2];
    unsigned char l_sampleRate[4];
    unsigned int l_numthreads;
    int rc;
    intptr_t l_index;
    string l_file;
    string l_path;
    bool l_valid = false;
    struct stat l_pathinfo;
    DIR *dir;
    struct dirent *entry;
    size_t found1, found2;

    cout << "Multi-threaded WAV to MP3 Converter v1.0.0" << endl;
    cout << "by Rowen Chumacera" << endl << endl;
    cout << "Using LAME version " << get_lame_version() << endl << endl;

    // For debugging
//    argc = 2;
//    argv[1] = "C:\\git\\Wav2MP3\\test";

    // Check for arguments
    if (argc == 2)
    {
        l_path = argv[1];

        // Validate command-line argument if it contains for \\ or /
        found1 = l_path.rfind('\\');
        found2 = l_path.rfind('/');
        if (found1 != string::npos || found2 != string::npos)
        {
            // Replace \\ for compatibility
            replace(l_path.begin(), l_path.end(), '\\', '/');
            // Try to access user-defined path
            if (stat(l_path.c_str(), &l_pathinfo) != 0)
            {
                cout << "cannot access " << l_path << endl;
                exit(-1);
            }
            else if (l_pathinfo.st_mode & S_IFDIR)
            {
                dir = opendir(l_path.c_str());
                if (dir != NULL)
                {
                    // Check possible number of concurrent threads
                    l_numthreads = getNumberOfCores();
                    // Check if the number of supported concurrent threads is not well-defined or cannot be computed
                    if (l_numthreads == 0)
                    {
                        l_numthreads = 1;
                    }
                    // Re-declare the thread pointer to a definite size
                    l_threads = new pthread_t[l_numthreads];
                    // Report
                    cout << l_numthreads << " concurrent threads are supported." << endl;

                    // Check WAV files in user-defined path
                    l_index = 0;
                    cout << "Checking " << l_path << " for WAV files.." << endl;
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

                                if(strcmp((const char *)fileFormat, "WAVE"))
                                {
                                    l_index++;
                                }
                                fclose(wavFile);
                            }
                        }
                    }
                    closedir(dir);
                    // Re-declare input information structure
                    inputInfo = new struct procInfo[l_index];
                    // Fill input information
                    l_index = 0;
                    dir = opendir(l_path.c_str());
                    while((entry = readdir(dir)) != NULL)
                    {
                        if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0 )
                        {
                            l_file = entry->d_name;
                            found1 = l_file.find(".wav");
                            if (found1 != string::npos)
                            {
                                l_file = l_path + '/' + l_file;
                                wavFile = fopen(l_file.c_str(), "rb");
                                // Skip 8 bytes to file format data in header
                                fseek(wavFile, 8, SEEK_CUR);
                                fread(fileFormat, 1, 4, wavFile);

                                if(strcmp((const char *)fileFormat, "WAVE"))
                                {
                                    inputInfo[l_index].wavPath = l_path;
                                    inputInfo[l_index].wavFile = l_file;
                                    // Mono
                                    inputInfo[l_index].numChannels = 1;
                                    // Fixed for now
                                    inputInfo[l_index].sampleRate = 44100;
                                    // Save output filename
                                    inputInfo[l_index].mp3File = l_file.substr(0, l_file.length() - 3) + "mp3";
                                    // Initial status
                                    inputInfo[l_index].status = 0;
                                    l_index++;
                                }
                                fclose(wavFile);
                            }
                        }
                    }
                    closedir(dir);
                    numFiles = l_index;
                    // Report
                    cout << "Found " << l_index << " WAV files" << endl;

                    // Start the threads, using all possible concurrent threads
                    for (l_index = 0; l_index < l_numthreads; l_index++)
                    {
                        cout << "Creating Thread " << l_index << endl;
                        rc = pthread_create(&l_threads[l_index], NULL, convFiles, (void *)l_index);

                        if (rc)
                        {
                            cout << "Error:unable to create thread," << rc << endl;
                            exit(-1);
                        }
                        // Make it independent
                        pthread_detach(l_threads[l_index]);
                    }

                    // Start encoding
                    start = true;

                    // Wait for threads to finish
                    while(fileListIndex < numFiles);
                }
            }
            else
            {
                cout << l_path << " is no directory" << endl;
            }
            // User-define path was valid
            l_valid = true;
        }
    }

    if(!l_valid)
    {
        // No valid path was provided
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
        // Clean up
        delete[] l_threads;  // When done, free memory pointed to by l_threads.
        //delete[] inputInfo;
        l_threads = NULL;
        //inputInfo = NULL;

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
    short int *wavBufferL = NULL, *wavBufferR = NULL;
    unsigned char *mp3Buffer = NULL;

    // Stop when all WAV files in the folder have been processed
    while(fileListIndex < numFiles)
    {
        // Check for start flag
        if(start)
        {
            // Check for the status of currently indexed file
            if(inputInfo[fileListIndex].status != 0 && fileListIndex < numFiles)
            {
                fileListIndex++;
            }
            else
            {
                if(fileListIndex < numFiles)
                {
                    pthread_mutex_lock(&mutFilesFinished);
                    cout << "Thread ID: " << l_threadid << endl;
                    cout << "Path: " << inputInfo[fileListIndex].wavPath << endl;
                    cout << "WAV File: " << inputInfo[fileListIndex].wavFile << endl;
                    cout << "MP3 File: " << inputInfo[fileListIndex].mp3File << endl;

                    inputInfo[fileListIndex].status = 1;

                    wavFile = fopen(inputInfo[fileListIndex].wavFile.c_str(), "rb");
                    mp3File = fopen(inputInfo[fileListIndex].mp3File.c_str(), "wb");

                    l_numChannels = inputInfo[fileListIndex].numChannels;
                    l_sampleRate = inputInfo[fileListIndex].sampleRate;

                    // Re-declare buffers
                    wavBufferL = new short int[WAV_SIZE * l_numChannels];
                    mp3Buffer = new unsigned char[MP3_SIZE];
                    // Point to start of WAV sound data
                    fseek(wavFile, 44, SEEK_CUR);

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
                    lame_set_quality(lameParam, 5);
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
                    pthread_mutex_unlock(&mutFilesFinished);
                }
                else
                {
                    cout << "Terminating Thread ID " << l_threadid << endl;
                    break;
                }
            }
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
                break;
            }
        }

        if(fileListIndex >= numFiles) break;
    }

    pthread_exit(NULL);
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
