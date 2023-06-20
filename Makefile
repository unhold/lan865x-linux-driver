ifndef KDIR
    KDIR=/lib/modules/$(shell uname -r)/build
endif

obj-m := lan865x_t1s.o
lan865x_t1s-y := src/lan865x.o src/tc6.o

all:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
