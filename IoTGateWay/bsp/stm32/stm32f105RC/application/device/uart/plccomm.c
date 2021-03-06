/*
 * plccomm.c
 *
 */
#include <string.h>
#include <stdlib.h>

#include <rtthread.h>

#include <common/smsgdef.h>
#include <device/uart/plccomm.h>
#include "application.h"
#include <common/common.h>
#include <config/appconfig.h>
#include <config/configuration.h>
#include <network/networkmanager.h>

#define UART5_MUX_SEL	GET_PIN(B,4)
#define PLC_DATA_LEN	41
#define PLC_BUFF_SIZE	256
#define PV				"PV" //PV
#define TS				"TS" 
#define TK				"TK" 
#define DEV_TYPE_TS		0 //TS 
#define DEV_TYPE_TK		1 //TK
#define PV_LEN			2
#define TB_VALUE_LEN	2	//2 bytes value len
#define FB_VALUE_LEN	4	//4 bytes value len

//Sensor Code
#define SensorCode_1	0x0001
#define SensorCode_2	0x0002
#define SensorCode_3	0x0003
#define SensorCode_4	0x0004
#define SensorCode_5	0x0005
#define SensorCode_6	0x0006
#define SensorCode_7	0x0007
#define SensorCode_8	0x0008
#define SensorCode_9	0x0009
#define SensorCode_10	0x000A
#define SensorCode_11	0x000B
#define SensorCode_12	0x000C
#define SensorCode_13	0x000D
#define SensorCode_14	0x000E
#define SensorCode_15	0x000F
#define SensorCode_16	0x0010
#define SensorCode_17	0x0011

typedef struct dustCommInfo_tag {
	rt_mq_t plcMq;
	MqData_t mqData;

	rt_device_t uport;
	rt_event_t  rx_event;

	rt_uint8_t tcpOn; //tcp connection flag
	rt_uint8_t devInfo[4];
	rt_uint8_t devType; //0: TS, 1: TK
	PlDataInfo plDataInfo;
	rt_uint8_t txBuf[PLC_BUFF_SIZE];
	rt_uint8_t rxBuf[PLC_BUFF_SIZE];
} PlcCommInfo;

static PlcCommInfo plcCommInfo;
static rt_uint8_t findStart = DISABLE;
static rt_uint16_t sensorCodeCount = 1;

//Get Tcp flag
rt_uint8_t GetTcpStatus(void)
{
	return plcCommInfo.tcpOn;
}
//Set Tcp flag
void SetTcpStatus(rt_uint8_t tcpOn)
{
	if(tcpOn != plcCommInfo.tcpOn)
	{
		plcCommInfo.tcpOn = tcpOn;
	}
}

static rt_err_t dustcomm_rx_ind(rt_device_t dev, rt_size_t size)
{
    return rt_event_send(plcCommInfo.rx_event, SMSG_RX_DATA);
}
//Make Packet following Protocol
rt_size_t MakeSensorValue(rt_uint16_t sensorCode, rt_uint8_t *pData, rt_size_t position)
{
	rt_uint32_t time = 0;
	rt_uint16_t SensorCode = 0;
	rt_uint16_t innerPress = 0;
	rt_uint16_t outerPress = 0;
	rt_uint8_t length = sizeof(time);
	rt_uint8_t 	flaotingPoint = 0;
	rt_uint8_t 	len = sizeof(sensorCode);
	rt_uint8_t *pos =  (plcCommInfo.txBuf+position);

	*pos++ = STX;

	//Time
	time = HTONL(time);
	rt_memcpy(pos,&time,length);
	pos += length;

	length = sizeof(plcCommInfo.devInfo);
	rt_memcpy(pos,plcCommInfo.devInfo,length);
	pos += length;

	if(DEV_TYPE_TS == plcCommInfo.devType)//TS
	{
		innerPress = SensorCode_7;
		outerPress = SensorCode_8;
	}
	else//TK
	{
		innerPress = SensorCode_13;
		outerPress = SensorCode_14;
	}

	//Sensor code, PV
	SensorCode = HTONS(sensorCode);
	rt_memcpy(pos, &SensorCode, len);
	pos += len;
	rt_memcpy(pos, PV, PV_LEN);
	pos += PV_LEN;

	//Sensor Value
	if((innerPress == sensorCode) || (outerPress == sensorCode))
	{
		rt_uint32_t u32Val = 0;
		rt_uint8_t length = sizeof(u32Val);

		rt_memcpy(&u32Val,pData,length);
		u32Val = HTONL(u32Val);
		rt_memcpy(pos,&u32Val,length);
		pos += length;
		flaotingPoint = 3;
		*pos++ = flaotingPoint;

	}
	else
	{
		rt_uint16_t u16Val = 0;
		rt_uint8_t 	length = sizeof(u16Val);

		rt_memcpy(&u16Val,pData,length);
		u16Val = HTONS(u16Val);
		rt_memcpy(pos,&u16Val,length);
		pos += length;
		flaotingPoint = 1;
		*pos++ = flaotingPoint;
	}

	*pos++ = ETX;

	return (pos-plcCommInfo.txBuf-position);
}
//Check Sensorcode and make packet
static rt_size_t MakeSensorPayloadData(rt_uint8_t *pData, rt_uint16_t length)
{
	rt_size_t size = 0;
	rt_uint8_t 	*begin = pData;
	rt_uint8_t 	*pos = plcCommInfo.txBuf;
	rt_uint8_t *p_base = pos;

	begin++;//Skip STX of PLC Data
	//sensor 1
	if( SensorCode_1 ==  sensorCodeCount)
	{
		size += MakeSensorValue(SensorCode_1, begin, size);
	}
	begin += TB_VALUE_LEN;

	//sensor 2
	if( SensorCode_2 ==  sensorCodeCount)
	{
		size += MakeSensorValue(SensorCode_2, begin, size);
	}
	begin += TB_VALUE_LEN;

	//sensor 3
	if( SensorCode_3 ==  sensorCodeCount)
	{
		size += MakeSensorValue(SensorCode_3, begin, size);
	}
	begin += TB_VALUE_LEN;

	//sensor 4
	if( SensorCode_4 ==  sensorCodeCount)
	{
		size += MakeSensorValue(SensorCode_4, begin, size);
	}
	begin += TB_VALUE_LEN;

	//sensor 5
	if( SensorCode_5 ==  sensorCodeCount)
	{
		size += MakeSensorValue(SensorCode_5, begin, size);
	}
	begin += TB_VALUE_LEN;

	//sensor 6
	if( SensorCode_6 ==  sensorCodeCount)
	{
		size += MakeSensorValue(SensorCode_6, begin, size);
	}
	begin += TB_VALUE_LEN;

	//if TS
	if(DEV_TYPE_TS == plcCommInfo.devType)
	{
		begin += TB_VALUE_LEN;
		begin += TB_VALUE_LEN;
		begin += TB_VALUE_LEN;
		begin += TB_VALUE_LEN;
		begin += TB_VALUE_LEN;
		begin += TB_VALUE_LEN;

		//sensor 7 //inner press
		if( SensorCode_7 ==  sensorCodeCount)
		{
			size += MakeSensorValue(SensorCode_7, begin, size);
		}
		begin += FB_VALUE_LEN;

		//sensor 8 //outer press
		if( SensorCode_8 ==  sensorCodeCount)
		{
			size += MakeSensorValue(SensorCode_8, begin, size);
		}
		begin += FB_VALUE_LEN;

		//sensor 9
		if( SensorCode_9 ==  sensorCodeCount)
		{
			size += MakeSensorValue(SensorCode_9, begin, size);
		}
		begin += TB_VALUE_LEN;
		begin += TB_VALUE_LEN;

		//sensor 10
		if( SensorCode_10 ==  sensorCodeCount)
		{
			size += MakeSensorValue(SensorCode_10, begin, size);
		}
	}
	else// TK
	{
		//sensor 7
		size += MakeSensorValue(SensorCode_7, begin, size);
		begin += TB_VALUE_LEN;

		//sensor 8
		size += MakeSensorValue(SensorCode_8, begin, size);
		begin += TB_VALUE_LEN;

		//sensor 9
		size += MakeSensorValue(SensorCode_9, begin, size);
		begin += TB_VALUE_LEN;

		//sensor 10
		size += MakeSensorValue(SensorCode_10, begin, size);
		begin += TB_VALUE_LEN;

		//sensor 11
		size += MakeSensorValue(SensorCode_11, begin, size);
		begin += TB_VALUE_LEN;

		//sensor 12
		size += MakeSensorValue(SensorCode_12, begin, size);
		begin += TB_VALUE_LEN;

		//sensor 13 //inner press
		size += MakeSensorValue(SensorCode_13, begin, size);
		begin += FB_VALUE_LEN;

		//sensor 14 //outer press
		size += MakeSensorValue(SensorCode_14, begin, size);
		begin += FB_VALUE_LEN;

		//sensor 15
		size += MakeSensorValue(SensorCode_15, begin, size);
		begin += TB_VALUE_LEN;

		//sensor 16
		size += MakeSensorValue(SensorCode_16, begin, size);
		begin += TB_VALUE_LEN;

		//sensor 17
		size += MakeSensorValue(SensorCode_17, begin, size);
	}

	pos = p_base+size;

	return (pos-plcCommInfo.txBuf);
}

rt_uint8_t CalSum(rt_uint8_t *pData, rt_uint8_t len)
{
	rt_uint8_t result = 0;
	rt_uint8_t buf[41] = {0,};
	rt_uint8_t count = 0;

	rt_memcpy(buf,pData,len);

	for(count = 0 ; count < len ; count++)
	{
		result += buf[count];
	}

	return result;
}

static rt_size_t ParserReceiveData(rt_uint8_t *p_base, rt_size_t rx_size, PlDataInfo *plDataInfo)
{
	rt_size_t consumed = 0;

	if(RT_NULL != p_base && 0 < rx_size)
	{
		rt_uint8_t *begin=p_base, *end=begin, *last=(p_base+rx_size);

		do {
			if( (STX == *end) && (findStart == DISABLE))
			{
				findStart = ENABLE;
				begin = end;
			}
			else if((findStart == ENABLE) && (PLC_DATA_LEN == (rt_size_t)(end-begin)))
			{
				findStart = DISABLE;
				rt_uint8_t sum = *(begin+PLC_DATA_LEN-1);
				if( sum == CalSum(begin,PLC_DATA_LEN-1))
				{
					plDataInfo->valid = ENABLE;
				}
			}
		} while(++end < last);
		consumed = (rt_size_t)(begin-p_base);
	}

	return consumed;
}

static void plccomm_rx_thread(void *params)
{
    PlcCommInfo *p_handle = (PlcCommInfo *)params;
	rt_uint8_t *pBuf = p_handle->rxBuf;	
	rt_size_t uRemain = 0;
	rt_uint32_t events;

	rt_pin_write(UART5_MUX_SEL, PIN_LOW);

	struct serial_configure config = RT_SERIAL_CONFIG_DEFAULT;

	config.baud_rate = BAUD_RATE_19200;
	rt_err_t err = rt_device_control(p_handle->uport, RT_DEVICE_CTRL_CONFIG, (void *)&config);
	RT_ASSERT(err == RT_EOK);

    err = rt_device_open(p_handle->uport, RT_DEVICE_OFLAG_RDONLY | RT_DEVICE_FLAG_INT_RX);
	RT_ASSERT(err == RT_EOK);

	rt_device_set_rx_indicate(p_handle->uport, dustcomm_rx_ind);
	while(1)
	{	
		if(RT_EOK == (err=rt_event_recv(p_handle->rx_event, SMSG_RX_DATA, (RT_EVENT_FLAG_OR|RT_EVENT_FLAG_CLEAR), RT_WAITING_FOREVER, &events))
			&& (SMSG_RX_DATA & events) && (ENABLE == p_handle->tcpOn))
		{
			rt_size_t ulBytes;

			if(PLC_BUFF_SIZE <= uRemain)
			{
				uRemain = 0;
			}

			do {
				if((ulBytes=rt_device_read(p_handle->uport, 0, (pBuf+uRemain), (PLC_BUFF_SIZE-uRemain))) > 0)
				{
					rt_size_t consumed = ParserReceiveData(pBuf, (uRemain += ulBytes), &p_handle->plDataInfo);

					if(consumed > 0 && ((uRemain -= consumed) > 0))
					{
						memmove(pBuf, pBuf+consumed, uRemain);
					}
				}
			} while(0 < ulBytes);


			if(ENABLE == p_handle->plDataInfo.valid)
			{
				rt_uint8_t len = 0;

				if( 0 < (len = MakeSensorPayloadData(p_handle->rxBuf,PLC_DATA_LEN)))
				{
					SendData(p_handle->txBuf,len);
				}

				sensorCodeCount++;

				if(DEV_TYPE_TS == plcCommInfo.devType && SensorCode_10 < sensorCodeCount)
				{
					sensorCodeCount = 1;
				}
				else if(DEV_TYPE_TK == plcCommInfo.devType && SensorCode_17 < sensorCodeCount)
				{
					sensorCodeCount = 1;
				}

				rt_memset(p_handle->rxBuf,'\0',PLC_BUFF_SIZE);

				p_handle->plDataInfo.valid = DISABLE;
			}
		}
	}
}

void InitPlcCommInfo(rt_uint8_t *pData)
{
	rt_uint16_t devNum = 0;
	plcCommInfo.tcpOn = DISABLE;
	plcCommInfo.plcMq = RT_NULL;

	plcCommInfo.uport = RT_NULL;
	plcCommInfo.rx_event = RT_NULL;

	rt_memset(&plcCommInfo.plDataInfo, 0, sizeof(plcCommInfo.plDataInfo));

	rt_memcpy(plcCommInfo.devInfo,pData,sizeof(devNum));
	devNum = strtoul((char *)pData+2,0,10);
	devNum = HTONS(devNum);
	rt_memcpy(plcCommInfo.devInfo+2,&devNum,sizeof(devNum));

	if(0 == rt_strncmp((char *)plcCommInfo.devInfo,TS,rt_strlen(TS)) )
	{
		plcCommInfo.devType = DEV_TYPE_TS; //TS
	}
	else
	{
		plcCommInfo.devType = DEV_TYPE_TK; //TK
	}

}	

rt_bool_t InitPlcComm(void)
{
	rt_kprintf("Initialize PlcComm.\r\n");
    PlcCommInfo *h_data = &plcCommInfo;
    rt_thread_t tid_rx;

    InitPlcCommInfo(GetDeviceInfo());

    rt_pin_mode(UART5_MUX_SEL, PIN_MODE_OUTPUT);

	h_data->plcMq = rt_mq_create("plc_mq", sizeof(MqData_t), PLCCOMM_MQ_SIZE, RT_IPC_FLAG_FIFO);
	RT_ASSERT(RT_NULL != h_data->plcMq);

    h_data->uport = rt_device_find(UART5_DEV_NAME);
    RT_ASSERT(h_data->uport != RT_NULL);	

    h_data->rx_event = rt_event_create("plccomm_rx", RT_IPC_FLAG_FIFO);
    RT_ASSERT(h_data->rx_event != RT_NULL);

    tid_rx = rt_thread_create("plccomm_rx_thread", plccomm_rx_thread, (void *)h_data, PLCCOMMM_STACK_SIZE, RT_MAIN_THREAD_PRIORITY, 20);
    RT_ASSERT(tid_rx != RT_NULL);

    /* thread start  */
    return (RT_EOK == rt_thread_startup(tid_rx));
}
