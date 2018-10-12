#ifndef P2PCOMMON_H
#define P2PCOMMON_H

// Standard Library
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <vector>
#include <map>
#include <algorithm>
#include <fstream>
#include <sstream>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>

// Directory Management
#include <dirent.h>

// Network Includes
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

// Multithreading
#include <pthread.h>

using namespace std;

typedef struct {
	unsigned int socket_id;
	string type;
	string name;
} P2PSocket;

typedef struct {
	unsigned int socket_id;
	string message;
} P2PMessage;

typedef struct {
	unsigned int socket_id;
	string public_address;
	unsigned int public_port;
	string remote_path;
} FileAddress;

typedef struct {
	unsigned int file_id;
	unsigned int size;
	vector<FileAddress> addresses;
	string name;
	string path;
	string hash;
	bool completed;
	vector<unsigned int> missing_pieces;
} FileItem;

typedef struct {
	FileItem file_item;
	int socket_id;
	unsigned int start;
	unsigned int count;
} FileDataRequest;

typedef struct {
	FileItem file_item;
	char * packet;
} FileDataPacket;

class P2PCommon
{
	public:
		P2PCommon();
		static vector<string> parseRequest(string);
		static vector<string> parseAddress(string);
		static vector<string> splitString(string, char);
		static string trimWhitespace(string);
		static string renameDuplicateFile(string, string);
		static void clearScreen();

		// Some variables used throughout the program
		static const unsigned int MAX_FILENAME_LENGTH = 255;
};

#endif