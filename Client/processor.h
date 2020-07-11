#pragma once

#ifndef PROCESSOR_H_
#define PROCESSOR_H_

#include <iostream>
#include <vector>


using namespace std;

#include "defs.h"
#include "network.h"
#include "fileUtils.h"

bool isLoggedIn = false;

extern MESSAGE gSendMessage;
extern MESSAGE gRecvMessage;

char cookie[COOKIE_LEN];

/*
- Function: packMessage
- Description: Pack the message based on provided parameters
- [IN] opcode: int opcode value
- [IN] length: int length value
- [IN] offset: int offset value
- [IN] burst: int burst value
- [IN] payload: char array
*/
void packMessage(int opcode, int length, int offset, int burst, char* payload) {
	gSendMessage.opcode = opcode;
	gSendMessage.length = length;
	gSendMessage.offset = offset;
	gSendMessage.burst = burst;
	if (payload != NULL) {
		strcpy(gSendMessage.payload, payload);
	}
}

/*
- Function: processOpLogIn
- Description: Send log in request to server and update log in status
- Return: status code received from server
- [IN] username: int opcode value
- [IN] password: int length value
*/
int processOpLogIn(char* username, char* password) {
	// Construct request
	snprintf(gSendMessage.payload, BUFF_SIZE, "%s %s", username, password);
	packMessage(OPA_LOGIN, strlen(gSendMessage.payload), 0, 0, NULL);

	// Send and Recv
	handleSent();
	handleRecv();

	if (gRecvMessage.opcode == OPS_OK) {
		isLoggedIn = true;
	}

	Sleep(1000);

	return gRecvMessage.opcode;
}

/*
- Function: processOpLogOut
- Description: Send log out request to server and update log in status
- Return: status code received from server
*/
int processOpLogOut() {
	// Construct request
	packMessage(OPA_LOGOUT, 0, 0, 0, "");

	// Send and recv
	handleSent();
	handleRecv();

	if (gRecvMessage.opcode == OPS_OK) {
		isLoggedIn = false;
	}
	return gRecvMessage.opcode;
}

/*
- Function: processOpReauth
- Description: Check if there's a cookie file. If yes, send 
eauth request to server and update log in status.
- Return: -1 if cannot find valid cookie, else return
status code received from server
*/
int processOpReauth() {
	// Read SID from file
	readCookieFromFile(cookie, COOKIE_LEN);
	if (strlen(cookie) != COOKIE_LEN - 1) {
		return -1;
	}
	else {
		// Construct reauth message
		packMessage(OPA_REAUTH, COOKIE_LEN, 0, 0, cookie);
		handleSent();
		handleRecv();

		if (gRecvMessage.opcode == OPS_OK) {
			isLoggedIn = true;
		}

		return gRecvMessage.opcode;
	}
}

/*
- Function: processOpReqCookie
- Description: Send cookie request to server and write cookie to file
- Return: status code received from server
*/
int processOpReqCookie() {
	// Construct reauth message
	packMessage(OPA_REQ_COOKIES, 0, 0, 0, "");
	handleSent();
	handleRecv();

	if (gRecvMessage.opcode == OPS_OK) {
		strcpy_s(cookie, gRecvMessage.payload);
		writeCookietoFile(cookie);
	}

	return gRecvMessage.opcode;
}


/*****************************
Group
*****************************/

/*
- Function: processOpGroup
- Description: Send request to server
- Return: status code received from server
- [IN] opCode: int opcode value
- [IN] groupName: char array storing group name
*/
int processOpGroup(int opCode, char* groupName) {
	// Construct reauth message
	packMessage(opCode, strlen(groupName), 0, 0, groupName);
	handleSent();
	handleRecv();


	return gRecvMessage.opcode;
}

/*
- Function: processOpGroupList
- Description: Send group list request to server and populate
vector with group names
- Return: 0 if succeed, 1 if there's an interruption in the process
- [OUT] groupList: vector to store the group names
*/
int processOpGroupList(vector<char*> &groupList) {

	// Construct reauth message
	packMessage(OPG_GROUP_LIST, 0, 0, 0, "");
	handleSent();
	handleRecv();

	int groupCount = 0;
	if (gRecvMessage.opcode != OPG_GROUP_COUNT) {
		return gRecvMessage.opcode;
	}

	groupCount = atoi(gRecvMessage.payload);

	char* groupName;
	
	for (int i = 0; i < groupCount; i++) {
		// Send OPS_CONTINUE to server to request next group name
		packMessage(OPS_CONTINUE, 0, 0, 0, "");
		handleSent();
		handleRecv();

		if (gRecvMessage.opcode == OPG_GROUP_NAME) {
			groupName = (char*)malloc(GROUPNAME_SIZE);
			strcpy_s(groupName, GROUPNAME_SIZE, gRecvMessage.payload);
			groupList.push_back(groupName);
		}
		else {
			return 1;
		}
	}

	return 0;
}

/*****************************
Browsing
*****************************/

/*
- Function: processOpBrowse
- Description: Send request to server
- Return: status code received from server
- [IN] opCode: int opcode value
- [IN] groupName: char array storing group name
*/
int processOpBrowse(int opCode, char* path) {
	// Construct reauth message
	packMessage(opCode, strlen(path), 0, 0, path);
	handleSent();
	handleRecv();

	return gRecvMessage.opcode;
}

/*
- Function: processOpFileList
- Description: Send file/dir list request to server and
populate lists with file and dir names.
- [OUT] fileList: vector to store file names
- [OUT] dirList: vector to store directory names
*/
int processOpFileList(vector<char*> &fileList, vector<char*> &dirList) {
	// Construct reauth message
	packMessage(OPB_LIST, 0, 0, 0, "");
	handleSent();
	handleRecv();

	int fileCount = 0, dirCount = 0;

	if (gRecvMessage.opcode != OPB_FILE_COUNT) {
		return gRecvMessage.opcode;
	}

	fileCount = atoi(gRecvMessage.payload);

	packMessage(OPS_CONTINUE, 0, 0, 0, "");
	handleSent();
	handleRecv();

	// Receive dir count

	if (gRecvMessage.opcode != OPB_DIR_COUNT) {
		return gRecvMessage.opcode;
	}

	dirCount = atoi(gRecvMessage.payload);

	char* itemName;

	for (int i = 0; i < fileCount; i++) {

		packMessage(OPS_CONTINUE, 0, 0, 0, "");
		handleSent();
		handleRecv();

		if (gRecvMessage.opcode == OPB_FILE_NAME) {
			itemName = (char*)malloc(FILENAME_SIZE);
			strcpy_s(itemName, FILENAME_SIZE, gRecvMessage.payload);
			fileList.push_back(itemName);
		}
		else {
			return 1;
		}
	}

	for (int i = 0; i < dirCount; i++) {

		packMessage(OPS_CONTINUE, 0, 0, 0, "");
		handleSent();
		handleRecv();

		if (gRecvMessage.opcode == OPB_DIR_NAME) {
			itemName = (char*)malloc(FILENAME_SIZE);
			strcpy_s(itemName, FILENAME_SIZE, gRecvMessage.payload);
			dirList.push_back(itemName);
		}
		else {
			return 1;
		}
	}

	return 0;
}

#endif