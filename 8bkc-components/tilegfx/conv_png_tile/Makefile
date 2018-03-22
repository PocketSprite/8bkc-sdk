TARGET:=conv_png_tile
OBJ:=main.o
CFLAGS:=-ggdb $(shell xml2-config --cflags)
LDFLAGS:=-lm $(shell xml2-config --libs) -lgd

$(TARGET): $(OBJ)
	$(CC) -o $@ $(OBJ) $(LDFLAGS)

test: $(TARGET)
	./$(TARGET) test/test.c test/test.h test/level1.tmx

clean:
	rm -f $(OBJ)

.PHONY: clean test