CXX = clang++
CXXFLAGS += -std=c++14
LDFLAGS += -L/usr/local/lib -I/usr/local/include `pkg-config --libs protobuf librabbitmq libSimpleAmqpClient` \
					-Wl,--no-as-needed -Wl,--as-needed -ldl -lboost_system -lboost_chrono -lboost_program_options \
					-lopencv_imgproc -lopencv_core -lopencv_calib3d -lopencv_imgcodecs -lopencv_highgui -lopencv_aruco \
					-lboost_filesystem -lismsgs
PROTOC = protoc
LOCAL_PROTOS_PATH = ./msgs/

vpath %.proto $(LOCAL_PROTOS_PATH)

MAINTAINER = viros
SERVICE = aruco
VERSION = 1
LOCAL_REGISTRY = git.is:5000

all: debug

debug: CXXFLAGS += -g 
debug: LDFLAGS += -fsanitize=address -fno-omit-frame-pointer
debug: service test

release: CXXFLAGS += -Wall -Werror -O2
release: service test

test: test.o 
	$(CXX) $^ $(LDFLAGS) -o $@

service: service.o 
	$(CXX) $^ $(LDFLAGS) -o $@

.PRECIOUS: %.pb.cc
%.pb.cc: %.proto
	$(PROTOC) -I $(LOCAL_PROTOS_PATH) --cpp_out=. $<

clean:
	rm -f *.o *.pb.cc *.pb.h service test

docker: 
	docker build -t $(MAINTAINER)/$(SERVICE):$(VERSION) --build-arg=SERVICE=$(SERVICE) .

push_local: docker
	docker tag $(MAINTAINER)/$(SERVICE):$(VERSION) $(LOCAL_REGISTRY)/$(SERVICE):$(VERSION)
	docker push $(LOCAL_REGISTRY)/$(SERVICE):$(VERSION)

push_cloud: docker
	docker push $(MAINTAINER)/$(SERVICE):$(VERSION)