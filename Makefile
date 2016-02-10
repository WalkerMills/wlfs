obj-m := wlfs.o
wlfs-objs := init.o super.o util.o

KDIR := /lib/modules/$(shell uname -r)
M4_CONFIG := config.m4
TEST_DIR := test

.PHONY: all clean test

all: ko mkfs-wlfs

clean:
	$(MAKE) -C $(KDIR)/build M=$(PWD) clean
	$(RM) mkfs-wlfs $(TEST_DIR)/*.o $(TEST_DIR)/test_mkfs

test: all $(M4_CONFIG) test_mkfs
	m4 $(M4_CONFIG) test.sh | bash

ko:
	$(MAKE) -C $(KDIR)/build M=$(PWD) modules

# mkfs-wlfs.o: private CFLAGS = -DNDEBUG
util-user.o: util.c
	$(CC) $(CFLAGS) -c -o $@ $<
mkfs-wlfs: util-user.o

test_%: $(TEST_DIR)/test_%.c $(M4_CONFIG)
	m4 $(M4_CONFIG) $< | \
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -lgtest -x c++ -Wall -o $(TEST_DIR)/$@ -
