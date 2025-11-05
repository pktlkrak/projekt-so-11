all: ioman

ioman:
	cd ioman && ${MAKE}

.PHONY: clean ioman
clean:
	cd ioman && ${MAKE} clean
