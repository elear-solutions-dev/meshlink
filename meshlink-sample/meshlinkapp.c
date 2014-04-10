#include <libmeshlink.h>

int main(int argc , char **argv){

char *confbase = "/tmp/meshlink/";
char *name = "test";

tincremotehost* remotenode = malloc(sizeof(tincremotehost));
char *remotename = "nameofremotenode";

//TODO: change this, calling a function that returns tincremotehost
remotenode->name = remotename;
remotenode->publickey = NULL;

tinc_setup(confbase, name);
tinc_start(confbase);
while(1) {

//sample data to send out
char mydata[200];
memset(mydata,0,200);
strcpy(mydata,"Hello World!");

//send out data
tinc_send_packet(remotenode,mydata,sizeof(mydata));
sleep(10); //give time to this thread to finish before we exit
}
free(remotenode);
return 0;
}

