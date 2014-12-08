#define GPRS_SIGN_WIDTH 1655  //定义码元宽度
#define SEND_DATA_MAX 10
#define FAIL_MAX 4
#define ulong unsigned long

enum gprs_state
{
	GPRS_IDLE,
	READY_SEND_DATA,
	READY_RECEIVE_DATA,
	SEND_RESEND_SIGN,
	
}

/*
 *要发送四个cid数据，因此要用一个数组存储这几个cid数据
 *
 */
int parameter_list[SEND_DATA_MAX];
void send_data(void)
{
	/*准备数据控制位和数据位 */
	parameter_list[0] = parameter_list[0] & 0xff & 0x01;
	parameter_list[1] = parameter_list[1] & 0xff & 0x02;
	parameter_list[2] = parameter_list[0] & 0xff & 0x04;
	parameter_list[3] = parameter_list[1] & 0xff & 0x08;
	curr_data_size = 4;
}


int GPRS_SDA = 2;
int GPRS_SCL = 3;
int curr_state = GPRS_IDLE; 
ulong last_record_time = 0;  //上一次记录的时间
ulong update_gap_time = 0;  //累计时间
ulong limit_time = 0;       //剩余限制时间
void update_time_state(void)
{
	ulong gap_time;
	ulong curr_time = micros();
	gap_time = curr_time - last_record_time;

	if((curr_time - last_record_time) < 0 )
	{
		gap_time = ulong(-1) - last_record_time + curr_time;
	}
	
	if(limit_time > 0)
	{
		if(gap_time < limit_time)
		{
			limit_time = limit_time - gap_time;
		}
	}
	else
	{
		limit_time = 0;
	}

	last_record_time = curr_time;
	update_gap_time = update_gap_time + gap_time;
}

void setup(void)
{
	
}

void loop(void)
{
	update_time_state();
	switch(curr_state)
	{
		GPRS_IDLE:
			pinMode(GPRS_SDA,INPUT_PULLUP);
			if(digitalRead(GPRS_SDA) == LOW)
			{
				curr_state = READY_RECEIVE_DATA;
				break;
			}
				
			if(curr_data_size > 0)
			{
				sendData();
				curr_state = READY_SEND_DATA;
				break;
			}
			
					
	}
}
