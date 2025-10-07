#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_USERS 5
#define MAX_RESOURCES 5
#define MAX_NAME_LEN 20

typedef enum{
    READ = 1,
    WRITE = 2,
    EXECUTE = 4
}Permission;

typedef struct{
    char name[MAX_NAME_LEN];
}User;

typedef struct{
    char name[MAX_NAME_LEN];
}Resource;

typedef struct{
    char userName[MAX_NAME_LEN];
    int permissions;
}ACLEntry;

typedef struct{
    Resource res;
    ACLEntry entries[MAX_USERS];
    int entryCount;
}ACLControlledResource;

typedef struct{
    char resourceName[MAX_NAME_LEN];
    int permissions;
}Capability;

typedef struct{
    User user;
    Capability caps[MAX_RESOURCES];
    int capCount;
}CapabilityUser;

void printPermissions(int perm){
    if (perm & READ) printf("READ ");
    if (perm & WRITE) printf("WRITE ");
    if (perm & EXECUTE) printf("EXECUTE ");
}

int hasPermission(int userPerm, int requiredPerm){
    return (userPerm & requiredPerm) == requiredPerm;
}

void checkACLAccess(ACLControlledResource *res, const char *userName, int perm){
    for (int i = 0; i < res->entryCount; i++){
        if (strcmp(res->entries[i].userName, userName) == 0){
            printf("ACL Check: User %s requests ", userName);
            printPermissions(perm);
            printf("on %s: ", res->res.name);
            if (hasPermission(res->entries[i].permissions, perm))
                printf("Access GRANTED\n");
            else
                printf("Access DENIED\n");
            return;
        }
    }
    printf("ACL Check: User %s has NO entry for resource %s: Access DENIED\n",
           userName, res->res.name);
}

void checkCapabilityAccess(CapabilityUser *user, const char *resourceName, int perm){
    for (int i = 0; i < user->capCount; i++){
        if (strcmp(user->caps[i].resourceName, resourceName) == 0){
            printf("Capability Check: User %s requests ", user->user.name);
            printPermissions(perm);
            printf("on %s: ", resourceName);
            if (hasPermission(user->caps[i].permissions, perm))
                printf("Access GRANTED\n");
            else
                printf("Access DENIED\n");
            return;
        }
    }
    printf("Capability Check: User %s has NO capability for %s: Access DENIED\n",
           user->user.name, resourceName);
}

int main(){
    User users[MAX_USERS] = {{"Alice"}, {"Bob"}, {"Charlie"}, {"Abrar"}, {"Ayman"}};
    Resource resources[MAX_RESOURCES] = {{"File1"}, {"File2"}, {"File3"}, {"File4"}, {"File5"}};

    ACLControlledResource aclRes[MAX_RESOURCES];
    for (int i = 0; i < MAX_RESOURCES; i++){
        aclRes[i].res = resources[i];
        aclRes[i].entryCount = 0;
    }

    strcpy(aclRes[0].entries[0].userName, "Alice");
    aclRes[0].entries[0].permissions = READ | WRITE;
    strcpy(aclRes[0].entries[1].userName, "Bob");
    aclRes[0].entries[1].permissions = READ;
    aclRes[0].entryCount = 2;

    strcpy(aclRes[1].entries[0].userName, "Alice");
    aclRes[1].entries[0].permissions = READ;
    aclRes[1].entryCount = 1;

    strcpy(aclRes[2].entries[0].userName, "Abrar");
    aclRes[2].entries[0].permissions = WRITE;
    aclRes[2].entryCount = 1;

    strcpy(aclRes[3].entries[0].userName, "Ayman");
    aclRes[3].entries[0].permissions = READ | WRITE | EXECUTE;
    aclRes[3].entryCount = 1;

    strcpy(aclRes[4].entries[0].userName, "Alice");
    aclRes[4].entries[0].permissions = READ;
    aclRes[4].entryCount = 1;

    CapabilityUser capUsers[MAX_USERS];
    for (int i = 0; i < MAX_USERS; i++){
        capUsers[i].user = users[i];
        capUsers[i].capCount = 0;
    }

    strcpy(capUsers[0].caps[0].resourceName, "File1");
    capUsers[0].caps[0].permissions = READ | WRITE;
    strcpy(capUsers[0].caps[1].resourceName, "File5");
    capUsers[0].caps[1].permissions = READ;
    capUsers[0].capCount = 2;

    strcpy(capUsers[1].caps[0].resourceName, "File1");
    capUsers[1].caps[0].permissions = READ;
    capUsers[1].capCount = 1;

    capUsers[2].capCount = 0;

    strcpy(capUsers[3].caps[0].resourceName, "File3");
    capUsers[3].caps[0].permissions = WRITE;
    capUsers[3].capCount = 1;

    strcpy(capUsers[4].caps[0].resourceName, "File4");
    capUsers[4].caps[0].permissions = READ | WRITE | EXECUTE;
    capUsers[4].capCount = 1;

    checkACLAccess(&aclRes[0], "Alice", READ);
    checkACLAccess(&aclRes[0], "Bob", WRITE);
    checkACLAccess(&aclRes[0], "Charlie", READ);
    checkACLAccess(&aclRes[2], "Abrar", WRITE);
    checkACLAccess(&aclRes[3], "Ayman", EXECUTE);
    checkACLAccess(&aclRes[4], "Bob", READ);

    printf("\n");

    checkCapabilityAccess(&capUsers[0], "File1", WRITE);
    checkCapabilityAccess(&capUsers[1], "File1", WRITE);
    checkCapabilityAccess(&capUsers[2], "File1", READ);
    checkCapabilityAccess(&capUsers[3], "File3", WRITE);
    checkCapabilityAccess(&capUsers[4], "File4", READ);
    checkCapabilityAccess(&capUsers[0], "File3", READ);

    return 0;
}