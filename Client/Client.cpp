// TCPClient.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "ui.h"

int main(int argc, char** argv)
{
	// Validate parameters
	if (argc != 3) {
		printf("Wrong arguments! Please enter in format: \"%s [ServerIpAddress] [ServerPortNumber]\"", argv[0]);
		return 1;
	}

	// Initialize network
	int iRetVal = initializeNetwork(argc, argv);
	if (iRetVal == 1)
		return 1;

	// Open data file
	if (openFile("accData"))
		return 1;

	// Start program
	runUI();

	return 0;
}