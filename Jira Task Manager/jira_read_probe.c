#include "native_jira.h"
#include <stdio.h>
int main(void){
    JiraConnection c;JiraSnapshot snapshot={0};
    if(!JiraConnect(&c)){wprintf(L"Connection failed: %ls\n",c.error);JiraDisconnect(&c);return 1;}
    BOOL ok=JiraLoad(&c,&snapshot);
    if(ok)printf("Read-only Jira check passed. Tasks: %d. No writes performed.\n",snapshot.count);
    else wprintf(L"Read failed: %ls\n",c.error);
    JiraFreeSnapshot(&snapshot);JiraDisconnect(&c);return ok?0:1;
}
