#pragma once

#ifndef _DBUTILS_H
#define _DBUTILS_H

#include "dataStructures.h"
#include "sqlite3.h"

#define DB_NAME		"data.db"

int accountHasAccessToGroupDb(Account* account, char* groupName);
int addUserToGroupDb(Account* account, Group* group);
int deleteUserFromGroupDb(Account* account, Group* group);
int addGroupDb(Group* group);


sqlite3 *db;


// Function: openDb
// Description: Open database and store database info in variable db
// Return: 0 if succeed, else return 1
int openDb() {
	char *err_msg = 0;
	int ret = sqlite3_open(DB_NAME, &db);
	if (ret) {
		fprintf(stderr, "Cannot open database: %s\n", sqlite3_errmsg(db));
		sqlite3_close(db);
		return 1;
	}
	return 0;
}

// Function: readAccountDb
// Description: Read account information from databse
// Return: 0 if succeed, else return 1
// -OUT: accList: linked list to store accounts read from database
int readAccountDb(std::list<Account>& accList) {
	Account acc;
	sqlite3_stmt *res;
	int ret;

	if (db == NULL) {
		fprintf(stderr, "Databased not opened!\n");
		return 1;
	}

	char *sql = "SELECT UID, USERNAME, PASSWORD, LOCKED FROM ACCOUNT;";
	ret = sqlite3_prepare_v2(db, sql, -1, &res, 0);
	if (ret != SQLITE_OK) {
		printf("Failed to execute statement: %s\n", sqlite3_errmsg(db));
	}

	while (sqlite3_step(res) == SQLITE_ROW) {
		acc.uid = sqlite3_column_int(res, 0);
		strcpy_s(acc.username, CRE_MAXLEN, (const char *) sqlite3_column_text(res, 1));
		strcpy_s(acc.password, CRE_MAXLEN, (const char *) sqlite3_column_text(res, 2));
		acc.isLocked = (sqlite3_column_int(res, 3) == 1);

		acc.mutex = CreateMutex(NULL, false, NULL);

		accList.push_back(acc);
	}

	sqlite3_finalize(res);
	printf("Account data loaded.\n");
	return 0;
}


// Function: readGroupDb
// Description: Read group information from databse
// Return: 0 if succeed, else return 1
// -OUT: groupList: linked list to store groups read from database
int readGroupDb(std::list<Group>& groupList) {
	Group group;
	sqlite3_stmt *res;
	int ret;

	if (db == NULL) {
		fprintf(stderr, "Databased not opened!\n");
		return 1;
	}

	char *sql = "SELECT GID, GROUPNAME, PATHNAME, OWNERID FROM [GROUP];";
	ret = sqlite3_prepare_v2(db, sql, -1, &res, 0);
	if (ret != SQLITE_OK) {
		printf("Failed to execute statement: %s\n", sqlite3_errmsg(db));
	}

	while (sqlite3_step(res) == SQLITE_ROW) {
		group.gid = sqlite3_column_int(res, 0);
		strcpy_s(group.groupName, CRE_MAXLEN, (const char *)sqlite3_column_text(res, 1));
		strcpy_s(group.pathName, MAX_PATH, (const char *)sqlite3_column_text(res, 2));
		group.ownerId = sqlite3_column_int(res, 3);

		groupList.push_back(group);
	}

	sqlite3_finalize(res);
	printf("Group data loaded.\n");
	return 0;
}

// Function: accountHasAccessToGroupDb
// Description: Read from database to see if an account has access to a group
// Return: 1 if the account has access, return 0 if not
//         return -1 if fail to read database
// -IN: account: Account to check
//      groupName: a char array which has the group name to check
int accountHasAccessToGroupDb(Account* account, char* groupName) {
	sqlite3_stmt *res;
	int ret;

	if (db == NULL) {
		fprintf(stderr, "Databased not opened!\n");
		return -1;
	}

	char *sql = "SELECT gm.GID, gm.UID FROM GROUPMEMBER gm "
		"JOIN [GROUP] g ON g.GID = gm.GID "
		"WHERE gm.UID = ? AND g.GROUPNAME = ?;";

	ret = sqlite3_prepare_v2(db, sql, -1, &res, 0);
	if (ret != SQLITE_OK) {
		printf("Failed to execute statement: %s\n", sqlite3_errmsg(db));
	}
	else {
		sqlite3_bind_int(res, 1, account->uid);
		sqlite3_bind_text(res, 2, groupName, -1, NULL);
	}

	ret = (sqlite3_step(res) == SQLITE_ROW);

	sqlite3_finalize(res);
	return ret;
}

// Function: queryGroupForAccount
// Description: Read from database for a list of the groups that
//              an user has access to.
// Return: 0 if succeed, else return 1
// -IN: account: Account to check
// -OUT: groupList: a list to store the group information
int queryGroupForAccount(Account* account, std::list<Group> &groupList) {
	sqlite3_stmt *res;
	int ret;

	if (db == NULL) {
		fprintf(stderr, "Databased not opened!\n");
		return -1;
	}

	char *sql = "SELECT g.GID, g.GROUPNAME FROM GROUPMEMBER gm "
		"JOIN [GROUP] g ON g.GID = gm.GID "
		"WHERE gm.UID = ?;";

	ret = sqlite3_prepare_v2(db, sql, -1, &res, 0);
	if (ret != SQLITE_OK) {
		printf("Failed to execute statement: %s\n", sqlite3_errmsg(db));
	}
	else {
		sqlite3_bind_int(res, 1, account->uid);
	}

	Group group;
	while (sqlite3_step(res) == SQLITE_ROW) {
		group.gid = sqlite3_column_int(res, 0);
		strcpy_s(group.groupName, GROUPNAME_SIZE, (const char *)sqlite3_column_text(res, 1));

		groupList.push_back(group);
	}

	sqlite3_finalize(res);
	return ret;
}

// Function: addUserToGroupDb
// Description: Add an user access to a group in database
// Return: 0 if succeed, else return 1
// -IN: account: Account to add
//      group:   Group to add
int addUserToGroupDb(Account* account, Group* group) {
	sqlite3_stmt *res;
	int ret;

	if (db == NULL) {
		fprintf(stderr, "Databased not opened!\n");
		return -1;
	}

	char *sql = "INSERT INTO GROUPMEMBER(GID, UID) VALUES (?, ?);";
	ret = sqlite3_prepare_v2(db, sql, -1, &res, 0);
	if (ret != SQLITE_OK) {
		printf("Failed to execute statement: %s\n", sqlite3_errmsg(db));
	}
	else {
		sqlite3_bind_int(res, 1, group->gid);
		sqlite3_bind_int(res, 2, account->uid);
	}

	ret = (sqlite3_step(res) != SQLITE_DONE);

	sqlite3_finalize(res);
	return ret;
}

// Function: deleteUserFromGroupDb
// Description: Remove an user's access to a group in database
// Return: 0 if succeed, else return 1
// -IN: account: Account to remove
//      group:   Group to remove
int deleteUserFromGroupDb(Account* account, Group* group) {
	sqlite3_stmt *res;
	int ret;

	if (db == NULL) {
		fprintf(stderr, "Databased not opened!\n");
		return -1;
	}

	char *sql = "DELETE FROM GROUPMEMBER WHERE uid = ? AND gid = ?;";
	ret = sqlite3_prepare_v2(db, sql, -1, &res, 0);
	if (ret != SQLITE_OK) {
		printf("Failed to execute statement: %s\n", sqlite3_errmsg(db));
	}
	else {
		sqlite3_bind_int(res, 1, account->uid);
		sqlite3_bind_int(res, 2, group->gid);
	}

	ret = (sqlite3_step(res) != SQLITE_DONE);

	sqlite3_finalize(res);
	return ret;
}

// Function: addGroupDb
// Description: add a new group to database
// Return: 0 if succeed, else return 1
// -IN: group:   Group to add to database
int addGroupDb(Group* group) {
	sqlite3_stmt *res;
	int ret;

	if (db == NULL) {
		fprintf(stderr, "Databased not opened!\n");
		return -1;
	}

	char *sql = "INSERT INTO [GROUP](GROUPNAME, PATHNAME, OWNERID) VALUES (?, ?, ?);";
	ret = sqlite3_prepare_v2(db, sql, -1, &res, 0);
	if (ret != SQLITE_OK) {
		printf("Failed to execute statement: %s\n", sqlite3_errmsg(db));
	}
	else {
		sqlite3_bind_text(res, 1, group->groupName, -1, NULL);
		sqlite3_bind_text(res, 2, group->pathName, -1, NULL);
		sqlite3_bind_int(res, 3, group->ownerId);
	}

	ret = (sqlite3_step(res) != SQLITE_DONE);

	sqlite3_finalize(res);
	group->gid = (int) sqlite3_last_insert_rowid(db);

	return ret;
}

// Function: lockAccountDb
// Description: update an user's status in database
// Return: 0 if succeed, else return 1
// -IN: account:   Account to update in database
int lockAccountDb(Account* account) {
	sqlite3_stmt *res;
	int ret;

	if (db == NULL) {
		fprintf(stderr, "Databased not opened!\n");
		return -1;
	}

	char *sql = "UPDATE ACCOUNT SET LOCKED = 1 WHERE UID=?;";
	ret = sqlite3_prepare_v2(db, sql, -1, &res, 0);
	if (ret != SQLITE_OK) {
		printf("Failed to execute statement: %s\n", sqlite3_errmsg(db));
	}
	else {
		sqlite3_bind_int(res, 1, account->uid);
	}

	ret = (sqlite3_step(res) != SQLITE_DONE);

	sqlite3_finalize(res);

	return ret;
}

// Function: closeDb
// Description: close the opened database
void closeDb() {
	sqlite3_close(db);
}

#endif