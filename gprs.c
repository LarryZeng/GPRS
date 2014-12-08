#define GPRS_SIGN_WIDTH 1655  //������Ԫ���
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
 *Ҫ�����ĸ�cid���ݣ����Ҫ��һ������洢�⼸��cid����
 *
 */
int parameter_list[SEND_DATA_MAX];
void send_data(void)
{
	/*׼�����ݿ���λ������λ */
	parameter_list[0] = parameter_list[0] & 0xff & 0x01;
	parameter_list[1] = parameter_list[1] & 0xff & 0x02;
	parameter_list[2] = parameter_list[0] & 0xff & 0x04;
	parameter_list[3] = parameter_list[1] & 0xff & 0x08;
	curr_data_size = 4;
}


int GPRS_SDA = 2;
int GPRS_SCL = 3;
int curr_state = GPRS_IDLE; 
ulong last_record_time = 0;  //��һ�μ�¼��ʱ��
ulong update_gap_time = 0;  //�ۼ�ʱ��
ulong limit_time = 0;       //ʣ������ʱ��
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
