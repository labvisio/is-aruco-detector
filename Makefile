CXX = clang++
CXXFLAGS += -std=c++14 -Wall -Werror -O2
DEBUGFLAGS = -g -fsanitize=address -fno-omit-frame-pointer
RELEASEFLAGS = -O2
BUILDFLAGS = $(DEBUGFLAGS)
LDFLAGS += -L/usr/local/lib -I/usr/local/include `pkg-config --libs protobuf librabbitmq libSimpleAmqpClient`\
           -Wl,--no-as-needed -Wl,--as-needed -ldl -lboost_system -lboost_chrono -lboost_program_options\
		   -lopencv_imgproc -lopencv_core -lopencv_calib3d -lopencv_imgcodecs -lopencv_highgui -lopencv_aruco\
					 -lboost_filesystem -lismsgs
PROTOC = protoc

LOCAL_PROTOS_PATH = ./msgs/

vpath %.proto $(LOCAL_PROTOS_PATH)

MAINTAINER = mendonca
SERVICE = aruco
VERSION = 1
LOCAL_REGISTRY = git.is:5000

all: service test

test: test.o 
	$(CXX) $(BUILDFLAGS) $^ $(LDFLAGS) -o $@

service: service.o 
	$(CXX) $(BUILDFLAGS) $^ $(LDFLAGS) -o $@

.PRECIOUS: %.pb.cc
%.pb.cc: %.proto
	$(PROTOC) -I $(LOCAL_PROTOS_PATH) --cpp_out=. $<

clean:
	rm -f *.o *.pb.cc *.pb.h service 

docker:
	rm -rf libs/
	mkdir libs/
	lddcp service libs/
	docker build -t $(MAINTAINER)/$(SERVICE):$(VERSION) .
	rm -rf libs/

push_local: docker
	docker tag $(MAINTAINER)/$(SERVICE):$(VERSION) $(LOCAL_REGISTRY)/$(SERVICE):$(VERSION)
	docker push $(LOCAL_REGISTRY)/$(SERVICE):$(VERSION)

push_cloud: docker
	docker push $(MAINTAINER)/$(SERVICE):$(VERSION)