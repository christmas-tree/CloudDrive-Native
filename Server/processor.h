#pragma once

#ifndef _PROCESSOR_H
#define _PROCESSOR_H

#include <list>
#include <unordered_map>
#include <winsock2.h>
#include "dbUtils.h"

std::list<Attempt> attemptList;
std::list<Account> accountList;
std::list<Group> groupList;

std::unordered_map<SOCKET, Account*> socketAccountMap;

CRITICAL_SECTION attemptCritSec;

// Function: initializeData
// Description: Call functions to open database, read accounts and groups
//              information from database and initialize critical section
// Return: 0 if succeed, else return 1
int initializeData() {
	if (openDb()) return 1;
	if (readAccountDb(accountList)) return 1;
	if (readGroupDb(groupList)) return 1;

	InitializeCriticalSection(&attemptCritSec);
	return 0;
}

// Function: isValidName
// Description: Validate the provided string to see if it is a valid folder/file name
// Return: 1 if the string is valid, else return 0
// -IN:  s: a char array that needs checking
bool isValidName(char* s) {
	char prohibitedChars[] = { '<', '>', ':', '\"', '/', '\\', '|', '?', '*' };
	bool allSpaces = true;
	for (int i = 0; s[i] != 0; i++) {
		for (char c : prohibitedChars) {
			if (s[i] == c) return false;
		}
		if (s[i] != ' ') allSpaces = false;
	}
	if (allSpaces) return false;
	return true;
}

// Function: packMessage
// Description: Pack the message based on provided parameters
// -IN:  message: the message to be packed
//       opcode: int opcode value
//       length: int length value
//       offset: int offset value
//       burst: int burst value
//       payload: char array
void packMessage(LPMESSAGE message, int opcode, int length, int offset, int burst, char* payload) {
	message->opcode = opcode;
	message->length = length;
	message->offset = offset;
	message->burst = burst;
	strcpy(message->payload, payload);
}

/*
Generate a new unique cookie.
[OUT] cookie:		a char array to store newly created cookie
*/
void generateCookies(char* cookie) {
	static char* characters = "1234567890ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefjhijklmnopqrstuvwxyz";
	srand((unsigned int)time(NULL));

	bool duplicate = false;
	// Generate new cookie until it's unique
	do {
		// Randomly choose a character
		for (int i = 0; i < COOKIE_LEN - 1; i++) {
			cookie[i] = characters[rand() % 62];
		}
		cookie[COOKIE_LEN - 1] = 0;

		// Check if duplicate
		for (auto it = accountList.begin(); it != accountList.end(); it++) {
			if (strcmp(it->cookie, cookie) == 0) {
				duplicate = true;
				break;
			}
		}
	} while (duplicate);
}

/*
Process login and produce response
[IN/OUT] bufferObj:		buffer object to read from and write to
*/
int processOpLogIn(BUFFER_OBJ* bufferObj) {
	LPMESSAGE message = &(bufferObj->sock->mess);
	Account* account = NULL;
	time_t now = time(0);

	// Parse username and password
	char* username = NULL;
	char* password = NULL;

	username = message->payload;
	for (unsigned int i = 0; i < message->length; i++) {
		if (message->payload[i] == ' ') {
			message->payload[i] = 0;
			password = message->payload + i + 1;
		}
	}

	if (password == NULL) {
		packMessage(message, OPS_ERR_BADREQUEST, 0, 0, 0, "");
		return 1;
	}

	// Find account in account list
	for (auto it = accountList.begin(); it != accountList.end(); it++) {
		if (strcmp(it->username, message->payload) == 0) {
			account = &(*it);
			break;
		}
	}

	// If cannot find account, inform not found error
	if (account == NULL) {
		packMessage(message, OPS_ERR_NOTFOUND, 0, 0, 0, "");
		return 1;
	}

	WaitForSingleObject(account->mutex, INFINITE);

	// Check if account is currently active on another device
	for (auto accountIt = socketAccountMap.begin(); accountIt != socketAccountMap.end(); accountIt++) {
		if (accountIt->second->uid == account->uid) {
			packMessage(message, OPS_ERR_ANOTHERCLIENT, 0, 0, 0, "");
			ReleaseMutex(account->mutex);
			return 1;
		}
	}

	// Check if account is locked
	if (account->isLocked) {
		packMessage(message, OPS_ERR_LOCKED, 0, 0, 0, "");
		ReleaseMutex(account->mutex);
		return 1;
	}

	// Check if has attempt before
	Attempt* attempt = NULL;
	auto it = attemptList.begin();
	for ( ; it != attemptList.end(); it++) {
		if (it->account == account) {
			attempt = &((Attempt)*it);
			break;
		}
	}

	// Check password
	if (strcmp(account->password, password) != 0) {
		printf("Wrong password!\n");

		EnterCriticalSection(&attemptCritSec);
		
		if (attempt != NULL) {
			// If last attempt is more than 1 hour before then reset number of attempts
			if (now - attempt->lastAtempt > TIME_1_HOUR)
				attempt->numOfAttempts = 1;
			else
				attempt->numOfAttempts++;
			attempt->lastAtempt = now;

			// If number of attempts exceeds limit then block account and update database 
			if (attempt->numOfAttempts > ATTEMPT_LIMIT) {
				account->isLocked = true;
				lockAccountDb(account);
				printf("Account locked. Database updated.\n");

				packMessage(message, OPS_ERR_LOCKED, 0, 0, 0, "");
				LeaveCriticalSection(&attemptCritSec);
				ReleaseMutex(account->mutex);
				return 1;
			}
		}
		// if there is no previous attempt, add attempt
		else {
			Attempt newAttempt;
			newAttempt.lastAtempt = now;
			newAttempt.numOfAttempts = 1;
			newAttempt.account = account;
			attemptList.push_back(newAttempt);
		}
		packMessage(message, OPS_ERR_WRONGPASS, 0, 0, 0, "");
		LeaveCriticalSection(&attemptCritSec);
		ReleaseMutex(account->mutex);
		return 1;
	}

	// Passed all checks. Update active time and session account info
	account->lastActive = now;
	socketAccountMap[bufferObj->sock->s] = account;

	if (attempt != NULL)
		attemptList.erase(it);

	printf("Login successful.\n");
	packMessage(message, OPS_OK, 0, 0, 0, "");

	LeaveCriticalSection(&attemptCritSec);
	ReleaseMutex(account->mutex);

	return 1;
}

/*
Process log out and construct response
[OUT] buff:		a char array to store the constructed response message
[IN] cookie:		a char array containing the SID to be processed
*/
int processOpLogOut(BUFFER_OBJ* bufferObj) {
	LPMESSAGE message = &(bufferObj->sock->mess);
	Account* account = NULL;
	time_t now = time(0);

	// Find account. If cannot find account, deny log out
	auto accountSearch = socketAccountMap.find(bufferObj->sock->s);
	if (accountSearch == socketAccountMap.end()) {
		packMessage(message, OPS_ERR_NOTLOGGEDIN, 0, 0, 0, "");
		return 1;
	}

	account = accountSearch->second;

	// All checks out! Allow log out
	printf("Log out OK.\n");
	account->lastActive = now;
	account->workingGroup = NULL;
	account->cookie[0] = 0;
	socketAccountMap.erase(accountSearch);

	packMessage(message, OPS_OK, 0, 0, 0, "");
	printf("Log out successful.\n");
	return 1;
}

/*
Process reauthentication request and construct response
[IN] sock:		the socket that's sending the request
[OUT] buff:		a char array to store the constructed response message
[IN] cookie:		a char array containing the SID to be processed
*/
int processOpReauth(BUFFER_OBJ* bufferObj) {
	LPMESSAGE message = &(bufferObj->sock->mess);
	time_t now = time(0);
	Account* account = NULL;

	// Check if cookie's length is correct
	if (message->length != COOKIE_LEN) {
		packMessage(message, OPS_ERR_BADREQUEST, 0, 0, 0, "");
		return 1;
	}

	// Find account with cookie
	for (auto it = accountList.begin(); it != accountList.end(); it++)
		if (strcmp(message->payload, it->cookie) == 0) {
			account = (Account*)&(*it);
			break;
		}

	// If account does not exists
	if (account == NULL) {
		printf("Cookie not found.\n");
		packMessage(message, OPS_ERR_NOTFOUND, 0, 0, 0, "");
		return 1;
	}

	// Check if lastActive is more than 1 day ago
	if (now - account->lastActive > TIME_1_DAY) {
		printf("Login session timeout. Deny reauth.\n");
		packMessage(message, OPS_ERR_NOTLOGGEDIN, 0, 0, 0, "");
		return 1;
	}

	// Check if account is disabled
	if (account->isLocked) {
		printf("Account is locked. Reauth failed.\n");
		account->cookie[0] = 0;
		packMessage(message, OPS_ERR_LOCKED, 0, 0, 0, "");
		return 1;
	}

	// Check if account is logged in on another device
	for (auto accountIt = socketAccountMap.begin(); accountIt != socketAccountMap.end(); accountIt++) {
		if (accountIt->second->uid == account->uid) {
			packMessage(message, OPS_ERR_ANOTHERCLIENT, 0, 0, 0, "");
			ReleaseMutex(account->mutex);
			return 1;
		}
	}

	// All checks out!
	printf("Allow reauth.\n");
	socketAccountMap[bufferObj->sock->s] = account;
	account->lastActive = time(0);
	packMessage(message, OPS_OK, 0, 0, 0, "");
	return 1;
}

/*
Process cookie request and construct response
[IN] sock:		the socket that's sending the request
[OUT] buff:		a char array to store the constructed response message
[OUT] responseLen: a short to store the length of the response payload
*/
int processOpRequestCookie(BUFFER_OBJ* bufferObj) {
	LPMESSAGE message = &(bufferObj->sock->mess);

	// Find account
	auto accountSearch = socketAccountMap.find(bufferObj->sock->s);
	if (accountSearch == socketAccountMap.end()) {
		packMessage(message, OPS_ERR_FORBIDDEN, 0, 0, 0, "");
		return 1;
	}

	Account* account = accountSearch->second;
	char cookie[COOKIE_LEN];
	generateCookies(cookie);

	// Create cookie and add to account
	account->lastActive = time(0);
	strcpy_s(account->cookie, COOKIE_LEN, cookie);

	// Construct response
	packMessage(message, OPS_OK, 0, 0, 0, cookie);
	printf("Generated new Cookie for socket %d: %s.\n", bufferObj->sock->s, cookie);
	return 1;
}

/*
Remove socket information from any Session previously associated with it
[IN] sock:	the socket which has been disconnected
*/
void disconnect(SOCKET sock) {
	socketAccountMap.erase(sock);
}

int processOpGroup(BUFFER_OBJ* bufferObj) {

	LPMESSAGE message = &(bufferObj->sock->mess);
	Group* group = NULL;
	int ret;

	// Check if this socket is associated with an account
	auto accountSearch = socketAccountMap.find(bufferObj->sock->s);
	if (accountSearch == socketAccountMap.end()) {
		packMessage(message, OPS_ERR_FORBIDDEN, 0, 0, 0, "");
		return 1;
	}
	Account* account = accountSearch->second;
	std::list<Group> tempGroupList;
	auto it = tempGroupList.begin();

	switch (message->opcode) {
	case OPG_GROUP_LIST:

		if (queryGroupForAccount(account, tempGroupList)) {
			packMessage(message, OPS_ERR_SERVERFAIL, 0, 0, 0, "");
			return 1;
		}

		char count[10];
		MESSAGE newMessage;
		LPMESSAGE_LIST listPtr;

		// Send group count
		_itoa(tempGroupList.size(), count, 10);
		packMessage(message, OPG_GROUP_COUNT, 0, 0, 0, count);

		// Add directory count to wait queue
		if (!tempGroupList.empty()) {
			packMessage(&newMessage, OPG_GROUP_NAME, strlen(tempGroupList.front().groupName), 0, 0, tempGroupList.front().groupName);
			account->queuedMess = (LPMESSAGE_LIST)malloc(sizeof(MESSAGE_LIST));
			listPtr = account->queuedMess;
			listPtr->mess = newMessage;

			it = tempGroupList.begin();
			it++;
			// Add file name to wait queue
			for ( ; it != tempGroupList.end(); it++) {
				packMessage(&newMessage, OPG_GROUP_NAME, strlen((*it).groupName), 0, 0, (*it).groupName);
				listPtr->next = (LPMESSAGE_LIST)malloc(sizeof(MESSAGE_LIST));
				listPtr = listPtr->next;
				listPtr->mess = newMessage;
			}
		}
		return 1;

	case OPG_GROUP_USE:
		// Check if account has access to requested group
		ret = accountHasAccessToGroupDb(account, message->payload);
		if (ret == -1) {
			packMessage(message, OPS_ERR_SERVERFAIL, 0, 0, 0, "");
			return 1;
		}
		
		if (ret == 1) {
			// Find group in group list and attach to account
			for (auto it = groupList.begin(); it != groupList.end(); ++it) {
				if (strcmp(it->groupName, message->payload) == 0) {
					account->workingGroup = &(*it);
					account->workingDir[0] = 0;
					break;
				}
			}
			if (account->workingGroup == NULL) {
				packMessage(message, OPS_ERR_NOTFOUND, 0, 0, 0, "");
				return 1;
			}
			packMessage(message, OPS_OK, 0, 0, 0, "");
			return 1;
		}

		// Account doesn't have access to requested group
		packMessage(message, OPS_ERR_FORBIDDEN, 0, 0, 0, "");
		return 1;

	case OPG_GROUP_JOIN:
		// Check if the account already has access to this group
		ret = accountHasAccessToGroupDb(account, message->payload);
		if (ret == -1) {
			packMessage(message, OPS_ERR_SERVERFAIL, 0, 0, 0, "");
			return 1;
		}
		
		if (ret == 1) {
			packMessage(message, OPS_ERR_ALREADYINGROUP, 0, 0, 0, "");
			return 1;
		}

		// If not, check if the group exists
		for (auto it = groupList.begin(); it != groupList.end(); ++it) {
			if (strcmp(it->groupName, message->payload) == 0) {
				group = &(*it);
				break;
			}
		}

		if (group == NULL) {
			packMessage(message, OPS_ERR_NOTFOUND, 0, 0, 0, "");
			return 1;
		}

		// Add to database
		if (addUserToGroupDb(account, group)) {
			packMessage(message, OPS_ERR_SERVERFAIL, 0, 0, 0, "");
			return 1;
		}

		packMessage(message, OPS_OK, 0, 0, 0, "");
		return 1;

	case OPG_GROUP_LEAVE:
		// Check if the account has access to this group
		ret = accountHasAccessToGroupDb(account, message->payload);
		if (ret == -1) {
			packMessage(message, OPS_ERR_SERVERFAIL, 0, 0, 0, "");
			return 1;
		}
		
		if (ret == 0) {
			packMessage(message, OPS_ERR_FORBIDDEN, 0, 0, 0, "");
			return 1;
		}

		// Find the group in 
		for (auto it = groupList.begin(); it != groupList.end(); ++it) {
			if (strcmp(it->groupName, message->payload) == 0) {
				group = &(*it);
				break;
			}
		}

		if (group == NULL) {
			packMessage(message, OPS_ERR_NOTFOUND, 0, 0, 0, "");
			return 1;
		}

		// Delete permission from database
		if (deleteUserFromGroupDb(account, group)) {
			packMessage(message, OPS_ERR_SERVERFAIL, 0, 0, 0, "");
			return 1;
		}

		packMessage(message, OPS_OK, 0, 0, 0, "");
		return 1;

	case OPG_GROUP_NEW:

		// Verify group name:

		if (!isValidName(message->payload)) {
			packMessage(message, OPS_ERR_BADREQUEST, 0, 0, 0, "");
			return 1;
		}

		// Check if a group with the same name already exists
		for (auto it = groupList.begin(); it != groupList.end(); ++it) {
			if (strcmp(it->groupName, message->payload) == 0) {
				group = &(*it);
				break;
			}
		}

		if (group != NULL) {
			packMessage(message, OPS_ERR_GROUPEXISTS, 0, 0, 0, "");
			return 1;
		}

		// Initialize new group
		Group newGroup;
		newGroup.ownerId = account->uid;
		strcpy_s(newGroup.groupName, GROUPNAME_SIZE, message->payload);

		char path[MAX_PATH];
		strcpy_s(newGroup.pathName, MAX_PATH, newGroup.groupName);

		while (1) {
			snprintf(path, MAX_PATH, "%s/%s", STORAGE_LOCATION, newGroup.pathName);
			if (CreateDirectoryA(path, NULL) == 0) {
				if (GetLastError() == ERROR_ALREADY_EXISTS) {
					printf("Cannot create directory with path %s as it already exists", path);
					strcat_s(newGroup.pathName, GROUPNAME_SIZE, "_");
					continue;
				}
				else {
					packMessage(message, OPS_ERR_SERVERFAIL, 0, 0, 0, "");
					return 1;
				}
			}
			break;
		}

		// Add group to database
		if (addGroupDb(&newGroup)) {
			if (RemoveDirectoryA(path) == 0) {
				printf("Cannot remove directory at path %s. Error code %d!", path, GetLastError());
			}
			packMessage(message, OPS_ERR_SERVERFAIL, 0, 0, 0, "");
			return 1;
		}

		// Add user to group
		if (addUserToGroupDb(account, &newGroup)) {
			if (RemoveDirectoryA(path) == 0) {
				printf("Cannot remove directory with path %s. Error code %d!", path, GetLastError());
			}
			packMessage(message, OPS_ERR_SERVERFAIL, 0, 0, 0, "");
			return 1;
		}

		groupList.push_back(newGroup);
		packMessage(message, OPS_OK, 0, 0, 0, "");
		return 1;
	}
	return 0;
}

int processOpContinue(BUFFER_OBJ* bufferObj) {

	// Find account
	auto accountSearch = socketAccountMap.find(bufferObj->sock->s);
	if (accountSearch == socketAccountMap.end()) {
		packMessage(&(bufferObj->sock->mess), OPS_ERR_FORBIDDEN, 0, 0, 0, "");
		return 1;
	}

	Account* account = accountSearch->second;

	// Dequeue message and send 
	if (account->queuedMess != NULL) {
		bufferObj->sock->mess = account->queuedMess->mess;
		LPMESSAGE_LIST ptr = account->queuedMess->next;
		free(account->queuedMess);
		account->queuedMess = ptr;
		return 1;
	}
	else {
		packMessage(&(bufferObj->sock->mess), OPS_ERR_BADREQUEST, 0, 0, 0, "");
		return 1;
	}
	return 0;
}

int processOpBrowsing(BUFFER_OBJ* bufferObj) {

	LPMESSAGE message = &(bufferObj->sock->mess);

	// Find account
	auto accountSearch = socketAccountMap.find(bufferObj->sock->s);
	if (accountSearch == socketAccountMap.end()) {
		packMessage(message, OPS_ERR_FORBIDDEN, 0, 0, 0, "");
		return 1;
	}

	Account* account = accountSearch->second;

	// If account is not using any group, forbid browsing
	if (account->workingGroup == NULL) {
		packMessage(message, OPS_ERR_FORBIDDEN, 0, 0, 0, "");
		return 1;
	}

	WIN32_FIND_DATAA FindFileData;
	HANDLE hFind;
	std::list<MESSAGE> fileList;
	std::list<MESSAGE> folderList;
	char fullPath[MAX_PATH];
	char path[MAX_PATH];

	switch (message->opcode) {
	case OPB_LIST:
		MESSAGE newMessage;

		// Construct path
		snprintf(path, MAX_PATH, "%s/%s", STORAGE_LOCATION, account->workingGroup->pathName);
		if (strlen(account->workingDir) > 0) {
			snprintf(path, MAX_PATH, "%s/%s", path, account->workingDir);
		}
		strcat_s(path, MAX_PATH, "/*");

		// Find files
		hFind = FindFirstFileA(path, &FindFileData);
		if (hFind == INVALID_HANDLE_VALUE) {
			if (GetLastError() != ERROR_NO_MORE_FILES) {
				printf("FindFirstFile failed (%d)\n", GetLastError());
				packMessage(message, OPS_ERR_SERVERFAIL, 0, 0, 0, "");
				return 1;
			}
		}
		else {
			while (1) {
				if (FindFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
					packMessage(&newMessage, OPB_DIR_NAME, strlen(FindFileData.cFileName), 0, 0, FindFileData.cFileName);
					folderList.push_back(newMessage);
				}
				else {
					packMessage(&newMessage, OPB_FILE_NAME, strlen(FindFileData.cFileName), 0, 0, FindFileData.cFileName);
					fileList.push_back(newMessage);
				}

				if (FindNextFileA(hFind, &FindFileData) == 0) {
					if (GetLastError() == ERROR_NO_MORE_FILES) {
						break;
					}
					else {
						printf("FindNextFile failed (%d)\n", GetLastError());
						packMessage(message, OPS_ERR_SERVERFAIL, 0, 0, 0, "");
						return 1;
					}
				}
			}

			FindClose(hFind);
		}

		// Construct messages and enqueue pending sends
		char count[10];
		LPMESSAGE_LIST listPtr;

		// Send file count
		_itoa(fileList.size(), count, 10);
		packMessage(message, OPB_FILE_COUNT, 0, 0, 0, count);

		// Add directory count to wait queue
		_itoa(folderList.size(), count, 10);
		packMessage(&newMessage, OPB_DIR_COUNT, 0, 0, 0, count);
		account->queuedMess = (LPMESSAGE_LIST)malloc(sizeof(MESSAGE_LIST));
		listPtr = account->queuedMess;
		listPtr->mess = newMessage;

		// Add file name to wait queue
		for (auto it = fileList.begin(); it != fileList.end(); it++) {
			listPtr->next = (LPMESSAGE_LIST)malloc(sizeof(MESSAGE_LIST));
			listPtr = listPtr->next;
			listPtr->mess = *it;
		}

		// Add directory name to wait queue
		for (auto it = folderList.begin(); it != folderList.end(); it++) {
			listPtr->next = (LPMESSAGE_LIST)malloc(sizeof(MESSAGE_LIST));
			listPtr = listPtr->next;
			listPtr->mess = *it;
		}

		return 1;

	case OPB_FILE_CD:

		// Check if the requested path is valid
		if (!isValidName(message->payload)) {
			packMessage(message, OPS_ERR_BADREQUEST, 0, 0, 0, "");
			return 1;
		}

		// Check if this is a special navigation
		if (strcmp(message->payload, "..") == 0) {
			if (strlen(account->workingDir) == 0) {
				packMessage(message, OPS_ERR_FORBIDDEN, 0, 0, 0, "");
				return 1;
			}
			else {
				char* lastSlash = strrchr(account->workingDir, MAX_PATH);
				if (lastSlash) {
					*lastSlash = 0;
				}
				else {
					account->workingDir[0] = 0;
				}
				packMessage(message, OPS_OK, 0, 0, 0, "");
				return 1;
			}
		}

		if (strcmp(message->payload, ".") == 0) {
			packMessage(message, OPS_OK, 0, 0, 0, "");
			return 1;
		}

		// Check if path exists
		if (strlen(account->workingDir) == 0){
			snprintf(path, MAX_PATH, "%s", message->payload);
		}
		else {
			snprintf(path, MAX_PATH, "%s/%s", account->workingDir, message->payload);
		}
		snprintf(fullPath, MAX_PATH, "%s/%s/%s", STORAGE_LOCATION, account->workingGroup->pathName, path);

		hFind = FindFirstFileA(fullPath, (LPWIN32_FIND_DATAA)&FindFileData);
		if (hFind == INVALID_HANDLE_VALUE) {
			if (GetLastError() == ERROR_NO_MORE_FILES) {
				packMessage(message, OPS_ERR_NOTFOUND, 0, 0, 0, "");
				return 1;
			}
			printf("FindFirstFile failed (%d)\n", GetLastError());
			packMessage(message, OPS_ERR_SERVERFAIL, 0, 0, 0, "");
			return 1;
		}
		else {
			strcpy(account->workingDir, path);
			packMessage(message, OPS_OK, 0, 0, 0, "");
			FindClose(hFind);
			return 1;
		}

	case OPB_FILE_DEL:
		// Check if account is the group owner
		if (account->uid != account->workingGroup->ownerId) {
			packMessage(message, OPS_ERR_FORBIDDEN, 0, 0, 0, "");
			return 1;
		}

		// Delete file
		if (strlen(account->workingDir) == 0) {
			snprintf(path, MAX_PATH, "%s", message->payload);
		}
		else {
			snprintf(path, MAX_PATH, "%s/%s", account->workingDir, message->payload);
		}
		snprintf(fullPath, MAX_PATH, "%s/%s/%s", STORAGE_LOCATION, account->workingGroup->pathName, path);

		if (DeleteFileA(fullPath) == 0) {
			if (GetLastError() == ERROR_FILE_NOT_FOUND) {
				printf("Cannot remove file %s. File not found!", fullPath);
				packMessage(message, OPS_ERR_NOTFOUND, 0, 0, 0, "");
				return 1;
			}
			printf("Cannot remove file %s. Error code %d!", fullPath, GetLastError());
			packMessage(message, OPS_ERR_SERVERFAIL, 0, 0, 0, "");
			return 1;
		}
		packMessage(message, OPS_OK, 0, 0, 0, "");
		return 1;

	case OPB_DIR_DEL:

		// Check if account is group owner
		if (account->uid != account->workingGroup->ownerId) {
			packMessage(message, OPS_ERR_FORBIDDEN, 0, 0, 0, "");
			return 1;
		}

		if (strlen(account->workingDir) == 0) {
			snprintf(path, MAX_PATH, "%s", message->payload);
		}
		else {
			snprintf(path, MAX_PATH, "%s/%s", account->workingDir, message->payload);
		}
		snprintf(fullPath, MAX_PATH, "%s/%s/%s", STORAGE_LOCATION, account->workingGroup->pathName, path);

		// Delete directory
		if (RemoveDirectoryA(fullPath) == 0) {
			printf("Cannot remove directory with path %s. Error code %d!", fullPath, GetLastError());
			packMessage(message, OPS_ERR_SERVERFAIL, 0, 0, 0, "");
			return 1;
		}
		packMessage(message, OPS_OK, 0, 0, 0, "");
		return 1;

	case OPB_DIR_NEW:
		// Construct full path
		if (strlen(account->workingDir) == 0) {
			snprintf(path, MAX_PATH, "%s", message->payload);
		}
		else {
			snprintf(path, MAX_PATH, "%s/%s", account->workingDir, message->payload);
		}
		snprintf(fullPath, MAX_PATH, "%s/%s/%s", STORAGE_LOCATION, account->workingGroup->pathName, path);

		if (CreateDirectoryA(fullPath, NULL) == 0) {
			if (GetLastError() == ERROR_ALREADY_EXISTS) {
				printf("Cannot create directory with path %s as it already exists", fullPath);
				packMessage(message, OPS_ERR_ALREADYEXISTS, 0, 0, 0, "");
			}
			else {
				packMessage(message, OPS_ERR_SERVERFAIL, 0, 0, 0, "");
				return 1;
			}
		}

		packMessage(message, OPS_OK, 0, 0, 0, "");
		return 1;
	}
	return 0;
}


/*
Extract information from request and call corresponding process function
Function returns 1 if a response is ready to be sent, else returns 0
[IN] sock:		the socket that's sending the request
[IN/OUT] buff:	a char array which contains the request, and stores response
after the request has been processed
[OUT] len:		the length of the payload of the output
*/
int parseAndProcess(BUFFER_OBJ* bufferObj) {
	LPMESSAGE message = &(bufferObj->sock->mess);

	// Process request
	switch (message->opcode) {
		// Authentication operations
	case OPA_REAUTH:
		return processOpReauth(bufferObj);
	case OPA_REQ_COOKIES:
		return processOpRequestCookie(bufferObj);
	case OPA_LOGIN:
		return processOpLogIn(bufferObj);
	case OPA_LOGOUT:
		return processOpLogOut(bufferObj);

	case OPG_GROUP_LIST:
	case OPG_GROUP_USE:
	case OPG_GROUP_JOIN:
	case OPG_GROUP_LEAVE:
	case OPG_GROUP_NEW:
		return processOpGroup(bufferObj);

	case OPB_LIST:
	case OPB_FILE_CD:
	case OPB_FILE_DEL:
	case OPB_DIR_DEL:
	case OPB_DIR_NEW:
		return processOpBrowsing(bufferObj);

	case OPS_CONTINUE:
		return processOpContinue(bufferObj);

	default:
		printf("Bad request!\n");
		packMessage(&(bufferObj->sock->mess), OPS_ERR_BADREQUEST, 0, 0, 0, "");
		return 1;
	}
}

#endif