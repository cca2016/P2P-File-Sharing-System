#include <iostream>
#include "P2PClient.cpp"
using namespace std;

int main(int argc, const char* argv[])
{
	// Program welcome message
	cout << "P2P Client - startup requested" << endl;

	// Get any custom arguments
	string address;
	int port;
	if (argc == 3)
	{
		address = argv[1];
		port = atoi(argv[2]);
	}
	else
	{
		address = "localhost";
		port = 27890;
	}

	// Start up the client server
	P2PClient client;
	client.start(address, port);

	return 0;
}
