# Put the filename of the CPP file here
TARGET=Wav2MP3
CC=g++

all: $(TARGET).exe

$(TARGET).exe: $(TARGET).o
	#$(CC) -I/c/MinGW/include/ -lpthread -lmp3lame -o $(TARGET).exe $(TARGET).o 
	$(CC) -I/c/MinGW/include/ -o $(TARGET).exe $(TARGET).o -lpthread -lmp3lame 
	rm -f $(TARGET).o

$(TARGET).o: ./src/$(TARGET).cpp
	$(CC) -c ./src/$(TARGET).cpp
     
clean:
	rm -f $(TARGET).o $(TARGET).exe
	echo Done cleaning!