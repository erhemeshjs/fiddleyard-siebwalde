#include <State_Machine.h>
#include <Shift_Register.h>
#include <Command_Machine.h>
#include <Var_Out.h>
#include <Fiddle_Yard.h>
#include <Bridge_Ctrl.h>
#include <Fiddle_Move_Ctrl.h>
#include <Track_Move_Ctrl.h>
#include <Train_Detection.h>
#include <Drive_Train_IO.h>
#include <Fiddle_Init.h>

////////ERROR and return CODES////////
#define ERROR 0xEE	// general switch case when error
#define Busy -1
#define Finished 0

////////Bridge Errors//////
#define Bezet_Uit_Blok_6 0x01
#define Sensor_F12 0x02
#define Bezet_Uit_Blok_6_AND_Sensor_F12 0x03
#define CL_10_Heart_Sensor 0x04
#define Bridge_Open_Close_Timeout_Expired 0x05

////////Fiddle Yard move Errors//////
#define BridgeMotorContact_10 0x06
#define Bridge_10L_Contact 0x07
#define Bridge_10R_Contact 0x08
#define BridgeMotorContact_11 0x09
#define EndOffStroke_11 0x0A
#define Laatste_Spoor 0x0B
#define EndOffStroke_10 0x0C

////////State_Machine_Switch////////
#define Fiddle_Yard_Busy 255
#define Fiddle_Yard_Running 254
#define Resume 100
#define Fy_Reset 0
#define Idle 1
#define Fy_Standard_Run 2
#define Fy_Running_Standard 3
#define Fy_Collect 4
#define ERROR_Handler_Idle 9

////////Exe_Cmd_()////////
#define Nopp 0
#define Open_Bridge 1
#define Close_Bridge 2
#define Fiddle_Yard_One_Left 3
#define Fiddle_Yard_One_Right 4
#define Track_1 5
#define Track_2 6
#define Track_3 7
#define Track_4 8
#define Track_5 9
#define Track_6 10
#define Track_7 11
#define Track_8 12
#define Track_9 13
#define Track_10 14
#define Track_11 15
#define Detection_Train 16
#define Run_Fiddle_Yard 17
#define Stop_Fiddle_Yard 18
#define Stop_Fiddle_Yard_Now 19
#define Bezet_In_5B_Switch_On 20
#define Bezet_In_5B_Switch_Off 21
#define Bezet_In_6_Switch_On 22
#define Bezet_In_6_Switch_Off 23
#define Bezet_In_7_Switch_On 24
#define Bezet_In_7_Switch_Off 25
#define Restart_Previous_Command 26
#define Start_Collect 27

////////Fy_Running (standard)////////
#define Train_On_5B_Start 0
#define No_Train_On_8_Start 1
#define Drive_Train_In 2
#define Drive_Train_Out 3
#define Fy_Collect_Full 4


static unsigned int Send_Var_Out[3];

void ERROR_Code_Report(unsigned char ASL, unsigned char Code);


typedef struct
{
	unsigned char			State_Machine_Switch,						// State Machine Main Switch
							Fy_Running,// = Train_On_5B_Start,			// Switch for standard in<>out program
							Fy_Running_Old,// = 0,						// Old state when error occured
							Fy_Running_2,
							Fy_Running_Old_2,
							Fy_Init_Done,// = Off,						// Used for soft start of FY
							Execute_Command,// = Nopp,					// Used for executing commands when Idle, manipulate when resuming from error
							Execute_Command_Old,// = Nopp;				// Used when resuming
							FY_Running_Error,							// Switch used when resuming from error inside program
							Collect; // = Off;							// When trains need to be collected
							
}STATE_MACHINE_VAR;

static STATE_MACHINE_VAR ACT_ST_MCHN[2]= 	{{Fy_Reset,Train_On_5B_Start,0,0,0,Off,Nopp,Nopp,0,0},	// is 0 is BOTTOM
											 {Fy_Reset,Train_On_5B_Start,0,0,0,Off,Nopp,Nopp,0,0}};	// is 1 is TOP
											 




//////////////////////////////////////////////////////////////////////////////////////////////////////////////
void State_Machine_Update(unsigned char ASL)	//ASL = Active_Struct_Level, BOTTOM(0) or TOP(1)
{
	static char Return_Val_Routine = Busy;
	

		
	switch (ACT_ST_MCHN[ASL].State_Machine_Switch)
	{
		
		case Fy_Reset	:	Bridge_Ctrl_Reset(ASL);																		// Reset all Bridge Ctrl var
							Fiddle_Move_Ctrl_Reset(ASL);																// Reset all Fiddle Move Ctrl var
							Track_Move_Ctrl_Reset(ASL);																	// Reset all Track Move Ctrl var
							Train_Detection_Reset(ASL);																	// Reset all Train Detection var
							Drive_Train_IO_Reset(ASL);																	// Reset all Drive Train IO var
							Fiddle_Init_Reset(ASL);																		// Reset all Fiddle Init var
		
							Bezet_In_5B(ASL,On);
							Bezet_In_6(ASL,On);
							Bezet_In_7(ASL,On);
							Bezet_Weerstand(ASL,Off);
																			
							ACT_ST_MCHN[ASL].State_Machine_Switch = Idle;												// State Machine Main Switch
							ACT_ST_MCHN[ASL].Fy_Running = Train_On_5B_Start;											// Switch for standard in<>out program
							ACT_ST_MCHN[ASL].Fy_Running_Old = 0;														// Old state when error occured
							ACT_ST_MCHN[ASL].Fy_Running_2 = Train_On_5B_Start;											// Switch for Collect program
							ACT_ST_MCHN[ASL].Fy_Running_Old_2 = 0;														// Old state when error occured
							ACT_ST_MCHN[ASL].Fy_Init_Done = Off;														// Used for soft start of FY
							ACT_ST_MCHN[ASL].Execute_Command = Nopp;													// Used for executing commands when Idle, manipulate when resuming from error
							ACT_ST_MCHN[ASL].Execute_Command_Old = Nopp;												// Used when resuming
							ACT_ST_MCHN[ASL].FY_Running_Error = 0;														// Switch used when resuming from error inside program
							ACT_ST_MCHN[ASL].Collect = Off;																// When trains need to be collected
																		
							ACT_ST_MCHN[ASL].State_Machine_Switch = Idle;
							break;
		
		case Idle	:		switch (ACT_ST_MCHN[ASL].Execute_Command = Exe_Cmd_(ASL))
							{
								case	Nopp					:	break; // no command received (No Opperation Pending ;-)
								case	Open_Bridge				:	switch(Return_Val_Routine = Bridge_Open(ASL))
																	{
																		case	Finished	:	Exe_Cmd_Ret(ASL,0);
																								break;
																		case	Busy		:	break;
																		default				:	ERROR_Code_Report(ASL,Return_Val_Routine);
																								ACT_ST_MCHN[ASL].Execute_Command_Old = ACT_ST_MCHN[ASL].Execute_Command;
																								Exe_Cmd_Ret(ASL,0);
																								ACT_ST_MCHN[ASL].State_Machine_Switch = ERROR_Handler_Idle;
																								break;
																	}
																	break;
																	
								case	Close_Bridge			:	switch(Return_Val_Routine = Bridge_Close(ASL))
																	{
																		case	Finished	:	Exe_Cmd_Ret(ASL,0);
																								break;
																		case	Busy		:	break;
																		default				:	ERROR_Code_Report(ASL,Return_Val_Routine);
																								ACT_ST_MCHN[ASL].Execute_Command_Old = ACT_ST_MCHN[ASL].Execute_Command;
																								Exe_Cmd_Ret(ASL,0);
																								ACT_ST_MCHN[ASL].State_Machine_Switch = ERROR_Handler_Idle;
																								break;
																	}
																	break;
																	
								case	Fiddle_Yard_One_Left	:	switch(Return_Val_Routine = Fiddle_One_Left(ASL))
																	{
																		case	Finished	:	Exe_Cmd_Ret(ASL,0);
																								break;
																		case	Busy		:	break;
																		default				:	ERROR_Code_Report(ASL,Return_Val_Routine);
																								Exe_Cmd_Ret(ASL,0);
																								break;
																	}
																	break;
																	
								case	Fiddle_Yard_One_Right	:	switch(Return_Val_Routine = Fiddle_One_Right(ASL))
																	{
																		case	Finished	:	Exe_Cmd_Ret(ASL,0);
																								break;
																		case	Busy		:	break;
																		default				:	ERROR_Code_Report(ASL,Return_Val_Routine);
																								Exe_Cmd_Ret(ASL,0);
																								break;
																	}
																	break;
																														
								case	Track_1					:	switch(Return_Val_Routine = Track_Mover(ASL,1))
																	{	
																		case	Finished	:	Exe_Cmd_Ret(ASL,0);
																								break;
																		case	Busy		:	break;
																		default				:	ERROR_Code_Report(ASL,Return_Val_Routine);
																								Exe_Cmd_Ret(ASL,0);
																								break;
																	}
																	break;
								case	Track_2					:	switch(Return_Val_Routine = Track_Mover(ASL,2))
																	{	
																		case	Finished	:	Exe_Cmd_Ret(ASL,0);
																								break;
																		case	Busy		:	break;
																		default				:	ERROR_Code_Report(ASL,Return_Val_Routine);
																								Exe_Cmd_Ret(ASL,0);
																								break;
																	}
																	break;
								case	Track_3					:	switch(Return_Val_Routine = Track_Mover(ASL,3))
																	{	
																		case	Finished	:	Exe_Cmd_Ret(ASL,0);
																								break;
																		case	Busy		:	break;
																		default				:	ERROR_Code_Report(ASL,Return_Val_Routine);
																								Exe_Cmd_Ret(ASL,0);
																								break;
																	}
																	break;
								case	Track_4					:	switch(Return_Val_Routine = Track_Mover(ASL,4))
																	{	
																		case	Finished	:	Exe_Cmd_Ret(ASL,0);
																								break;
																		case	Busy		:	break;
																		default				:	ERROR_Code_Report(ASL,Return_Val_Routine);
																								Exe_Cmd_Ret(ASL,0);
																								break;
																	}
																	break;
								case	Track_5					:	switch(Return_Val_Routine = Track_Mover(ASL,5))
																	{	
																		case	Finished	:	Exe_Cmd_Ret(ASL,0);
																								break;
																		case	Busy		:	break;
																		default				:	ERROR_Code_Report(ASL,Return_Val_Routine);
																								Exe_Cmd_Ret(ASL,0);
																								break;
																	}
																	break;
								case	Track_6					:	switch(Return_Val_Routine = Track_Mover(ASL,6))
																	{	
																		case	Finished	:	Exe_Cmd_Ret(ASL,0);
																								break;
																		case	Busy		:	break;
																		default				:	ERROR_Code_Report(ASL,Return_Val_Routine);
																								Exe_Cmd_Ret(ASL,0);
																								break;
																	}
																	break;
								case	Track_7					:	switch(Return_Val_Routine = Track_Mover(ASL,7))
																	{	
																		case	Finished	:	Exe_Cmd_Ret(ASL,0);
																								break;
																		case	Busy		:	break;
																		default				:	ERROR_Code_Report(ASL,Return_Val_Routine);
																								Exe_Cmd_Ret(ASL,0);
																								break;
																	}
																	break;
								case	Track_8					:	switch(Return_Val_Routine = Track_Mover(ASL,8))
																	{	
																		case	Finished	:	Exe_Cmd_Ret(ASL,0);
																								break;
																		case	Busy		:	break;
																		default				:	ERROR_Code_Report(ASL,Return_Val_Routine);
																								Exe_Cmd_Ret(ASL,0);
																								break;
																	}
																	break;
								case	Track_9					:	switch(Return_Val_Routine = Track_Mover(ASL,9))
																	{	
																		case	Finished	:	Exe_Cmd_Ret(ASL,0);
																								break;
																		case	Busy		:	break;
																		default				:	ERROR_Code_Report(ASL,Return_Val_Routine);
																								Exe_Cmd_Ret(ASL,0);
																								break;
																	}
																	break;
								case	Track_10				:	switch(Return_Val_Routine = Track_Mover(ASL,10))
																	{	
																		case	Finished	:	Exe_Cmd_Ret(ASL,0);
																								break;
																		case	Busy		:	break;
																		default				:	ERROR_Code_Report(ASL,Return_Val_Routine);
																								Exe_Cmd_Ret(ASL,0);
																								break;
																	}
																	break;
								case	Track_11				:	switch(Return_Val_Routine = Track_Mover(ASL,11))
																	{	
																		case	Finished	:	Exe_Cmd_Ret(ASL,0);
																								break;
																		case	Busy		:	break;
																		default				:	ERROR_Code_Report(ASL,Return_Val_Routine);
																								Exe_Cmd_Ret(ASL,0);
																								break;
																	}
																	break;
																	
								case	Detection_Train			:	switch(Return_Val_Routine = Train_Detection(ASL))
																	{	
																		case	Finished	:	Exe_Cmd_Ret(ASL,0);
																								break;
																		case	Busy		:	break;
																		default				:	ERROR_Code_Report(ASL,Return_Val_Routine);
																								ACT_ST_MCHN[ASL].Execute_Command_Old = ACT_ST_MCHN[ASL].Execute_Command;
																								Exe_Cmd_Ret(ASL,0);
																								ACT_ST_MCHN[ASL].State_Machine_Switch = ERROR_Handler_Idle;
																								break;
																	}
																	break;
																	
								case	Run_Fiddle_Yard			:	if (ACT_ST_MCHN[ASL].Fy_Init_Done == On)
																	{
																		Fiddle_Yard_Soft_Start(ASL);
																		if (ACT_ST_MCHN[ASL].Collect == Off)
																		{
																			ACT_ST_MCHN[ASL].State_Machine_Switch = Fy_Standard_Run;
																		}
																		else if (ACT_ST_MCHN[ASL].Collect == On)
																		{
																			ACT_ST_MCHN[ASL].State_Machine_Switch = Fy_Collect;
																		}
																		
																	}
																	else if (ACT_ST_MCHN[ASL].Fy_Init_Done == Off)
																	{
																		ACT_ST_MCHN[ASL].State_Machine_Switch = Fy_Standard_Run;
																	}
																	Exe_Cmd_Ret(ASL,1);	// ASL,1 (1) means no other commands allowed
																	break;
																	
								case	Stop_Fiddle_Yard_Now	:	Fiddle_Yard_Reset(ASL);
																	ACT_ST_MCHN[ASL].State_Machine_Switch = Fy_Reset;
																	Exe_Cmd_Ret(ASL,0);
																	break;
																	
								case 	Bezet_In_5B_Switch_On	:	Bezet_In_5B(ASL,On);
																	Exe_Cmd_Ret(ASL,0);
																	break;
																	
								case 	Bezet_In_5B_Switch_Off	:	Bezet_In_5B(ASL,Off);
																	Exe_Cmd_Ret(ASL,0);
																	break;
																	
								case 	Bezet_In_6_Switch_On	:	Bezet_In_6(ASL,On);
																	Exe_Cmd_Ret(ASL,0);
																	break;
																	
								case 	Bezet_In_6_Switch_Off	:	Bezet_In_6(ASL,Off);
																	Exe_Cmd_Ret(ASL,0);
																	break;
																	
								case 	Bezet_In_7_Switch_On	:	Bezet_In_7(ASL,On);
																	Exe_Cmd_Ret(ASL,0);
																	break;
																	
								case 	Bezet_In_7_Switch_Off	:	Bezet_In_7(ASL,Off);
																	Exe_Cmd_Ret(ASL,0);
																	break;
																	
								case 	Start_Collect			:	ACT_ST_MCHN[ASL].Collect = !ACT_ST_MCHN[ASL].Collect;
																	Exe_Cmd_Ret(ASL,0);
																	ACT_ST_MCHN[ASL].Fy_Running = Train_On_5B_Start;
																	switch (ACT_ST_MCHN[ASL].Collect)
																	{
																		case	On	:	Collect_On(ASL); break;
																		case	Off	:	Collect_Off(ASL); break;
																		default		:	break;
																	}
																	break;

								default							:	break;	
							}	
							break;
			
		 case Fy_Standard_Run		:	if (Exe_Cmd_(ASL) == Stop_Fiddle_Yard_Now)										// Init Program
										{
											Fiddle_Yard_Reset(ASL);
											ACT_ST_MCHN[ASL].State_Machine_Switch = Fy_Reset;
											break;
										} 
										switch (Return_Val_Routine = Init_Fiddle_Yard(ASL,ACT_ST_MCHN[ASL].Fy_Init_Done))
										{
											case	Finished	:	ACT_ST_MCHN[ASL].Fy_Init_Done = On;
																	if (Exe_Cmd_(ASL) == Stop_Fiddle_Yard)
																	{
																		Fiddle_Yard_Stopped(ASL);
																		ACT_ST_MCHN[ASL].State_Machine_Switch = Idle;
																		break;
																	}
																	if (ACT_ST_MCHN[ASL].Collect == Off)
																	{
																		ACT_ST_MCHN[ASL].State_Machine_Switch = Fy_Running_Standard;
																	}
																	else if (ACT_ST_MCHN[ASL].Collect == On)
																	{
																		ACT_ST_MCHN[ASL].State_Machine_Switch = Fy_Collect;
																	} 
																	break;
											case	Busy		:	break;
											default				:	ERROR_Code_Report(ASL,Return_Val_Routine);
																	ACT_ST_MCHN[ASL].State_Machine_Switch = Idle;
																	break;	//TERUG LATEN SCHREEUWEN wanneer error naar boven komt!!!!!!!!!!!!!!!!!!!
										}										
										break;
							
		case Fy_Running_Standard	:	if (Exe_Cmd_(ASL) == Stop_Fiddle_Yard_Now)										// Drive in - Drive Out program
										{
											Fiddle_Yard_Stopped(ASL);
											ACT_ST_MCHN[ASL].State_Machine_Switch = Fy_Reset;
											break;
										}
										ACT_ST_MCHN[ASL].Collect = Off;
										
										switch (ACT_ST_MCHN[ASL].Fy_Running)
										{
											case	Train_On_5B_Start	:	if ((Bezet_Uit_5B(ASL)) && (Fiddle_Yard_Full(ASL,0)))
																			{
																				Train_In_Track_Out_Count_Set(0);
																				Train_On_5B(ASL);
																				Train_Drive_In_Start(ASL);
																				ACT_ST_MCHN[ASL].Fy_Running = Drive_Train_In; // Train Drive In
																				break;
																			}
																			else {ACT_ST_MCHN[ASL].Fy_Running = No_Train_On_8_Start;}
																			break;	
																			
											case	Drive_Train_In		:	switch (Return_Val_Routine = Train_Drive_In(ASL,0))
																			{
																				case	Finished	:	if (Exe_Cmd_(ASL) == Stop_Fiddle_Yard)
																										{
																											Fiddle_Yard_Stopped(ASL);
																											ACT_ST_MCHN[ASL].State_Machine_Switch = Idle;
																											break;
																										} 
																										ACT_ST_MCHN[ASL].Fy_Running = No_Train_On_8_Start; // Train out?
																										break;
																				case	Busy		:	break;
																				default				:	ERROR_Code_Report(ASL,Return_Val_Routine);
																										ACT_ST_MCHN[ASL].Fy_Running_Old = ACT_ST_MCHN[ASL].Fy_Running;
																										ACT_ST_MCHN[ASL].Fy_Running = ERROR;
																										//ACT_ST_MCHN[ASL].Fy_Init_Done = Off;
																										break;	//TERUG LATEN SCHREEUWEN wanneer error naar boven komt!!!!!!!!!!!!!!!!!!!
																			}
																			break;
																			
											case	No_Train_On_8_Start	:	if (!(Bezet_Uit_8A(ASL)) && (Train_In_Track_Out_Count_Repeater_Ret() == 0))
																			{
																				Train_On_8A(ASL);
																				ACT_ST_MCHN[ASL].Fy_Running = Drive_Train_Out; // Train Drive Out
																				Train_Drive_Out_Start(ASL);
																				break;
																			}
																			else {ACT_ST_MCHN[ASL].Fy_Running = Train_On_5B_Start;}
																			break;		
																			
											case	Drive_Train_Out		:	switch (Return_Val_Routine = Train_Drive_Out(ASL))
																			{
																				case	Finished	:	if (Exe_Cmd_(ASL) == Stop_Fiddle_Yard)
																										{
																											Fiddle_Yard_Stopped(ASL);
																											ACT_ST_MCHN[ASL].State_Machine_Switch = Idle;
																											break;
																										} 
																										ACT_ST_MCHN[ASL].Fy_Running = Train_On_5B_Start; // back to main
																										break;
																				case	Busy		:	break;
																				default				:	ERROR_Code_Report(ASL,Return_Val_Routine);
																										ACT_ST_MCHN[ASL].Fy_Running_Old = ACT_ST_MCHN[ASL].Fy_Running;
																										ACT_ST_MCHN[ASL].Fy_Running = ERROR;
																										//ACT_ST_MCHN[ASL].Fy_Init_Done = Off;
																										break;	//TERUG LATEN SCHREEUWEN wanneer error naar boven komt!!!!!!!!!!!!!!!!!!!
																			}
																			break;
																			
											case	ERROR				:	/*switch (ACT_ST_MCHN[ASL].FY_Running_Error)
																			{
																				case	0	:	Old_Track2_When_Error(ASL,Track_Nr(ASL));
																								ACT_ST_MCHN[ASL].FY_Running_Error = 1;
																								ACT_ST_MCHN[ASL].State_Machine_Switch = Idle;
																								Bezet_Weerstand(ASL, Off);
																								break;
																								
																				case	1	:	switch (Return_Val_Routine = Init_Fiddle_Yard(ASL,Off))
																								{
																									case	Finished	:	ACT_ST_MCHN[ASL].Fy_Init_Done = On;
																															if (Exe_Cmd_(ASL) == Stop_Fiddle_Yard)
																															{
																																Fiddle_Yard_Stopped(ASL);
																																ACT_ST_MCHN[ASL].State_Machine_Switch = Idle;
																																break;
																															}
																															ACT_ST_MCHN[ASL].FY_Running_Error = 2;				// bridges are open after init=ok
																															break;
																									case	Busy		:	break;
																									default				:	ERROR_Code_Report(ASL,Return_Val_Routine);
																															ACT_ST_MCHN[ASL].Fy_Running = ERROR;
																															ACT_ST_MCHN[ASL].FY_Running_Error = 1;
																															ACT_ST_MCHN[ASL].State_Machine_Switch = Idle;
																															break;
																								}
																								break;
																								
																				case	2	:	switch (Return_Val_Routine = Track_Mover(ASL,Old_Track2_When_Error_Ret(ASL)))
																								{
																									case	Finished	:	ACT_ST_MCHN[ASL].FY_Running_Error = 0; 
																															ACT_ST_MCHN[ASL].Fy_Running = ACT_ST_MCHN[ASL].Fy_Running_Old;
																															ACT_ST_MCHN[ASL].Train_Drive_Sequencer = ACT_ST_MCHN[ASL].Train_Drive_Sequencer_Old;
																															break;
																									case	Busy		:	break;
																									default				:	ERROR_Code_Report(ASL,Return_Val_Routine);
																															ACT_ST_MCHN[ASL].Fy_Running = ERROR;
																															ACT_ST_MCHN[ASL].FY_Running_Error = 1;
																															ACT_ST_MCHN[ASL].State_Machine_Switch = Idle;
																															break;
																								}
																								break;
																																																												
																				default		:	break;
																				
																			}*/
																			break;
																																						
											default						:	ACT_ST_MCHN[ASL].Fy_Running = Train_On_5B_Start;
																			break;	
										}
										break;
										
		case	Fy_Collect			:	if (Exe_Cmd_(ASL) == Stop_Fiddle_Yard_Now)							// Collect all incoming trains, leave 1 track empty
										{
											Fiddle_Yard_Stopped(ASL);
											ACT_ST_MCHN[ASL].State_Machine_Switch = Fy_Reset;
											break;
										}
										if (Exe_Cmd_(ASL) == Stop_Fiddle_Yard)
										{
											Fiddle_Yard_Stopped(ASL);
											ACT_ST_MCHN[ASL].State_Machine_Switch = Idle;
										} 
																				
										switch (ACT_ST_MCHN[ASL].Fy_Running_2)
										{
											case	Train_On_5B_Start	:	if ((Bezet_Uit_5B(ASL)) && (Fiddle_Yard_Full(ASL,0)))
																			{
																				Train_In_Track_Out_Count_Set(0);
																				Train_On_5B(ASL);
																				Train_Drive_In_Start(ASL);
																				ACT_ST_MCHN[ASL].Fy_Running_2 = Drive_Train_In; // Train Drive In
																				break;
																			}
																			else if (Fiddle_Yard_Full(ASL,0) == 0)
																			{
																				ACT_ST_MCHN[ASL].Fy_Running_2 = Fy_Collect_Full;
																				Collect_Finished_Fy_Full(ASL);																				
																			}
																			break;	
																			
											case	Drive_Train_In		:	switch (Return_Val_Routine = Train_Drive_In(ASL,0))
																			{
																				case	Finished	:	if (Exe_Cmd_(ASL) == Stop_Fiddle_Yard)
																										{
																											Fiddle_Yard_Stopped(ASL);
																											ACT_ST_MCHN[ASL].State_Machine_Switch = Idle;
																											break;
																										} 
																										ACT_ST_MCHN[ASL].Fy_Running_2 = Train_On_5B_Start; // Train out?
																										break;
																				case	Busy		:	break;
																				default				:	ERROR_Code_Report(ASL,Return_Val_Routine);
																										ACT_ST_MCHN[ASL].Fy_Running_Old_2 = ACT_ST_MCHN[ASL].Fy_Running_2;
																										ACT_ST_MCHN[ASL].Fy_Running_2 = ERROR;
																										break;	//TERUG LATEN SCHREEUWEN wanneer error naar boven komt!!!!!!!!!!!!!!!!!!!
																			}
																			break;
																			
											case	Fy_Collect_Full		:	switch (Return_Val_Routine = Train_Drive_In(ASL,1))
																			{
																				case	Finished	:	if (Exe_Cmd_(ASL) == Stop_Fiddle_Yard)
																										{
																											Fiddle_Yard_Stopped(ASL);
																											ACT_ST_MCHN[ASL].State_Machine_Switch = Idle;
																											break;
																										} 
																										ACT_ST_MCHN[ASL].State_Machine_Switch = Idle;
																										ACT_ST_MCHN[ASL].Fy_Running_2 = Train_On_5B_Start;
																										ACT_ST_MCHN[ASL].Collect = Off;
																										break;
																				case	Busy		:	break;
																				default				:	ERROR_Code_Report(ASL,Return_Val_Routine);
																										ACT_ST_MCHN[ASL].Fy_Running_Old_2 = ACT_ST_MCHN[ASL].Fy_Running_2;
																										ACT_ST_MCHN[ASL].Fy_Running_2 = ERROR;
																										break;	//TERUG LATEN SCHREEUWEN wanneer error naar boven komt!!!!!!!!!!!!!!!!!!!
																			}
																			break;
																			
											case	ERROR				:	/*switch (ACT_ST_MCHN[ASL].FY_Running_Error)
																			{
																				case	0	:	Old_Track2_When_Error(ASL,Track_Nr(ASL));
																								ACT_ST_MCHN[ASL].FY_Running_Error = 1;
																								ACT_ST_MCHN[ASL].State_Machine_Switch = Idle;
																								Bezet_Weerstand(ASL, Off);
																								break;
																								
																				case	1	:	switch (Return_Val_Routine = Init_Fiddle_Yard(ASL,Off))
																								{
																									case	Finished	:	ACT_ST_MCHN[ASL].Fy_Init_Done = On;
																															if (Exe_Cmd_(ASL) == Stop_Fiddle_Yard)
																															{
																																Fiddle_Yard_Stopped(ASL);
																																ACT_ST_MCHN[ASL].State_Machine_Switch = Idle;
																																break;
																															}
																															ACT_ST_MCHN[ASL].FY_Running_Error = 2;				// bridges are open after init=ok
																															break;
																									case	Busy		:	break;
																									default				:	ERROR_Code_Report(ASL,Return_Val_Routine);
																															ACT_ST_MCHN[ASL].Fy_Running_2 = ERROR;
																															ACT_ST_MCHN[ASL].FY_Running_Error = 1;
																															ACT_ST_MCHN[ASL].State_Machine_Switch = Idle;
																															break;
																								}
																								break;
																								
																				case	2	:	switch (Return_Val_Routine = Track_Mover(ASL,Old_Track2_When_Error_Ret(ASL)))
																								{
																									case	Finished	:	ACT_ST_MCHN[ASL].FY_Running_Error = 0; 
																															ACT_ST_MCHN[ASL].Fy_Running_2 = ACT_ST_MCHN[ASL].Fy_Running_Old_2;
																															ACT_ST_MCHN[ASL].Train_Drive_Sequencer = ACT_ST_MCHN[ASL].Train_Drive_Sequencer_Old;
																															break;
																									case	Busy		:	break;
																									default				:	ERROR_Code_Report(ASL,Return_Val_Routine);
																															ACT_ST_MCHN[ASL].Fy_Running_2 = ERROR;
																															ACT_ST_MCHN[ASL].FY_Running_Error = 1;
																															ACT_ST_MCHN[ASL].State_Machine_Switch = Idle;
																															break;
																								}
																								break;
																																																												
																				default		:	break;
																				
																			}*/
																			break;
										}
										break;
										
		case	ERROR_Handler_Idle	:	if (Exe_Cmd_(ASL) == Restart_Previous_Command)	// for direct commands
										{
											Exe_Cmd_Resume(ASL,ACT_ST_MCHN[ASL].Execute_Command_Old);
											ACT_ST_MCHN[ASL].State_Machine_Switch = Idle;
										}
										if (Exe_Cmd_(ASL) == Stop_Fiddle_Yard_Now)
										{
											Fiddle_Yard_Reset(ASL);
											ACT_ST_MCHN[ASL].State_Machine_Switch = Fy_Reset;
											break;
										}
										break;
		
		default						:	break;
		
	}
	
	Var_Out_Programm(ASL);
		
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////



void ERROR_Code_Report(unsigned char ASL, unsigned char Code)
{
	switch (Code)
	{
		////////Bridge Errors//////
		case	Bezet_Uit_Blok_6					:	Bezet_Uit_Blok_6_Send(ASL);						break;	//Bridge Error Code
		case	Sensor_F12							:	Sensor_F12_Send(ASL);							break;	//Bridge Error Code
		case	Bezet_Uit_Blok_6_AND_Sensor_F12		:	Bezet_Uit_Blok_6_AND_Sensor_F12_Send(ASL);		break;	//Bridge Error Code
		case	CL_10_Heart_Sensor					:	CL_10_Heart_Sensor_Send(ASL);					break;	//Bridge Error Code
		case	Bridge_Open_Close_Timeout_Expired	:	Bridge_Open_Close_Timeout_Expired_Send(ASL);	break;	//Bridge Error Code
		
		////////Fiddle Yard move Errors//////
		case	BridgeMotorContact_10				:	BridgeMotorContact_10_Send(ASL);				break;
		case	Bridge_10L_Contact					:	Bridge_10L_Contact_Send(ASL);					break;
		case	Bridge_10R_Contact					:	Bridge_10R_Contact_Send(ASL);					break;
		case	BridgeMotorContact_11				:	BridgeMotorContact_11_Send(ASL);				break;
		case	EndOffStroke_11						:	EndOffStroke_11_Send(ASL);						break;
		case	Laatste_Spoor						:	Laatste_Spoor_Send(ASL);						break;
		case	EndOffStroke_10						:	EndOffStroke_10_Send(ASL);						break;
		default										:	Universal_Error_Send(ASL);						break;
	}
}