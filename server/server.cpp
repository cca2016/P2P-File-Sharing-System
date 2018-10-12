#include <iostream>
#include "P2PServer.cpp"
using namespace std;

int main(int argc, const char* argv[])
{
	cout << "P2P Server is about to start:" << endl;

	// Start up the server
	P2PServer server;
	server.start();

	return 0;
}
