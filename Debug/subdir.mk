################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../PayloadIfDaemon.c \
../SerialMsgUtils.c \
../alt_functions.c \
../become_daemon.c \
../error_functions.c \
../get_num.c 

OBJS += \
./PayloadIfDaemon.o \
./SerialMsgUtils.o \
./alt_functions.o \
./become_daemon.o \
./error_functions.o \
./get_num.o 

C_DEPS += \
./PayloadIfDaemon.d \
./SerialMsgUtils.d \
./alt_functions.d \
./become_daemon.d \
./error_functions.d \
./get_num.d 


# Each subdirectory must supply rules for building sources it contributes
%.o: ../%.c
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C Compiler'
	arm-linux-gnueabihf-gcc -I/usr/arm-linux-gnueabihf/include/c++/4.6.3 -O0 -g3 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


