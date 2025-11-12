all: ioman passenger

ioman:
	cd ioman && ${MAKE}

passenger: 
	cd passenger && ${MAKE}

.PHONY: clean ioman passenger
clean:
	cd ioman && ${MAKE} clean
	cd passenger && ${MAKE} clean
