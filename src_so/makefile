TARGET=libSoAVLTree.so
OBJS=$(patsubst %.c, %.o, $(wildcard ./*.c))

$(TARGET):$(OBJS)
	@$(CC) -shared -fPIC $^ -o $@
# 	发布
	cp $(TARGET) /usr/lib/

# 模式匹配
%.o:%.c
	@$(CC) -c $^ -o $@

clean:
	@$(RM) $(OBJS) $(TARGET)
	@$(RM) /usr/lib/$(TARGET)
