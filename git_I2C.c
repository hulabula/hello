#include"SFP.H"
#include"EEPROM.H"

//Code  ( SMB0CN & 0xF0 )
#define  SMB_RADD       0X20        // 接收到地址，请求确认
#define  SMB_RSTO       0X10        // 检测到停止条件
#define  SMB_RDB        0X00        // 接收到数据字节
#define  SMB_TDB        0X40        // 字节已发送
#define  SMB_TSTO       0X50        // 检测到停止条件

#define  I2CSET_STA    5
#define  I2CSET_STO    5
#define  I2CSET_ACK    1
#define  I2CSET_SI     0

#define  Module_Address  0xA0        // Mudule 7bits I2C Slave Address


extern bit PassEntry;
extern bit RWORDADD;              // I2C 准备接收读写地址指示位             // CRC 使能
extern bit I2C_A0A2;
extern bit I2CRW;                // I2C 读写标志  0 写，1 读
extern bit RxPowerSel;
extern bit A0A2_OPEN;
extern bit UserPass_EN;
extern unsigned char PassLevel;

extern unsigned char xdata A2H_RAM[128];
extern unsigned char xdata RealData[32];

extern unsigned char WriteNumber;     // I2C写入字节数
extern unsigned char I2C_Pointer[2];      // 当前地址         
extern unsigned char xdata I2C_WData[Module_Page];   // 主机写操作的数据
extern unsigned char SMB_RCount;  // 模块接收字节计数

extern unsigned char I2CBUFFER[2];    // I2C 发送缓冲
extern IntraDebug xdata DebugData;
//extern unsigned char xdata HostCtrl;
//extern unsigned int xdata MON_RxPower[2];
extern unsigned char xdata FlashKey[2];


static unsigned char xdata Fbuff[FLASHPAGESIZE];

unsigned int SearchTable(unsigned char t)
{
	code unsigned int TableAddr[6] = {F_T0,F_T0,F_T2,F_T3,F_T4,F_T5};   // T0和T1是同一个表
	if(t<6)
		return TableAddr[t];
	else
		return 0xFF00;
}

void I2C_Write(void)
{
	unsigned int i;
	unsigned int W_addr;
	unsigned int waddr=0xFF00;
	unsigned char xdata *wtbuf;
	
	if(SMB_RCount<1)
		return;
	
	if(I2C_A0A2)
	{
		if(I2C_Pointer[1]>0x7F)
		{
			if(PassLevel==PASS_LEVEL3)
				waddr = SearchTable(RealData[F_L_Table_Sel-F_L_OFF]);
			else if(PassLevel>PASS_LEVEL0)
			{
				waddr = F_T0;
			}
			else
				return;
			goto _SaveToFlash;
		}
		if(I2C_Pointer[1]<F_L_Temp)
		{
			if(PassLevel<PASS_LEVEL2)
			{	
				return;		  
			}
			waddr = F_L;
			goto _SaveToFlash;
		}	
		for(i=0;i<SMB_RCount;i++)
			{
				switch(I2C_Pointer[1])
				{
//					case F_L_CTRL:
//						HostCtrl = I2C_WData[i]&0x40;
//						break;
					case F_L_Pass0:
					case F_L_Pass1:
					case F_L_Pass2:
					case F_L_Pass3:
						RealData[I2C_Pointer[1]-F_L_OFF] = I2C_WData[i];
						PassEntry = 1;
						break;
					case F_L_Table_Sel:
						RealData[I2C_Pointer[1]-F_L_OFF] = I2C_WData[i]; 
						break;
					default:
						break;
				}
				I2C_Pointer[1]++;
			}
		if(PassEntry)
		{
			PassEntry = 0;
				if( (RealData[F_L_Pass0-F_L_OFF]=='A')
					&&(RealData[F_L_Pass1-F_L_OFF]=='n')
					&&(RealData[F_L_Pass2-F_L_OFF]=='d')
					&&(RealData[F_L_Pass3-F_L_OFF]=='y') ) 	
				{
					PassLevel = PASS_LEVEL3;
				}
				else if(A0A2_OPEN)
				{
					PassLevel = PASS_LEVEL2;
				}
				else if( (RealData[F_L_Pass0-F_L_OFF]==FLASH_ByteRead(F_T3+F_T3_A0A2Pass))
							 &&(RealData[F_L_Pass1-F_L_OFF]==FLASH_ByteRead(F_T3+F_T3_A0A2Pass+1))
							 &&(RealData[F_L_Pass2-F_L_OFF]==FLASH_ByteRead(F_T3+F_T3_A0A2Pass+2))
							 &&(RealData[F_L_Pass3-F_L_OFF]==FLASH_ByteRead(F_T3+F_T3_A0A2Pass+3)) )
				{
					PassLevel = PASS_LEVEL2;
				}
				else if(UserPass_EN)
				{
					if( (RealData[F_L_Pass0-F_L_OFF]==FLASH_ByteRead(F_T3+F_T3_UsePass))
						&&(RealData[F_L_Pass1-F_L_OFF]==FLASH_ByteRead(F_T3+F_T3_UsePass+1))
						&&(RealData[F_L_Pass2-F_L_OFF]==FLASH_ByteRead(F_T3+F_T3_UsePass+2))
						&&(RealData[F_L_Pass3-F_L_OFF]==FLASH_ByteRead(F_T3+F_T3_UsePass+3)) )
						PassLevel = PASS_LEVEL1;
					else
						PassLevel = PASS_LEVEL0;
				}
				else
				{
					PassLevel = PASS_LEVEL1;
				}
		}
		return;	
	}	    
	// A0
	else
	{
		if(PassLevel<PASS_LEVEL2)
		{
			return;
		}
		waddr = (I2C_Pointer[0]&0x80)? F_A0H : F_A0L;
	}
	
_SaveToFlash:  
	
	if(waddr>Flash_AddressMAX)
			return;
	
	if((waddr==F_T3)&&(I2C_Pointer[1]>=F_T3_RAM))
	{
		for(i=0;i<SMB_RCount;i++)
		{
			if(I2C_Pointer[1]<F_T3_RO)
				*(((unsigned char*)&DebugData.CMD)+(I2C_Pointer[1]-F_T3_RAM)) = I2C_WData[i];
			I2C_Pointer[1]++;
		}
	}
	else
	{
		FlashKey[1] = FLASH_KEYB;
		waddr += I2C_Pointer[I2C_A0A2]&0x7F;
		
		W_addr = waddr&0x01FF;
		if(W_addr>=FLASHPAGESIZE)
			return;
			
		SMB0CF = 0x00;   // Disable I2C
		// 保存数据
		W_addr = waddr&Flash_AddressMASK;
		if(W_addr==F_T0)
		{
			if(F_T0Flag_USE==FLASH_ByteRead(F_T0Flag_A))   // UserEEPOM_A is using, Erase B
				W_addr = F_T0B;
			wtbuf = A2H_RAM;
		}
		else
		{
			for(i=0;i<FLASHPAGESIZE;i++)
			{
				Fbuff[i] = FLASH_ByteRead(W_addr+i);	
			}
			wtbuf = Fbuff;
		}
		// 更新数据
		waddr = waddr&0x01FF;
		for(i=0;i<SMB_RCount;i++)
		{
			wtbuf[waddr+i] = I2C_WData[i];	
		}
		IE_EA = 0;
		IE_EA = 0;
		// 页擦除
		FLKEY  = FlashKey[0];                   // Key Sequence 1
		FLKEY  = FlashKey[1];                   // Key Sequence 2
		PSCTL |= 0x03;                   // PSWE = 1; PSEE = 1
		*((char xdata *)W_addr) = 0;                     // initiate page erase
		PSCTL &= ~0x03;                  // PSWE = 0; PSEE = 0
		
		// 写入Flash
		for(i=0;i<FLASHPAGESIZE;i++)
		{
			//FLASH_ByteWrite((W_addr+i),buff[i]);	
			FLKEY  = FlashKey[0];                      // Key Sequence 1
			FLKEY  = FlashKey[1];                      // Key Sequence 2
			PSCTL |= 0x01;                      // PSWE = 1
			*((char xdata *)(W_addr+i)) = wtbuf[i];                     // write the byte
			PSCTL &= ~0x01;                     // PSWE = 0
		}
		if(W_addr==F_T0)     // 如果写入的是T0_A，则将A设为Use，B设为Idel
		{
			FLKEY  = FlashKey[0];                      // Key Sequence 1
			FLKEY  = FlashKey[1];                      // Key Sequence 2
			PSCTL |= 0x01;                      // PSWE = 1
			*((char xdata *)(F_T0Flag_A)) = F_T0Flag_USE;                     // write the byte
			PSCTL &= ~0x01;                     // PSWE = 0
			FLKEY  = FlashKey[0];                      // Key Sequence 1
			FLKEY  = FlashKey[1];                      // Key Sequence 2
			PSCTL |= 0x01;                      // PSWE = 1
			*((char xdata *)(F_T0Flag_B)) = F_T0Flag_IDEL;                     // write the byte
			PSCTL &= ~0x01;                     // PSWE = 0
		}
		else if(W_addr==F_T0B)  // 如果写入的是T0_B，则将B设为Use，A设为Idel
		{
			FLKEY  = FlashKey[0];                      // Key Sequence 1
			FLKEY  = FlashKey[1];                      // Key Sequence 2
			PSCTL |= 0x01;                      // PSWE = 1
			*((char xdata *)(F_T0Flag_B)) = F_T0Flag_USE;                     // write the byte
			PSCTL &= ~0x01;                     // PSWE = 0
			FLKEY  = FlashKey[0];                      // Key Sequence 1
			FLKEY  = FlashKey[1];                      // Key Sequence 2
			PSCTL |= 0x01;                      // PSWE = 1
			*((char xdata *)(F_T0Flag_A)) = F_T0Flag_IDEL;                     // write the byte
			PSCTL &= ~0x01;                     // PSWE = 0
		}
		FlashKey[0] = 0;
		FlashKey[1] = 0;
		IE_EA = 1;
		SMB0CF = 0x90;   // Enable I2C
		I2C_Pointer[(unsigned char)I2C_A0A2] += SMB_RCount;
	} 		
}	
 
 
unsigned char ReadEEP(unsigned char address)
{
	unsigned char OutD=0;
	unsigned char addU8;
	unsigned int raddr;

	addU8 = address;
	// A2
	if(I2C_A0A2)
	{
		if(addU8<0x80)
		{

/*			if(addU8==F_L_RxPower)   // 双字节完整性
			{
				if(RxPowerSel)
					*((unsigned int*)&RealData[F_L_RxPower-F_L_OFF]) = MON_RxPower[1];
				else
					*((unsigned int*)&RealData[F_L_RxPower-F_L_OFF]) = MON_RxPower[0];
			}
*/
			if(addU8<F_L_Temp)	          //<96
			{
				raddr = F_L+addU8;
				OutD = FLASH_ByteRead(raddr);
			}
			else if(addU8<F_L_Pass0)	  //<123
				OutD = RealData[addU8-F_L_OFF];
			else if(addU8==F_L_Table_Sel) //=127
				OutD = RealData[F_L_Table_Sel-F_L_OFF];
		}
		else
		{
			addU8 &= 0x7F;
			if(PassLevel == PASS_LEVEL3)										//密码校验失败，就指向 USER EEPROM,没有使用的空间
				raddr = SearchTable(RealData[F_L_Table_Sel-F_L_OFF]);
			else if(PassLevel>PASS_LEVEL0)
				raddr = F_T0;						//F_A2H
			else
				return 0x00;
			if(raddr>Flash_AddressMAX)
				return 0x00;
			
			if(raddr==F_T0)
			{
				OutD = A2H_RAM[addU8];
			}
			else if((raddr==F_T3)&&(addU8>=F_T3_I2C_CMD))
			{
				addU8 -= F_T3_I2C_CMD;
				OutD = *(((unsigned char*)&DebugData.CMD)+addU8);
			}
			else
			{		
				raddr = raddr+addU8;
				OutD = FLASH_ByteRead(raddr);
			}			
		}		
	}
	//A0
	else
	{
		if(addU8<0x80)
		{
			raddr = F_A0L+addU8;
		}
		else
		{
			raddr = addU8-0x80+F_A0H;
		}
		OutD = FLASH_ByteRead(raddr);
	}
	return OutD;
} 


void SMBus_ISR (void) interrupt 7
{ 
	unsigned char buf;
    
	switch (SMB0CN0 & 0xF0)                    // 读SMBus状态向量
	{
		// Slave Receiver: Start+Address received 
		case  SMB_RADD:
			buf = SMB0DAT;
			if((buf&0xfc)==Module_Address)       // A0 A1 A2 A3   
			{
				I2C_A0A2 = buf&0x02;             // Check A0 ? A2
				if(buf&0x01)   // Read
				{
					SMB0DAT = I2CBUFFER[I2C_A0A2];
					SMB0CN0 = 1<<I2CSET_ACK;
					I2CRW = 1;
					I2C_Pointer[I2C_A0A2]++;
					I2CBUFFER[I2C_A0A2] = ReadEEP(I2C_Pointer[I2C_A0A2]);
				}
				else           // Write
				{
					SMB0CN0 = 1<<I2CSET_ACK;
					I2CRW = 0;
					SMB_RCount = 0;
					RWORDADD = 1;                 // 准备接收数据地址
				}	
			}
			else                                // If received slave address does not
			{                                   // match,
				SMB0CN0 = 0;
			}
			break;

		// Slave Receiver: Data received
		case  SMB_RDB:
			if(!I2CRW)
			{
				buf = SMB0DAT;
				SMB0CN0 = 0x02;
				if(!RWORDADD)
				{
					if( SMB_RCount < Module_Page ) 
					{
						I2C_WData[SMB_RCount++] = buf; 
					}                          
				}  
				else                    // 收到地址 
				{
					RWORDADD = 0;                      // 清接收地址标志
					I2C_Pointer[I2C_A0A2] = buf;        	
					I2CBUFFER[I2C_A0A2] = ReadEEP(I2C_Pointer[I2C_A0A2]);
				}
			}
			else
				SMB0CN0 = 0;
			break;

		// Slave Receiver: Stop received
		case  SMB_RSTO:
			SMB0CN0_STO = 0;  
			SMB0CN0_SI = 0;
			FlashKey[0] = FLASH_KEYA;
			if(!(I2CRW|RWORDADD))
			{	
				I2C_Write();
				I2CBUFFER[I2C_A0A2] = ReadEEP(I2C_Pointer[I2C_A0A2]);
			}
			SMB_RCount = 0;  
			break;

		// Slave Transmitter: Data byte transmitted
		case  SMB_TDB:
			if(SMB0CN0_ACK)                         // 主机收到数据
			{
				SMB0DAT = I2CBUFFER[I2C_A0A2];    
				SMB0CN0_SI = 0;
				I2C_Pointer[(unsigned char)I2C_A0A2]++;
				I2CBUFFER[I2C_A0A2] = ReadEEP(I2C_Pointer[I2C_A0A2]);
			}
			else
				SMB0CN0_SI = 0;  
			break;

		// Slave Transmitter: Arbitration lost, Stop detected
		case  SMB_TSTO:
			SMB0CN0_STO = 0 ;
			SMB0CN0_SI = 0;
			SMB_RCount = 0;
			break;                          

		// Default: all other cases undefined
		default:
			SMB0CN0_SI = 0;
			SMB0CF = 0x00;
 			SMB0CF = 0x90;
			break;
   }
}

