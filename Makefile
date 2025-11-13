all: ioman passenger dispatcher boat

ioman:
	cd ioman && ${MAKE}

passenger: 
	cd passenger && ${MAKE}

dispatcher:
	cd dispatcher && ${MAKE}

boat:
	cd boat && ${MAKE}

.PHONY: clean ioman passenger dispatcher boat
clean:
	cd ioman && ${MAKE} clean
	cd passenger && ${MAKE} clean
	cd dispatcher && ${MAKE} clean
	cd boat && ${MAKE} clean

