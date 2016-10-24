
RM := rm -rf

# All of the sources participating in the build are defined here
-include subdir.mk

# All Target
all: NormalDistribParams

# Tool invocations
NormalDistribParams: $(OBJS) $(USER_OBJS)
	@echo 'Building target: $@'
	@echo 'Invoking: G++ Linker'
	g++ -static -o "NormalDistribParams" $(OBJS) $(USER_OBJS) $(LIBS)
	@echo 'Finished building target: $@'
	@echo ' '

# Other Targets
clean:
	-$(RM) $(OBJS) $(CPP_DEPS) $(EXECUTABLES) NormalDistribParams
	-@echo ' '

.PHONY: all clean dependents
.SECONDARY:
