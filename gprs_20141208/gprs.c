#define GPRS_SIGN_WIDTH 1655  //定义码元宽度
#define GPRS_DATA_MAX 10
#define GPRS_RETRY_MAX 5
#define ulong unsigned long
#define uiint unsigned int

uint send_data_index = 0;
uint receive_data_index = 0;
ulong send_data[GPRS_DATA_MAX];
ulong recive_data[GPRS_DATA_MAX];
byte repeat_sign = 1;
int GPRS_SDA = 2;
int GPRS_SCL = 3;
int curr_state; 
int last_state;
ulong last_record_time = 0;  //上一次记录的时间
ulong update_gap_time = 0;   //状态改变累计时间
long limit_time = 0;         //状态改变限制时间
byte  gprs_resign_data = 0;
byte gprs_rx_retry = 0;
byte gprs_receive_resign_data =0;
ulong single_tx_data;
ulong single_rx_data;
uint single_data_index = 0;
uint single_data_re_index = 0;

enum gprs_state
{
	GPRS_IDLE,                 			 //SDA设OUTPUT,SCL设IINPUT.如果检测send_data_index大于0,发送数据：SDA拉低，limit_time为5T。
								//等待SCL变为低电平
								 //如果检测到SDA被拉低,接收数据:SCL设为输出模式，SCL拉低。等待SDA变成低电平.
	GPRS_WAIT_SCL_TO_LOW,		 //如果检测到SCL被拉低，等待1T时间(update_gap_time>1T),将SDA拉高，等待SCL变为高电平
	                           			     	//否则如果limit_time==0，终止发送数据。
	GPRS_WAIT_SCL_TO_HIGH,      	//if(SCL==HIGH)进入发送重发数据状态。否则终止发送数据
	SEND_REPEAT_SIGN,          		//SCL设为输出,根据repeat_sign状态决定SDA状态 等待1T时间,将SCL拉高
    	SEND_DATA_SCL_HIGH,          	//SCL设为高电平，等待1T时间，进入发送数据状态
    	SEND_DATA_SCL_LOW,                  			//SCL拉低，根据sinle_tx_data对应位数的数据决定SDA发送数据状态,等待1T时间，
	                           				 //若数据未发完，进入SEND_SET_SCL_HIGH状态否则等待sda ack
	WAIT_SDA_ACK_LOW,          	 //若检测到SDA为低,进入WAIT_SDA_ACK_HIGH状态，如果limit_time = 0,终止程序
	WAIT_SDA_ACK_HIGH,			//如检测到SDA为高,数据发送成功，进入IDLE状态等待下一个发送数据。如果limit_time = 0,终止程序
	
	GPRS_RE_WAIT_SDA_TO_LOW,		
	GPRS_RE_WAIT_SDA_TO_HIGH,
	GPRS_RECEIVE_REPEAT_SIGN,
	GPRS_RE_WAIT_SDA_HIGH,
	GPRS_RE_WAIT_RETRY_SCL_LOW,
	GPRS_RE_WAIT_RETRY_SCL_HIGH,
	GPRS_RE_WAIT_SCL_LOW,
	GPRS_RE_WAIT_SCL_HIGH,
	GPRS_RE_SEND_ACK_LOW,
	GPRS_RE_SEND_ACK_HIGH
}

/*
 *要发送四个cid数据，因此要用一个数组存储这几个cid数据
 *
 */
int parameter_list[SEND_DATA_MAX];

byte data_index;
void send_cid(byte cmd_data,uint data) //需计算校验和并且合并控制命令数据和发送数据
{
	/*  
	 * |----|----------------|----|-------|
	 *
	 * |----| ---->cmd   4bit (28)
	 *           |----------------| ---->data 16bit (12)
	 *                                             |----| ---->校验和 4bit (8)
	 * */

    	byte checksum = 0;
	byte temp_data;
	checksum = (cmd_data + data&0x0f
		                + (data >> 4)&0x0f
						+ (data >> 8)&0x0f
						+ (data >> 12)&0x0f)&0x0f;

	bitWrite(temp_data,0,bitRead(checksum,3));
	bitWrite(temp_data,1,bitRead(checksum,2));
	bitWrite(temp_data,2,bitRead(checksum,1));
	bitWrite(temp_data,3,bitRead(checksum,0));

	send_data[send_data_index] = (ulong)cmd_data<<28 + (ulong)data<<12 + temp<<8;
	send_data_index++;
}

byte retry_cnt = 0;
void prepare_data(void)
{
	int i;
	if(retry_cnt > 0)
	{
		retry_cnt--;
	}
	else if(retry_cnt == 0)
	{
		retry_cnt = GPRS_RETRY_MAX;
		single_tx_data = send_data[0];
		for(i =0,i<GPRS_DATA_MAX,i++)
		{
			send_data[i] = send_data[i+1];
		}
		send_data_index--;	
	}
}

void update_time_state(void)
{
	ulong gap_time;
	ulong curr_time = micros();
	gap_time = curr_time - last_record_time; //每次循环间隔时间
    
	if((curr_time - last_record_time) < 0 )
	{
		gap_time = ulong(-1) - last_record_time + curr_time;
	}
	
	if(limit_time > 0)
	{
		limit_time = limit_time - gap_time;  //状态改变时间限制
	}
	else
	{
		limit_time = 0;
	}

	last_record_time = curr_time;
	update_gap_time = update_gap_time + gap_time; //状态改变后
}

void send_abort(void)
{
	curr_state = GPRS_IDLE;
}

void receive_abort(void)
{
	curr_state = GPRS_IDLE;
}
void setup(void)
{
	curr_state = GPRS_IDLE; 
   	last_state = GPRS_IDLE;

	pinMode(GPRS_SDA,INPUT_PULLUP);
	pinMode(GPRS_SCL,INPUT_PULLUP);
}

int deal_data()
{
 	byte checksum = 0;
	checksum = checksum + ((single_rx_data>>4) & 0x0f);
	checksum = checksum + ((single_rx_data>>8) & 0x0f);
	checksum = checksum + ((single_rx_data>>12)&0x0f);
	checksum = checksum + ((single_rx_data>>16)&0x0f);
	checksum = checksum + ((single_rx_data>>20)&0x0f);
	checksum = checksum &0x0f;
	byte tmp = 0;int i;
	for(i = 0;i<4;i++)
	{
		bitWrite(tmp,i,bitread(checksum,3-i));
	}
	checksum = tmp;
	if(checksum != (single_rx_data&0x0f))
	{
		return 0;
	}
	gprs_rx_retry = ((single_rx_data & 0x1000000)==0)? 0:1;
	return 1;
	
}
void loop(void)
{
	update_time_state();
	switch(curr_state)
	{
		case GPRS_IDLE:
			if(digitalRead(GPRS_SDA) == LOW)
			{
				pinMode(GPRS_SCL,OUTPUT);
				digitalWrite(GPRS_SCL,LOW);
				curr_state = GPRS_RE_WAIT_SDA_HIGH;		
				limit_time = 5*GPRS_SIGN_WIDTH;				
			}
			
			if(send_data_index > 0 || retry_cnt >0)
			{
			 	prepare_data();
				pinMode(GPRS_SDA,OUTPUT);
				digitalWrite(GPRS_SDA,LOW);
				curr_state = GPRS_WAIT_SCL_TO_LOW;
				limit_time = GPRS_SIGN_WIDTH * 5;
			}
			
			break;

		case GPRS_WAIT_SCL_TO_LOW:
			pinMode(GPRS_SCL,INPUT_PULLUP);
			if(digitalRead(GPRS_SCL) == LOW)
			{
				if(update_gap_time > GPRS_SIGN_WIDTH)
				{
					update_gap_time = 0;
					digitalWrite(GPRS_SDA,HIGH);
					limit_time = GPRS_SIGN_WIDTH * 3;
					curr_state = GPRS_WAIT_SCL_TO_HIGH;
				}
			}
			if(limit_time == 0)
			{
				send_abort();
			}
			break;

		case GPRS_WAIT_SCL_TO_HIGH:
			if(digitalRead(GPRS_SCL) == HIGH)
			{
				update_gap_time = 0;
				curr_state = SEND_REPEAT_SIGN;
			}
			if(limit_time == 0)
			{
				send_abort();
			}
			break;
		
		case SEND_REPEAT_SIGN:	
			if(update_gap_time > GPRS_SIGN_WIDTH)
			{
				update_gap_time = 0;
				digitalWrite(GPRS_SCL,HIGH);
				pinMode(GPRS_SCL,OUTPUT);
				
				digitalWrite(GPRS_SCL,LOW);
				digitalWrite(GPRS_SDA,gprs_resign_data);
				
				curr_state = SEND_DATA_SCL_HIGH;
			}
			break;

		case SEND_DATA_SCL_HIGH:
			if(update_gap_time > GPRS_SIGN_WIDTH)
			{
				update_gap_time = 0;
				digitalWrite(GPRS_SCL,HIGH);
				curr_state = SEND_DATA_SCL_LOW;
			}
			break;

		case SEND_DATA_SCL_LOW:
			if(update_gap_time > GPRS_SIGN_WIDTH)
			{
				digitalWrite(GPRS_SCL,LOW);
				if(31 - single_data_index > 7)
				{
					if(bitRead(single_tx_data,31 - single_data_index) == 1)
					{
						digitalWrite(GPRS_SDA,HIGH);
					}
					else
					{
						digitalWrite(GPRS_SDA,LOW);
					}
					single_data_index++;
					curr_state = SEND_DATA_SCL_HIGH;
				}
				else
				{
					pinMode(GPRS_SDA,INPUT_PULLUP);
					curr_state = WAIT_SDA_ACK_LOW;
					limit_time = 2*GPRS_SIGN_WIDTH;
				}
			}
			break;
		case WAIT_SDA_ACK_LOW:
			if(digitalRead(GPRS_SDA) == LOW)
			{
				curr_state = WAIT_SDA_ACK_HIGH;
				limit_time = 5*GPRS_SIGN_WIDTH;
			}
			
			if(limit_time == 0)
			{
				send_abort();				
			}
			break;
		case WAIT_SDA_ACK_HIGH:
			if(digitalRead(GPRS_SDA) == HIGH)
			{
				curr_state = GPRS_IDLE;
				limit_time = 10*GPRS_SIGN_WIDTH;
				gprs_resign_data = (gprs_resign_data + 1)%2;
				retry_cnt = 0;
				pinMode(GPRS_SDA,INPUT_PULLUP);
				pinMode(GPRS_SCL,INPUT_PULLUP);
			}
			if(limit_time == 0)
			{
				send_abort();
			}
			break;
		case GPRS_RE_WAIT_SDA_HIGH:
			if(digitalRead(GPRS_SDA) == HIGH)
			{
				digitalWrite(GPRS_SCL,HIGH);
				curr_state = GPRS_RE_WAIT_SCL_LOW;
				limit_time = 5*GPRS_SIGN_WIDTH;
				pinMode(GPRS_SCL,INPUT_PULUP);
				single_rx_data = 0;
				single_data_re_index = 0;
			}
			if(limit_time == 0)
			{
				receive_abort();
			}
			break;
	/*	case GPRS_RE_WAIT_RETRY_SCL_LOW:
			if(digitalRead(GPRS_SCL) == LOW)
			{
				curr_state = GPRS_RE_WAIT_RETRY_SCL_HIGH;
				limit_time = 5*GPRS_SIGN_WIDTH;
			}
			if(limit_time == 0)
			{
				receive_abort();
			}
			break;
		case GPRS_RE_WAIT_RETRY_SCL_HIGH:
			if(digitalRead(GPRS_SCL) == HIGH)
			{
				if(digitalRead(GPRS_SDA) == HIGH)
				{
					gprs_receive_resign_data = 1;
				}
				else
				{
					gprs_receive_resign_data = 0;
				}
				curr_state = GPRS_RE_WAIT_SCL_LOW;
			}
			if(limit_time == 0)
			{
				curr_state = GPRS_IDLE;
			}
			break;*/
		case GPRS_RE_WAIT_SCL_LOW:
			if(digitalRead(GPRS_SCL) == LOW)
			{
				curr_state = GPRS_RE_WAIT_SCL_HIGH;
				limit_time = 5*GPRS_SIGN_WIDTH;
			}
			if(limit_time == 0)
			{
				curr_state = GPRS_IDLE;
			}
			break;
		case  GPRS_RE_WAIT_SCL_HIGH:
			if(digitalRead(GPRS_SCL) == HIGH)
			{
				//single_rx_data = (single_rx_data<<1) + digitalRead(GPRS_SCL);
				if(digitalRead(GPRS_SDA) == HIGH)
				{
					bitwrite(single_rx_data,24 - single_data_re_index,1);
				}
				else
				{
					bitWrite(single_rx_data,24 - single_data_re_index,0);
				}
				single_data_re_index ++;
				
				if(single_data_re_index>=25)	
				{
					if(deal_data())
					{
						curr_state = GPRS_RE_SEND_ACK_LOW;
					}
					else 
					{
						receive_abort();
						curr_state = GPRS_IDLE;
					}
				}
				else
				{
					limit_time = 5*GPRS_SIGN_WIDTH;
					curr_state = GPRS_RE_WAIT_SCL_LOW;
			    	}
			}
			if(limit_time == 0)
			{
				receive_abort();
			}
			break;
		case GPRS_RE_SEND_ACK_LOW:
		{
			digitalWrite(GPRS_SDA,HIGH);
			pinMode(GPRS_SDA,INPUT_PULLUP);
			curr_state = GPRS_RE_SEND_ACK_HIGH;
			break;
		case GPRS_RE_SEND_ACK_HIGH:
		{
			if(update_gap_time > GPRS_SIGN_WIDTH)
			{
				digitalWrite(GPRS_SDA,HIGH);
				curr_state = GPRS_IDLE;
			}
			break;
		}
	}
	if(last_state != curr_state)
	{
		last_state = curr_state;
		update_gap_time = 0;
	}
}
