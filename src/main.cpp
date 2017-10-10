#include <iostream>
#include <csignal>
#include <errno.h>
#include "logger/lib_logger.h"
#include "dynamic/lib_dynamic.h"
#include "can/can_drv.h"
#include "sharedMem/lib_sharedMem.h"

using namespace std;
using namespace boost::interprocess;

#define TAG "Main"

#define duiTotalFifoElem 100

struct can_frame TxCanMsg;

struct ProgramPosition{
	char name[100];
	int pid;
	double latitude;
	double longitude;
};

bool bIgnitionSet = false;

void SignalHandler(int signo)
{
	if(signo == SIGINT){
		ALOGI(TAG, __FUNCTION__, "Interrupt Signal catched");
		bIgnitionSet = true;
	}
}

int main(){

      ALOGI(TAG, __FUNCTION__, "Main Program Init");
      int dyn = iLog_eCreateFile_Exe("HelloLog.txt");
      ALOGI(TAG, __FUNCTION__, "Dynamic return %d", dyn);
      struct sigaction a;
	  a.sa_handler = SignalHandler;
	  a.sa_flags = 0;
	  sigemptyset( &a.sa_mask );
	  sigaction(SIGINT, &a, NULL );
      //Initialize CAN Interface
      CANDrv * CanInfDrv = new CANDrv("CANFIFO-VCan0");
      TxCanMsg.can_id = 0x0CFEF100;
      TxCanMsg.can_dlc = 8;
      strcpy((char*)TxCanMsg.data, "ABCDEFGH");
      CanInfDrv->CanSendMsg(TxCanMsg);
      
	  //Shared memory testing Structure
	  struct ProgramPosition TestPosWr, TestPosRd;
	  memset(TestPosWr.name, 0, 100);
	  strcpy(&TestPosWr.name[0], "SharedMemoryProgram\0");
	  TestPosWr.pid = getpid();
	  TestPosWr.latitude = 10.11223344;
	  TestPosWr.longitude = 36.11223344;
	  ALOGD(TAG, __FUNCTION__, "Write Struct Name = %s", (char*)TestPosWr.name);
	  ALOGD(TAG, __FUNCTION__, "Write Struct Pid = %d", (int)TestPosWr.pid);
	  ALOGD(TAG, __FUNCTION__, "Write Struct Latitude = %.4f", (double)TestPosWr.latitude);
	  ALOGD(TAG, __FUNCTION__, "Write Struct Longitude = %.4f", (double)TestPosWr.longitude);
	  
	  //Create a new Shm
      Shared_Memory * Shm = new Shared_Memory((char*)"shmNew1", 1024);
      
      //write in shm
      Shm->sharedMemoryWrite((void *)&TestPosWr, 0, sizeof(TestPosWr));
      
      //read from shm
      int Rdsize = Shm->sharedMemoryRead(&TestPosRd, 0, sizeof(TestPosRd));
      ALOGD(TAG, __FUNCTION__, "Read SHM Data Amount = %d", Rdsize);
      ALOGD(TAG, __FUNCTION__, "Read Struct Name = %s", TestPosRd.name);
      ALOGD(TAG, __FUNCTION__, "Read Struct Pid = %d", TestPosRd.pid);
      ALOGD(TAG, __FUNCTION__, "Read Struct Latitude = %.4f", TestPosRd.latitude);
      ALOGD(TAG, __FUNCTION__, "Read Struct Longitude = %.4f", TestPosRd.longitude);
      
      
      while(bIgnitionSet == false);
      CanInfDrv->StopCANDriver();
      sleep(2);
      delete Shm;
	  delete CanInfDrv;
      ALOGI(TAG, __FUNCTION__, "Good Bye");
      return 0;
}
