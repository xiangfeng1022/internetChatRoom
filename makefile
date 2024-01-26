TARFET=server
TARGET=client
OBJ=balanceBinarySearchTree.o doubleLinkList.o doubleLinkListQueue.o threadPool.o chatRoomServer.o
OBJECT=balanceBinarySearchTree.o doubleLinkList.o doubleLinkListQueue.o threadPool.o chatRoomClient.o

$(TARFET):$(OBJ)
	gcc $^ -o $@ -lsqlite3

$(TARGET):$(OBJECT)
	gcc $^ -o $@ -lsqlite3

balanceBinarySearchTree.o:balanceBinarySearchTree.c
	gcc -c $^ -o $@

doubleLinkList.o:doubleLinkList.c
	gcc -c $^ -o $@

doubleLinkListQueue.o:doubleLinkListQueue.c
	gcc -c $^ -o $@

threadPool.o:threadPool.c
	gcc -c $^ -o $@

chatRoomServer.o:chatRoomServer.c
	gcc -c $^ -o $@

chatRoomClient.o:chatRoomClient.c
	gcc -c $^ -o $@

clean:
	@rm -rf *.o $(TARFET) $(TARGET)

all:server client