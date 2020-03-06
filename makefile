all: main ./www/upload
.PHONY:main ./www/upload
./www/upload:upload.cpp
	g++ -g -std=c++0x $^ -o $@ -lboost_system -lboost_filesystem
main:main.cpp
	g++ -g -std=c++0x $^ -o $@ -lpthread -lboost_system -lboost_filesystem
	
