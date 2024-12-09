
all: feed manager //vai correr o q esta a frente do all 

manager: manager.o
	gcc -o manager manager.o -pthread

manager.o: manager.c  
	gcc -c manager.c

feed: feed.o
	gcc -o feed feed.o 

feed.o: feed.c
	gcc -c feed.c  

clean:
	rm -f manager feed manager.o feed.o

clean-manager:
	rm -f manager manager.o 


clean-feed:
	rm -f feed feed.o

clean-apagaNP:
	rm -f MANAGER FEED*
