all: container

container:
	docker build -t osmo-freeswitch-pbx:latest .

run:
	docker run --rm -it --name=osmo-freeswitch-pbx \
		-p 5060:5060/udp -p 6000-6020:6000-6020/udp \
		osmo-freeswitch-pbx:latest

stop:
	docker rm -f osmo-freeswitch-pbx
