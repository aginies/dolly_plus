CXXFLAGS = -g
HEADERS = Buffer.h Disk.h Net.h Server.h buffer.h configfile.h packet.h \
Config.h List.h Packet.h StopWatch.h common.h net.h
#all: $BINDIR/server $BINDIR/client
#	touch all
all : dollyS dollyC dping
	touch all

clean : 
	rm *.o
	rm dollyS dollyC dping

dollyS : Server.o Config.o Net.o List.o Packet.o Buffer.o Disk.o ServThread.o StopWatch.o
	g++ -o dollyS Server.o Config.o Net.o List.o Packet.o Buffer.o Disk.o ServThread.o StopWatch.o -lpthread

dollyS_dy : Server.o Config.o Net.o List.o Packet.o Buffer.o Disk.o ServThread.o StopWatch.o
	g++ -o dollyS.dy Server.o Config.o Net.o List.o Packet.o Buffer.o Disk.o ServThread.o StopWatch.o -lpthread

dollyC : Client.o ClntThread.o Net.o Packet.o Buffer.o Disk.o StopWatch.o 
	g++ -o dollyC Client.o ClntThread.o Net.o Packet.o Buffer.o Disk.o StopWatch.o -lpthread

dollyC_dm : Client_dummy.o ClntThread.o Net.o Packet.o Buffer.o Disk.o StopWatch.o 
	sed -e's/^DataOut \*find_next_and_newnet/DataOut *find_next_and_newnet(PacketIte *ite); DataOut *NO_MEANINGNAME/' Client.cpp > temp.cpp
	g++ -o dollyC.dm temp.cpp Client_dummy.cpp ClntThread.o Net.o Packet.o Buffer.o Disk.o StopWatch.o -lpthread


dping : Ping.o Config.o Net.o Packet.o List.o Buffer.o StopWatch.o
	g++ -o dping Ping.o Config.o Net.o Packet.o List.o Buffer.o StopWatch.o -lpthread

ClntThread.o : ClntThread.cpp ${HEADERS}

ServThread.o : ServThread.cpp ${HEADERS}

Client.o : Client.cpp net.h Net.h Packet.h Disk.h

Server.o : Server.cpp common.h Config.h List.h net.h Net.h Packet.h

Ping.o : Ping.cpp common.h Config.h net.h Net.h Packet.h List.h

List.o : List.cpp List.h

Packet.o : Packet.cpp Packet.h packet.h

Net.o : Net.cpp Net.h net.h common.h

Config.o: Config.cpp Config.h configfile.h packet.h

Buffer.o : buffer.h Buffer.h Buffer.cpp 

Disk.o : Disk.cpp Disk.h packet.h Buffer.h

StopWatch.o : StopWatch.h StopWatch.cpp

selsend.o : selsend.cpp

select.o : select.cpp

linstall : dollyS dollyC
	cp dollyS /distr/RedHat/instimage/dolly/bin
#	strip dollyC
	cp dollyC /distr/RedHat/instimage/dolly/bin
	ls -l dollyC
	scp dollyC dollyS dcpcf002:/distr71/dolly/bin

