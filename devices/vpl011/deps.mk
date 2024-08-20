LIBS = vbus irq_controller vuart cpu_model
ifneq ($(HOSTED), 1)
LIBS += $(PLATFORM)
endif
