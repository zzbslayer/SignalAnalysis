#include <stdio.h>
#include <stdint.h>
#include "NIDAQmx.h"
#pragma comment(lib,"NIDAQmx.lib")
#include<iostream>
#include<string>
#include<sstream>

#include <thread>                // std::thread
#include <mutex>                // std::mutex, std::unique_lock
#include <condition_variable>    // std::condition_variable
#include<Windows.h>
using namespace std;

float64     data1[5000];
float64     data2[5000];


std::mutex mtx; // 全局互斥锁.
std::condition_variable cv; // 全局条件变量.
bool ready = false; // 全局标志位.
std::unique_lock <std::mutex> lck(mtx);


#define DAQmxErrChk(functionCall) if( DAQmxFailed(error=(functionCall)) ) goto Error; else
#define N 12
#define M 20

int32 CVICALLBACK EveryNCallback(TaskHandle taskHandle, int32 everyNsamplesEventType, uInt32 nSamples, void *callbackData);
int32 CVICALLBACK EveryNCallback2(TaskHandle taskHandle, int32 everyNsamplesEventType, uInt32 nSamples, void *callbackData);
int32 CVICALLBACK DoneCallback(TaskHandle taskHandle, int32 status, void *callbackData);

float64 period(float64* signal, float64 voltage, int size) {
	float64		dddd = 0.001;
	float64 last = signal[0];
	float64 start = -1;
	int cnt = 0;
	float64 end = -1;
	for (int i = 0; i < size; i++) {
		float64 cur = signal[i];
		float64 curTime = dddd * i;
		printf("<Period> cur: %f, curTime: %f\n", cur, curTime);
		if (abs(cur - last) > voltage) {
			printf("<Period> Trigger Time: %f\n", curTime);
			if (start == -1) {
				start = curTime;
			}
			else if (cnt < 1) {
				cnt++;
			}
			else if (end == -1) {
				end = curTime;
				break;
			}
		}
		last = cur;
	}
	return end - start;
}

int main(void)
{
	int32       error = 0;
	TaskHandle  TRItaskHandle = 0;
	int32       read;
	float64		triggerThreshold = 5;
	char        errBuff[2048] = { '\0' };
	char        Buff1[2048] = { '\0' };
	string channel = "Dev1/ai4";


	TaskHandle  ACCtaskHandle = 0;

	DAQmxErrChk(DAQmxCreateTask("", &TRItaskHandle));//创建读取任务
	DAQmxErrChk(DAQmxCreateAIVoltageChan(TRItaskHandle, channel.c_str(), "", DAQmx_Val_Cfg_Default, -10.0, 10.0, DAQmx_Val_Volts, NULL));//第二步：创建电压输入通道，其中+-10代表输入模拟信号的幅值范围。
	DAQmxErrChk(DAQmxCfgSampClkTiming(TRItaskHandle, "", 10000.0, DAQmx_Val_Rising, DAQmx_Val_FiniteSamps, 6000));//采样频率10000,采样6000个点
	DAQmxErrChk(DAQmxSetAITermCfg(TRItaskHandle, channel.c_str(), DAQmx_Val_PseudoDiff));//设置终端的配置为伪微分PseudoDiff（我也不知道为什么是伪微分，但不是伪微分会报错。
	DAQmxErrChk(DAQmxRegisterEveryNSamplesEvent(TRItaskHandle, DAQmx_Val_Acquired_Into_Buffer, 6000, 0, EveryNCallback, NULL))//注册回调函数EveryNCallback去读取缓存
		DAQmxErrChk(DAQmxRegisterDoneEvent(TRItaskHandle, 0, DoneCallback, NULL));//注册回调函数DoneCallback返回错误
	printf("*--------*\n");
	DAQmxErrChk(DAQmxStartTask(TRItaskHandle));

	while (!ready)
		cv.wait(lck);

	DAQmxErrChk(DAQmxClearTask(TRItaskHandle));
	ready = false;

	channel = "Dev1/ai0";

	DAQmxErrChk(DAQmxCreateTask("", &TRItaskHandle));//创建读取任务
	DAQmxErrChk(DAQmxCreateAIVoltageChan(TRItaskHandle, channel.c_str(), "", DAQmx_Val_Cfg_Default, -10.0, 10.0, DAQmx_Val_Volts, NULL));//第二步：创建电压输入通道，其中+-10代表输入模拟信号的幅值范围。
	DAQmxErrChk(DAQmxCfgSampClkTiming(TRItaskHandle, "", 10000.0, DAQmx_Val_Rising, DAQmx_Val_FiniteSamps, 6000));//采样频率10000,采样6000个点
	DAQmxErrChk(DAQmxSetAITermCfg(TRItaskHandle, channel.c_str(), DAQmx_Val_PseudoDiff));//设置终端的配置为伪微分PseudoDiff（我也不知道为什么是伪微分，但不是伪微分会报错。
	DAQmxErrChk(DAQmxRegisterEveryNSamplesEvent(TRItaskHandle, DAQmx_Val_Acquired_Into_Buffer, 6000, 0, EveryNCallback2, NULL))//注册回调函数EveryNCallback去读取缓存
		DAQmxErrChk(DAQmxRegisterDoneEvent(TRItaskHandle, 0, DoneCallback, NULL));//注册回调函数DoneCallback返回错误
	printf("*--------*\n");
	DAQmxErrChk(DAQmxStartTask(TRItaskHandle));

	while (!ready)
		cv.wait(lck);
	DAQmxErrChk(DAQmxClearTask(TRItaskHandle));


	float64 T = period(data1, 10, 5000);  // Important
	printf("<Callback> Period: %f\n", T);

	float64		dddd = 0.001;
	int size = T / dddd;
	for (int i = 0; i < size; i++) {
		printf("<Main> data1[%d]: %f\n", i, data1[i]);
	}

	

Error:
	if (DAQmxFailed(error))
		DAQmxGetExtendedErrorInfo(errBuff, 2048);
	if (ACCtaskHandle != 0) {
		/*********************************************/
		// DAQmx Stop Code
		/*********************************************/
		DAQmxStopTask(ACCtaskHandle);
		DAQmxClearTask(ACCtaskHandle);
	}
	if (DAQmxFailed(error))
		printf("DAQmx Error: %s\n", errBuff);
	printf("End of program, press Enter key to quit\n");
	getchar();
	return 0;
}

int32 CVICALLBACK EveryNCallback(TaskHandle taskHandle, int32 everyNsamplesEventType, uInt32 nSamples, void *callbackData)
{
	int32       error = 0;
	char        errBuff[2048] = { '\0' };
	static int  totalRead = 0;
	int32       read = 0;
	float64		dddd = 0.001;
	float64     last = 0;

	printf("<Callback> Reading...\n");
	/*********************************************/
	// DAQmx Read Code
	/*********************************************/
	DAQmxErrChk(DAQmxReadAnalogF64(taskHandle, 1000, 10.0, DAQmx_Val_GroupByScanNumber, data1, 6000, &read, NULL));
	if (read>0) {
		string filename = "data1.csv";
		FILE *fp2 = fopen(filename.c_str(), "w+");
		for (int l = 0; l<3000; l++)
		{
			fprintf(fp2, "%f", data1[l]);
			fputc(',', fp2);
			fprintf(fp2, "%f", dddd*l);
			fputc(',\n', fp2);

		}
		fclose(fp2);
		string log = "<Callback> Write File " + filename + " Completed.\n";
		printf(log.c_str());


		std::unique_lock <std::mutex> lck(mtx);
		ready = true; // 设置全局标志位为 true.
		cv.notify_all(); // 唤醒所有线程.
	}

Error:
	if (DAQmxFailed(error)) {
		DAQmxGetExtendedErrorInfo(errBuff, 2048);
		/*********************************************/
		// DAQmx Stop Code
		/*********************************************/
		DAQmxStopTask(taskHandle);
		DAQmxClearTask(taskHandle);
		printf("DAQmx Error: %s\n", errBuff);
	}
	return 0;
}


int32 CVICALLBACK EveryNCallback2(TaskHandle taskHandle, int32 everyNsamplesEventType, uInt32 nSamples, void *callbackData)
{
	int32       error = 0;
	char        errBuff[2048] = { '\0' };
	static int  totalRead = 0;
	int32       read = 0;
	float64		dddd = 0.001;
	float64     last = 0;

	printf("<Callback> Reading...\n");
	/*********************************************/
	// DAQmx Read Code
	/*********************************************/
	DAQmxErrChk(DAQmxReadAnalogF64(taskHandle, 1000, 10.0, DAQmx_Val_GroupByScanNumber, data2, 6000, &read, NULL));
	if (read>0) {
		string filename = "data2.csv";
		FILE *fp2 = fopen(filename.c_str(), "w+");
		for (int l = 0; l<3000; l++)
		{
			fprintf(fp2, "%f", data2[l]);
			fputc(',', fp2);
			fprintf(fp2, "%f", dddd*l);
			fputc(',\n', fp2);

		}
		fclose(fp2);
		string log = "<Callback> Write File " + filename + " Completed.\n";
		printf(log.c_str());

		/*
		float64 T = period(data, 10);
		printf("<Callback> Period: %f\n", T);
		*/
		std::unique_lock <std::mutex> lck(mtx);
		ready = true; // 设置全局标志位为 true.
		cv.notify_all(); // 唤醒所有线程.
	}

Error:
	if (DAQmxFailed(error)) {
		DAQmxGetExtendedErrorInfo(errBuff, 2048);
		/*********************************************/
		// DAQmx Stop Code
		/*********************************************/
		DAQmxStopTask(taskHandle);
		DAQmxClearTask(taskHandle);
		printf("DAQmx Error: %s\n", errBuff);
	}
	return 0;
}
int32 CVICALLBACK DoneCallback(TaskHandle taskHandle, int32 status, void *callbackData)
{
	int32   error = 0;
	char    errBuff[2048] = { '\0' };

	// Check to see if an error stopped the task.
	DAQmxErrChk(status);

Error:
	if (DAQmxFailed(error)) {
		DAQmxGetExtendedErrorInfo(errBuff, 2048);
		DAQmxClearTask(taskHandle);
		printf("DAQmx Error: %s\n", errBuff);
	}
	return 0;
}
