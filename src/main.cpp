#include <iostream>
#include <memory>
#include <errno.h>
#include "logger/lib_logger.h"
#include "sharedMem/lib_sharedMem.h"
#include "signal/lib_signal.h"
#include "main_app/InOutApp.h"
#include "main_app/j1939.h"

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

//****************************************************************************//
//Definitions of Engine Source Addresses
//****************************************************************************//
#define PROP_SA  0x21
#define ENG_SA   0x00
#define TRANS_SA 0x03
#define BRAKE_SA 0x0B

//****************************************************************************//
//Definition of used received PGNs
//****************************************************************************//
#define EEC1_PGN		0xF004 //SA : 0x00 Engine : Motorisation
#define CCVS_PGN		0xFEF1
#define ENG_TEMP1_PGN	0xFEEE
#define ENG_FLD_PGN		0xFEEF

//****************************************************************************//
//J1939 Parameters
//****************************************************************************//
unsigned short usEngineSpeed = 0; //Engine RPM in Revolution Per Minute
unsigned short usVehicleSpeed = 0;//Vehicle Speed in Km/h
signed char    scCoolTemp = 0;//Engine Coolant Temperature
unsigned short  usOilPres = 0;//Engine Oil Pressure
//****************************************************************************//
//J1939 Received Messages
//****************************************************************************//

/* Simple Testing Method using Linux can-utils command line cansend for each message
 * Engine Speed (2500 RPM)    : cansend vcan0 06F00400#FF.FF.FF.20.4E.FF.FF.FF
 * Vehicle Speed (130 Km/h)   : cansend vcan0 06FEF100#FF.00.82.FF.FF.FF.FF.FF
 * Engine Cool. Temp (-25°C)  : cansend vcan0 06FEEE00#0F.FF.FF.FF.FF.FF.FF.FF
 * Engine Oil Press. (500mbar): cansend vcan0 06FEEF00#FF.FF.FF.7D.FF.FF.FF.FF
 */

static const struct J1939_eRxDataInfo CstP1939_iRxDataDef[4] =
{
 {EEC1_PGN,		 ENG_SA,  3, 2 ,  1, 8,  0,  0,   12000  ,  &usEngineSpeed},
 {CCVS_PGN,		 ENG_SA,  1, 2 ,  1, 256,0,  0,   250    ,  &usVehicleSpeed},
 {ENG_TEMP1_PGN, ENG_SA,  0, 1 ,  1, 1,  -40,-40, 210    ,  &scCoolTemp},
 {ENG_FLD_PGN,	 ENG_SA,  3, 1 ,  4, 1,  0,  0,   1000   ,  &usOilPres},
};

unsigned long ulBuildCanId(unsigned char ucSA, unsigned short usPGN)
{
	unsigned char ucDefPrio = 6;
	return static_cast<unsigned long>(static_cast<unsigned long>(ucDefPrio << 24) +
									  static_cast<unsigned long>(usPGN << 8) +
									  static_cast<unsigned long>(ucSA));
}


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
      //initialize a Signal Catcher for SIGINT
      std::shared_ptr<SignalApi> InterruptSignal(new SignalApi(SIGINT,SignalHandler));
      

      std::shared_ptr<InOutApp> InOutBank1(new InOutApp());

	  InOutBank1->SetOutputOn(InOutBank1->CeEnum_ParkOut);
	  
	  
      //Initialize CAN Interface
      	//struct can_filter J1939Filters[size]={0};
		struct can_filter* J1939Filters = new can_filter[4];
		for(unsigned int j = 0; j < 4; j++) {
			J1939Filters[j].can_id   = ulBuildCanId(CstP1939_iRxDataDef[j].ucSA,
			                                        CstP1939_iRxDataDef[j].usPGN);
			J1939Filters[j].can_mask = CAN_EFF_MASK; //J1939 use CAN2.0B only extended frames
		}
      
      std::shared_ptr<J1939Layer> J1939LayerApp(new J1939Layer("CANFIFO-VCan0", "vcan0", &CstP1939_iRxDataDef[0], 4, J1939Filters));
      TxCanMsg.can_id = 0x0CFEF100;
      TxCanMsg.can_dlc = 8;
      strcpy((char*)TxCanMsg.data, "ABCDEFGH");
      J1939LayerApp->SendJ1939Msg(TxCanMsg);
      
	  //Shared memory testing Structure
	  struct ProgramPosition TestPosWr, TestPosRd;
	  memset(TestPosWr.name, 0, 100);
	  strcpy(TestPosWr.name ,"SharedMemoryProgram\0");
	  TestPosWr.pid = getpid();
	  TestPosWr.latitude = 10.11223344;
	  TestPosWr.longitude = 36.11223344;
	  ALOGD(TAG, __FUNCTION__, "Write Struct Name = %s", TestPosWr.name);
	  ALOGD(TAG, __FUNCTION__, "Write Struct Pid = %d", TestPosWr.pid);
	  ALOGD(TAG, __FUNCTION__, "Write Struct Latitude = %.4f", TestPosWr.latitude);
	  ALOGD(TAG, __FUNCTION__, "Write Struct Longitude = %.4f", TestPosWr.longitude);
	  
	  //Create a new Shm
      std::shared_ptr<Shared_Memory> Shm(new Shared_Memory("shmNew1", 1024));
      //write in shm
      Shm->sharedMemoryWrite((void *)&TestPosWr, 0, sizeof(TestPosWr));
      
      //read from shm
      int Rdsize = Shm->sharedMemoryRead(&TestPosRd, 0, sizeof(TestPosRd));
      ALOGD(TAG, __FUNCTION__, "Read SHM Data Amount = %d", Rdsize);
      ALOGD(TAG, __FUNCTION__, "Read Struct Name = %s", TestPosRd.name);
      ALOGD(TAG, __FUNCTION__, "Read Struct Pid = %d", TestPosRd.pid);
      ALOGD(TAG, __FUNCTION__, "Read Struct Latitude = %.4f", TestPosRd.latitude);
      ALOGD(TAG, __FUNCTION__, "Read Struct Longitude = %.4f", TestPosRd.longitude);
      struct timespec MainDataDisplayTimer;
      MainDataDisplayTimer.tv_sec = 3;
	  MainDataDisplayTimer.tv_nsec = 0; 
      while(bIgnitionSet == false){
		  ALOGD(TAG, __FUNCTION__, "usEngineSpeed  = %d", usEngineSpeed);
		  ALOGD(TAG, __FUNCTION__, "usVehicleSpeed = %d", usVehicleSpeed);
		  ALOGD(TAG, __FUNCTION__, "scCoolTemp     = %d", scCoolTemp);
		  ALOGD(TAG, __FUNCTION__, "ucOilPres      = %d", usOilPres);
		  nanosleep(&MainDataDisplayTimer, NULL);
	  }
      J1939LayerApp->ForceStopCAN();
      sleep(2);
      ALOGI(TAG, __FUNCTION__, "Good Bye");
      return 0;
}
