all:	sender receiver
sender:	sender.c wrapper.h
	gcc -o sender sender.c -lpthread
	
receiver:	receiver.c wrapper.h
	gcc -o receiver receiver.c -lpthread
	
clean:
	rm sender receiver
