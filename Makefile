all: ioman passenger dispatcher boat controller

ioman:
	cd ioman && ${MAKE}

passenger: 
	cd passenger && ${MAKE}

dispatcher:
	cd dispatcher && ${MAKE}

boat:
	cd boat && ${MAKE}

controller:
	cd controller && ${MAKE}

.PHONY: clean ioman passenger dispatcher boat controller
clean:
	cd ioman && ${MAKE} clean
	cd passenger && ${MAKE} clean
	cd dispatcher && ${MAKE} clean
	cd boat && ${MAKE} clean
	cd controller && ${MAKE} clean

