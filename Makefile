all: ioman passenger dispatcher

ioman:
	cd ioman && ${MAKE}

passenger: 
	cd passenger && ${MAKE}

dispatcher:
	cd dispatcher && ${MAKE}

.PHONY: clean ioman passenger dispatcher
clean:
	cd ioman && ${MAKE} clean
	cd passenger && ${MAKE} clean
	cd dispatcher && ${MAKE} clean

