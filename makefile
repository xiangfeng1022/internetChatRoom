TARGET1=server
TARGET2=client
OBJ1=chatRoomServer.o
OBJ2=chatRoomClient.o

LDFALGS=-L./src_so -L./src_so1
LIBS=-lSoAVLTree -lSoThreadPool

SO_DIR=./src_so 

# 使用$(TARGET) 必须要加 '$' 符号
$(TARGET1):$(OBJ1)
	@$(CC) $^ $(LDFALGS) $(LIBS) -o $@ -lsqlite3

$(TARGET2):$(OBJ2)
	@$(CC) $^ $(LDFALGS) $(LIBS) -o $@ -lsqlite3

%.o:%.c
	@$(CC) -c $^ -o $@

# 伪文件 / 伪目标
.PHONY:	clean

# 清除编译出来的依赖文件 和 二进制文件
clean:
	@$(RM) *.o $(TARGET1) $(TARGET2)

all:$(TARGET1) $(TARGET2)