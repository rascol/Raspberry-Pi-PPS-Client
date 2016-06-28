
RM := rm -rf

# All of the sources participating in the build are defined here
-include subdir.mk

# All Target
all: pps-client-stop

# Tool invocations
pps-client-stop: $(OBJS) $(USER_OBJS)
	@echo 'Building target: $@'
	@echo 'Invoking: G++ Linker'
	$(CROSS_COMPILE)g++ -o "pps-client-stop" $(OBJS) $(USER_OBJS) $(LIBS)
	@echo 'Finished building target: $@'
	@echo ' '

# Other Targets
clean:
	-$(RM) $(OBJS) $(CPP_DEPS) $(EXECUTABLES) pps-client-stop
	-@echo ' '

.PHONY: all clean dependents
.SECONDARY:
