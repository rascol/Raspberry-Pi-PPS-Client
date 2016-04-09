
# Add inputs and outputs from these tool invocations to the build variables 
CPP_SRCS += \
./interrupt-timer.cpp 

OBJS += \
./interrupt-timer.o

CPP_DEPS += \
./interrupt-timer.d

# Each subdirectory must supply rules for building sources it contributes
%.o: ./%.cpp
	@echo 'Building file: $<'
	@echo 'Invoking: G++ Compiler'
	g++ -O3 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '
