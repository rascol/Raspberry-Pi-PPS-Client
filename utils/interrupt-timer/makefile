
RM := rm -rf

# All of the sources participating in the build are defined here
-include subdir.mk

# All Target
all: interrupt-timer

# Tool invocations
interrupt-timer: $(OBJS) $(USER_OBJS)
	@echo 'Building target: $@'
	@echo 'Invoking: G++ Linker'
	g++ -o "interrupt-timer" $(OBJS) $(USER_OBJS) $(LIBS)
	@echo 'Finished building target: $@'
	@echo ' '

# Other Targets
clean:
	-$(RM) $(OBJS) $(CPP_DEPS) $(EXECUTABLES) interrupt-timer
	-@echo ' '

.PHONY: all clean dependents
.SECONDARY:
