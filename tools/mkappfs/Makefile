OBJS:=main.o appfs.o partemu/partemu.o partemu/crc32_le.o
TARGET:=mkappfs
APPFS_COMPONENT:=../../8bkc-components/appfs/
CFLAGS:=-I$(APPFS_COMPONENT)/include -Ipartemu -I. -ggdb

%.o: $(APPFS_COMPONENT)/%.c
	$(CC) -c -o $@ $(CFLAGS) $^

$(TARGET): $(OBJS)
	$(CC) -o $@ $(LDFLAGS) $(OBJS)

clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: clean
