#include <iostream>
#include <pthread.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <vector>
#include <string>


void* ReadThread(void* arg) {
	for (int i = 0; i < 8; ++i) {
		std::cout<<"  THREAD 1: AAA "<<(i + 1)<<std::endl;
	}

  return NULL;
}

void* WriteThread(void* arg) {
	for (int i = 0; i < 8; ++i) {
		std::cout<<"    THREAD 2: BBB "<<(i + 1)<<std::endl;
	}   
 
	return NULL;
}

void CreateThread() {
		std::cout<<"~~~Experiment~~~"<<std::endl;

    for (int i = 0; i < 1; ++i) {
        pthread_t tmp;
        pthread_create(&tmp, NULL, ReadThread, NULL);
    }

		sleep(1);

    for (int i = 0; i < 1; ++i) {
        pthread_t tmp;
				pthread_create(&tmp, NULL, WriteThread, NULL);
    }

    sleep(6);
		return;
}














