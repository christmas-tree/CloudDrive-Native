
#ifndef UI_H_
#define UI_H_

#include <stdlib.h>
#include "processor.h"

using namespace std;

bool quit = false;
extern bool isLoggedIn;
char username[CRE_MAXLEN];
char password[CRE_MAXLEN];
char* currentGroup;
char filePath[MAX_PATH];

vector<char*> dirList;
vector<char*> fileList;

void handleLogIn();
void handleLogOut();
void handleNavigate();
void handleNewFolder();
void handleUpload();
void handleDownload();
void handleDelete();
void handleVisitGroup();
void handleCreateGroup();
void handleJoinGroup();
void handleLeaveGroup();
void showMenuNotLoggedIn();
void showMenuLoggedIn();
void showGroupMenu();

/*
- Function: getPassword
- Description: Get password from stdin and show asterisks
in place of characters
- [OUT] password: a char array to store the input password
*/
void getPassword(char *password) {
	char ch = 0;
	int i = 0;

	// Get console mode and disable character echo
	DWORD conMode, dwRead;
	HANDLE handleIn = GetStdHandle(STD_INPUT_HANDLE);

	GetConsoleMode(handleIn, &conMode);
	SetConsoleMode(handleIn, conMode & ~(ENABLE_ECHO_INPUT | ENABLE_LINE_INPUT));

	// Read character and echo * to screen
	while (ReadConsoleA(handleIn, &ch, 1, &dwRead, NULL) && ch != '\r') {
		if (ch == '\b') {
			if (i > 0) {
				printf("\b \b");
				i--;
			}
		}
		else if (i < CRE_MAXLEN - 1 && ch != ' ') {
			password[i++] = ch;
			printf("*");
		}
	}
	password[i] = 0;

	// Restore original console mode
	SetConsoleMode(handleIn, conMode);
}

/*
- Function: getInput
- Description: Get input from stdin. If input is longer than
buffer len then flush the input buffer. Remove the newline
character at string end if exists.
- Return: Return 1 if input is empty, else return 0.
- [OUT] s:		a char array to store the input
- [IN] maxLen:	the maximum length of the char array
*/
bool getInput(char* s, size_t maxLen) {
	fgets(s, maxLen, stdin);
	int slen = strlen(s);
	// If s is an emptry string, return 1
	if ((slen == 0) || ((slen == 1) && (s[0] = '\n')))
		return 1;
	// Purge newline character if exists
	if (s[slen - 1] == '\n')
		s[slen - 1] = 0;
	else {
		// Flush stdin
		char ch;
		while ((ch = fgetc(stdin)) != '\n' && ch != EOF);
	}
	return 0;
}


/*
- Function: getNumber
- Description: Get a number from stdin. If input is longer than
buffer len then flush the input buffer. Remove the newline character
at string end if exists.
- Return: 1 if input is empty, return -1 if input is not a number,
else return 0.
- [OUT] num: an int to store the input number
*/
int getNumber(int *num) {
	char buf[15];
	int ret = getInput(buf, 15);
	for (int i = 0; buf[i] != 0; i++) {
		if (buf[i] > '9' || buf[i] < '0')
			return -1;
	}
	*num = atoi(buf);
	return 0;
}

/*
- Function: isValidName
- Description: Check if a char array is a valid file/directory name.
- Return: 1 if is a valid name, else return 0
- [IN] s: a char array to check
*/
bool isValidName(char* s) {
	char prohibitedChars[] = {'<', '>', ':', '\"', '/', '\\', '|', '?', '*'};
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

/*
- Function: showStatusMsg
- Description: Show the elaborated message depending
on the status code.
- [IN] statusCode: the status code
- [IN] opType: a char array storing the name of the operation.
*/
void showStatusMsg(int statusCode, char* opType) {
	switch (statusCode) {
	case OPS_OK:
		printf("%s successfully.\n", opType);
		break;;
	case OPS_ERR_BADREQUEST:
		printf("Bad request error!\n");
		break;
	case OPS_ERR_WRONGPASS:
		printf("Wrong password. Please try again!\n");
		break;
	case OPS_ERR_NOTLOGGEDIN:
		printf("You did not log in!\n");
		break;
	case OPS_ERR_LOCKED:
		printf("Account locked!\n");
		break;
	case OPS_ERR_ANOTHERCLIENT:
		printf("Account logged in on another client!\n");
		break;
	case OPS_ERR_ALREADYINGROUP:
		printf("Account is already in this group!\n");
		break;
	case OPS_ERR_GROUPEXISTS:
		printf("A group with this name already exists!\n");
		break;
	case OPS_ERR_ALREADYEXISTS:
		printf("Item already exists.\n");
		break;
	case OPS_ERR_SERVERFAIL:
		printf("Internal server error.\n");
		break;
	case OPS_ERR_FORBIDDEN:
		printf("You don't have permission to perform this action.\n");
		break;
	case OPS_ERR_NOTFOUND:
		printf("Not found!\n");
		break;
	default:
		printf("Unknown return code from Server: %d\n", statusCode);
		break;
	}
	Sleep(1000);
}

/*
- Function: showMenuNotLoggedIn
- Description: Show menu when user has not logged in, get
user choice and call related handle functions.
*/
void showMenuNotLoggedIn() {
	system("cls");
	printf("|================+++============|\n");
	printf("|         File Transfer         |\n");
	printf("|_____________[Menu]____________|\n");
	printf("|                               |\n");
	printf("|       1. Login                |\n");
	printf("|       2. Exit                 |\n");
	printf("|                               |\n");
	printf("|===============================|\n");

	char choice = '.';
	do {
		choice = _getch();
		switch (choice) {
		case '1':
			return handleLogIn();
		case '2':
			quit = true;
			return;
		default:
			printf("\nInvalid choice. Please choose again: ");
			choice = '.';
		}
	} while (choice == '.');
}

/*
- Function: showMenuLoggedIn
- Description: Show menu when user has logged in, get
user choice and call related handle functions.
*/
void showMenuLoggedIn() {
	system("cls");
	printf("|================+++============|\n");
	printf("|         File Transfer         |\n");
	printf("|_____________[Menu]____________|\n");
	printf("|                               |\n");
	printf("|      1. Visit group           |\n");
	printf("|      2. Create new group      |\n");
	printf("|      3. Join group            |\n");
	printf("|      4. Log out               |\n");
	printf("|      5. Exit                  |\n");
	printf("|                               |\n");
	printf("|===============================|\n");

	char choice = '.';
	do {
		choice = _getch();
		switch (choice) {
		case '1':
			return handleVisitGroup();
		case '2':
			return handleCreateGroup();
		case '3':
			return handleJoinGroup();
		case '4':
			return handleLogOut();
		case '5':
			quit = true;
			return;
		default:
			printf("\nInvalid choice. Please choose again: ");
			choice = '.';
		}
	} while (choice == '.');
}

/*
- Function: showGroupMenu
- Description: Show browsing menu, get and show files
in current directory, get user choice and call related handle functions.
*/
void showGroupMenu() {
	while (1) {
		system("cls");
		printf("|================+++============|\n");
		printf("|         File Transfer         |\n");
		printf("|_____________[Menu]____________|\n");
		printf("|                               |\n");
		printf("|      1. Navigate              |\n");
		printf("|      2. Create new folder     |\n");
		printf("|      3. Upload file           |\n");
		printf("|      4. Download file         |\n");
		printf("|      5. Delete file           |\n");
		printf("|      6. Leave group           |\n");
		printf("|      7. Back                  |\n");
		printf("|      8. Exit                  |\n");
		printf("|                               |\n");
		printf("|===============================|\n");

		// Clear file list and directory list
		fileList.clear();
		dirList.clear();

		// Get file list and directory list from Server
		int ret = processOpFileList(fileList, dirList);
		if (ret == 1) {
			printf("\nError sending to server.\n");
			Sleep(2000);
			return;
		}

		printf("\n%d file%s and %d subfolder%s.\n\n", fileList.size(), (fileList.size() > 1) ? "s" : "",
			dirList.size(), (dirList.size() > 1) ? "s" : "");

		printf("\033[0;36m");
		for (char* file : fileList) {
			printf("File:\t%s\n", file);
		}
		printf("\033[0m");

		printf("\033[0;33m");
		for (char* dir : dirList) {
			printf("Folder:\t%s\n", dir);
		}
		printf("\033[0m");

		char choice = '.';
		do {
			choice = _getch();
			switch (choice) {
			case '1':
				handleNavigate();
				break;
			case '2':
				handleNewFolder();
				break;
			case '3':
				handleUpload();
				break;
			case '4':
				handleDownload();
				break;
			case '5':
				handleDelete();
				break;
			case '6':
				handleLeaveGroup();
				break;
			case '7':
				return;
			case '8':
				quit = true;
				return;
			default:
				printf("\nInvalid choice. Please choose again: ");
				choice = '.';
			}
		} while (choice == '.');
	}
}


/*****************************
Group
*****************************/

/*
- Function: handleVisitGroup
- Description: Show group list, get user choice and call function to send
the request to server.
*/
void handleVisitGroup() {
	printf("\n");

	int groupCount = 0;
	vector<char*> groupList;

	int ret = processOpGroupList(groupList);
	if (ret == 1) {
		printf("\nError sending to server.\n");
		Sleep(2000);
		return;
	}

	for (char* group : groupList) {
		printf("%d. %s\n", ++groupCount, group);
	}

	if (groupCount == 0) {
		printf("\nYou are not in any group. Try joining one!\n");
		return;
	} 

	int selection;
	bool valid;
	do {
		valid = true;
		printf("\nChoose group: ");
		ret = getNumber(&selection);
		if (ret == 1) return;
		else if (ret == -1) valid = false;
		else if (selection > groupCount || selection <= 0) {
			valid = false;
		}

	} while (!valid);
	printf("\n\n");

	ret = processOpGroup(OPG_GROUP_USE, groupList[selection - 1]);
	if (ret == 1) {
		printf("\nError sending to server.\n");
		for (char* group : groupList) {
			free(group);
		}

		Sleep(2000);
		return;
	}

	// Free other groups
	currentGroup = groupList[selection - 1];
	for (char* group : groupList) {
		if (group != currentGroup) free(group);
	}

	showStatusMsg(ret, "Enter group");

	showGroupMenu();
}

/*
- Function: handleCreateGroup
- Description: Get user input and call function to send
the request to server.
*/
void handleCreateGroup() {
	char newGroupName[GROUPNAME_SIZE];
	printf("\n");

	do {
		printf("Group name should be %d characters or less.\n", GROUPNAME_SIZE);
		printf("Don't input over %d characters, else it will be truncated.\n\n", GROUPNAME_SIZE);
		printf("New group name: ");

		if (getInput(newGroupName, GROUPNAME_SIZE)) {
			return;
		}
		
	} while (!isValidName(newGroupName));

	printf("\n\n");

	int ret = processOpGroup(OPG_GROUP_NEW, newGroupName);
	if (ret == 1) {
		printf("\nError sending to server.\n");
		Sleep(1000);
		return;
	}
	showStatusMsg(ret, "Create group");
}

/*
- Function: handleJoinGroup
- Description: Get user input and call function to send
the request to server.
*/
void handleJoinGroup() {
	char groupName[GROUPNAME_SIZE];
	printf("\n");

	do {
		printf("Group name should be %d characters or less.\n", GROUPNAME_SIZE);
		printf("Don't input over %d characters, else it will be truncated.\n\n", GROUPNAME_SIZE);
		printf("Enter group name to join: ");

		if (getInput(groupName, GROUPNAME_SIZE)) {
			return;
		}

	} while (!isValidName(groupName));

	printf("\n\n");

	int ret = processOpGroup(OPG_GROUP_JOIN, groupName);
	if (ret == 1) {
		printf("\nError sending to server.\n");
		Sleep(2000);
		return;
	}
	showStatusMsg(ret, "Join group");
}

/*
- Function: handleLeaveGroup
- Description: Get user input and call function to send
the request to server.
*/
void handleLeaveGroup() {
	char ch;
	int iRetVal;
	printf("\n");
	printf("Are you sure you want to leave this group? (Y/..): ");
	ch = _getch();
	if (ch == 'Y' || ch == 'y') {
		iRetVal = processOpGroup(OPG_GROUP_LEAVE, currentGroup);
		if (iRetVal == 1) {
			printf("\nError connecting to server.\n");
			Sleep(2000);
			return;
		}
		showStatusMsg(iRetVal, "Leave group");
		return;
	}
	printf("\nLeave group aborted.");
	Sleep(1000);
}


/*****************************
Browsing
*****************************/

/*
- Function: handleNavigate
- Description: Get user input and call function to send
the request to server.
*/
void handleNavigate() {
	char path[MAX_PATH];
	printf("\n");

	do {
		printf("Path should be %d characters or less.\n", MAX_PATH);
		printf("Don't input over %d characters, else it will be truncated.\n\n", MAX_PATH);
		printf("Where to? ");

		if (getInput(path, GROUPNAME_SIZE)) {
			return;
		}
	} while (!isValidName(path));

	printf("\n\n");

	int ret = processOpBrowse(OPB_FILE_CD, path);
	if (ret == 1) {
		printf("\nError sending to server.\n");
		Sleep(2000);
		return;
	}

	showStatusMsg(ret, "Navigating");
}

/*
- Function: handleNewFolder
- Description: Get user input and call function to send
the request to server.
*/
void handleNewFolder() {
	char newFolderName[MAX_PATH];
	printf("\n");

	do {
		printf("Folder name should be %d characters or less.\n", MAX_PATH);
		printf("Don't input over %d characters, else it will be truncated.\n\n", MAX_PATH);
		printf("New folder name: ");

		if (getInput(newFolderName, GROUPNAME_SIZE)) {
			return;
		}
	} while (!isValidName(newFolderName));
	printf("\n\n");

	int ret = processOpBrowse(OPB_DIR_NEW, newFolderName);
	if (ret == 1) {
		printf("\nError sending to server.\n");
		Sleep(2000);
		return;
	}

	showStatusMsg(ret, "Create new folder");
}

/*
- Function: handleDelete
- Description: Get user input and call function to send
the request to server.
*/
void handleDelete() {
	char deleteItem[MAX_PATH];
	bool valid;
	bool isFolder;
	int ret;
	printf("\n");
	
	do {
		valid = false;
		printf("File/folder name should be %d characters or less.\n", MAX_PATH);
		printf("Don't input over %d characters, else it will be truncated.\n\n", MAX_PATH);
		printf("File/folder name: ");

		if (getInput(deleteItem, GROUPNAME_SIZE)) {
			return;
		}

		if (isValidName(deleteItem)) {
			for (unsigned int i = 0; i < fileList.size(); i++) {
				if (strcmp(fileList[i], deleteItem) == 0) {
					isFolder = false;
					valid = true;
					break;
				}
			}

			if (!valid) {
				for (unsigned int i = 0; i < dirList.size(); i++) {
					if (strcmp(dirList[i], deleteItem) == 0) {
						isFolder = true;
						valid = true;
						break;
					}
				}
			}
		}
	} while (!valid);
	printf("\n\n");

	if (isFolder)
		ret = processOpBrowse(OPB_DIR_DEL, deleteItem);
	else
		ret = processOpBrowse(OPB_FILE_DEL, deleteItem);

	if (ret == 1) {
		printf("\nError sending to server.\n");
		Sleep(2000);
		return;
	}

	showStatusMsg(ret, "Delete item");
}

/*
- Function: handleUpload
- Description: Get file name to upload and call function to send
file to server.
*/
void handleUpload() {
	printf("\n");

	do {
		printf("File name should be %d characters or less.\n", MAX_PATH);
		printf("Don't input over %d characters, else it will be truncated.\n\n", MAX_PATH);
		printf("Enter file path on your computer: ");

		if (getInput(filePath, GROUPNAME_SIZE)) {
			return;
		}

	} while (!isValidName(filePath));
	printf("\n\n");

	uploadFileToServer(filePath);
	Sleep(2000);
	return;
}

/*
- Function: handleDownload
- Description: Get file name to download and call function to download
file from server.
*/
void handleDownload() {
	printf("\n");

	do {
		printf("Path should be %d characters or less.\n", MAX_PATH);
		printf("Don't input over %d characters, else it will be truncated.\n\n", MAX_PATH);
		printf("Which file? ");
		if (getInput(filePath, GROUPNAME_SIZE)) {
			return;
		}

	} while (!isValidName(filePath));
	printf("\n\n");

	downloadFileFromServer(filePath);

	Sleep(2000);
}


/*****************************
Authentication
*****************************/

/*
- Function: handleLogOut
- Description: Confirm log out and call function to send request
to server
*/
void handleLogOut() {
	char ch;
	int iRetVal;

	printf("\n");
	printf("Are you sure you want to log out? (Y/..): ");

	ch = _getch();

	if (ch == 'Y' || ch == 'y') {
		iRetVal = processOpLogOut();
		if (iRetVal == 1) {
			printf("\nClient will quit.\n");
			quit = true;
			Sleep(2000);
			return;
		}
		showStatusMsg(iRetVal, "Log out");
		return;
	}
	printf("\nLog out aborted.");
	Sleep(1000);
}

/*
- Function: handleLogIn
- Description: Get username, password and call function to send request
to server
*/
void handleLogIn() {
	printf("Your username and password should be %d characters or less,\n", CRE_MAXLEN);
	printf("and cannot contain spaces.\n");
	printf("Don't input over %d characters, else it will be truncated.\n\n", CRE_MAXLEN);

	bool valid;
	do {
		valid = true;
		printf("Username: ");
		scanf_s("%s", username, (unsigned)_countof(username));

		printf("Password: ");
		getPassword(password);
		if (strlen(username) == 0) {
			printf("\nUsername cannot be empty.\n\n");
			valid = false;
		}
		if (strlen(password) == 0) {
			printf("\nPassword cannot be empty.\n\n");
			valid = false;
		}
	} while (!valid);
	printf("\n\n");

	int ret = processOpLogIn(username, password);
	if (ret == 1) {
		printf("\nClient will quit.\n");
		quit = true;
		Sleep(1000);
		return;
	}
	showStatusMsg(ret, "Log in");

	if (ret == OPS_OK) {
		ret = processOpReqCookie();
		if (ret == 1) {
			printf("\nClient will quit.\n");
			quit = true;
			Sleep(1000);
			return;
		}
		showStatusMsg(ret, "Request cookie");
	}
}


/*****************************
Runner
*****************************/

/*
- Function: runUI
- Description: User interface main loop
*/
void runUI() {

	// Try to reauthenticate
	int iRetVal = processOpReauth();
	if (iRetVal == -1) {
		printf("\nNo cookie found.\n");
	} else if (iRetVal == 1) {
		return;
	} else {
		showStatusMsg(iRetVal, "Reauthenticate");
	}

	// Main menu loop
	while (!quit) {
		if (isLoggedIn)
			showMenuLoggedIn();
		else
			showMenuNotLoggedIn();
	}
}

#endif