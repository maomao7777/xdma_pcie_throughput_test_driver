

MODULE_NAME = my_test


##################
# Source
##################

SRC += src/main.c
SRC += src/phy_driver.c
SRC += src/phy_driver_test.c
SRC += src/pcie_xdma.c


INC += -I./inc

EXTRA_CFLAGS += $(INC)


##################
# Kernel Module
##################

KBUILD_EXTRA_SYMBOLS := $(OSW_BIN_DIR)/module/Module.symvers
KBUILD_EXTRA_SYMBOLS += $(MODULES_BIN_DIR)/ksocket/Module.symvers
KBUILD_EXTRA_SYMBOLS += $(MODULES_BIN_DIR)/param/Module.symvers
KBUILD_EXTRA_SYMBOLS += $(MODULES_DIR)/ukcomm/Module.symvers
KBUILD_EXTRA_SYMBOLS += $(UCI_K_DIR)/Module.symvers

ifneq ($(KERNELRELEASE),)

OBJS  := $(SRC:.c=.o)
$(MODULE_NAME)-objs := $(OBJS)

obj-m := $(MODULE_NAME).o

else

all:
	@echo "================================================================================"
	@echo "MODULE_NAME = $(MODULE_NAME)"
	@echo "KERN_DIR    = $(KERNEL_BIN_DIR)"
	@echo "CC          = $(CROSS)gcc"
	@echo "BUILD_ROOT  = $(BUILD_ROOT)"
	@echo "================================================================================"
	$(MAKE) -C $(KERNEL_BIN_DIR) M=$(BUILD_ROOT) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS) V=$(V) modules
	@$(STRIP) --strip-debug $(MODULE_NAME).ko

config:
	

clean:
	$(MAKE) -C $(KERNEL_BIN_DIR) M=$(BUILD_ROOT) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS) V=$(V) clean
	@$(RM) Modules.symvers

distclean: clean


endif

.PHONY: config clean distclean
