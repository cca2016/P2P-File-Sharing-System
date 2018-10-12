/**
 * Peer-to-peer server class
 */

#include "P2PCommon.hpp"

P2PCommon::P2PCommon() {}

vector<string> P2PCommon::parseRequest(string request)
{
	return P2PCommon::splitString(request, '\n');
}

vector<string> P2PCommon::parseAddress(string request)
{
	return P2PCommon::splitString(request, ':');
}

vector<string> P2PCommon::splitString(string request, char delimeter)
{
	// Remove spaces from the request
	stringstream ss(request);
	vector<string> request_parsed;

	if (request.length() > 0)
	{
		string line;
		while (getline(ss, line, delimeter))
		{
			request_parsed.push_back(line);
		}
	}

	return request_parsed;
}

string P2PCommon::trimWhitespace(string line)
{
	// Trim trailing whitespace
	line.erase(line.find_last_not_of(" \n\r\t")+1);

	// Trim leading whitespace
	if (line.length() > 0)
	{
		int first_non_whitespace = line.find_first_not_of(" \n\r\t");
		if (first_non_whitespace > 0)
		{
			line = line.substr(first_non_whitespace, line.length() - first_non_whitespace);
		}
	}

	return line;
}

string P2PCommon::renameDuplicateFile(string filename, string directory)
{
	// Break the filename into pieces
	vector<string> filename_info = P2PCommon::splitString(filename, '.');
	string filename_orig_ext = filename_info.back();

	// If the directory string is empty, assume the current directory
	if (directory.length() == 0)
	{
		directory = "./";
	}

	// Copy the original filename
	string new_filename = filename;

	// If we need to, we'll attach an additional extension
	unsigned int copy_count = 1;

	// If this file already exists, then make a new name
	struct stat s;
	int file_status = stat((directory + "/" + filename).c_str(), &s);
	while (file_status == 0 && (s.st_mode & S_IFREG))
	{
		// File found - rename the file
		// Determine the length of the additional extension
		string filename_addtl_ext = "(" + to_string(copy_count++) + ").";

		// If the filename is greater than 255 characters, trim it accordingly
		if (filename.length() >= P2PCommon::MAX_FILENAME_LENGTH - filename_addtl_ext.length())
		{
			new_filename = filename.substr(0, P2PCommon::MAX_FILENAME_LENGTH - filename_addtl_ext.length() - filename_orig_ext.length());
		}
		else
		{
			new_filename = filename.substr(0, filename.length() - filename_orig_ext.length());
		}

		// Reattach the extensions
		new_filename += filename_addtl_ext + filename_orig_ext;

		// Update the file status
		file_status = stat((directory + "/" + new_filename).c_str(), &s);
	}

	// Return new filename
	return new_filename;
}

void P2PCommon::clearScreen()
{
	cout << string(5, '\n');
}
