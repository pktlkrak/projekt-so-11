import json
import sys

P = None
snippets = [
    ("ioman/main.c",                "logFD = creat"),
    (P,                             "logFD = open"),
    ("ioman/ioman_client.c",        "read(_takeoverReadEndPipe"),
    (P,                             "write(_socket, name"),
    (P,                             "close(_stderrPipe);"),
    ("ioman/main.c",                "unlink"),
    ("controller/main.c",           "#define F_EXEC"),
    (P,                             "#define WAIT_FOR_COMPLETION"),
    ("commons/include/messages.h",  "exit("),
    ("dispatcher/main.cpp",         "SIG_PLACE_ON_BRIDGE"),
    ("passenger/signal_helpers.c",  "sigwait"),
    ("boat/main.c",                 "raise(SIGTERM)"),
    (P,                             "SIG_BOAT_TERMINATE"),
    ("ioman/ioman_client.c",        "pipe2(_pipe, O_NONBLOCK);"),
    (P,                             "dup("),
    (P,                             "dup2("),
    ("ioman/ioman_client.c",        "pthread_join"),
    ("dispatcher/main.cpp",         "pthread_create"),
    (P,                             "define BRIDGE_LOCK"),
    (P,                             "define BRIDGE_UNLOCK"),
    (P,                             "pthread_cond_wait"),
    (P,                             "define BRIDGE_CHANGED"),
    ("controller/main.c",           "pthread_detach"),
    ("boat/main.c",                 "shmget"),
    (P,                             "shmat"),
    (P,                             "shmdt"),
    (P,                             "RMID"),

    ("commons/include/messages.h",  "MSGQUEUE_RECV_C_DIRECT"),
    ("dispatcher/main.cpp",         "MSGQUEUE_RECV_GLOBAL("),
    (P,                             "MSGQUEUE_SEND(&msgEvict"),
    ("passenger/main.c",            "MSGQUEUE_RECV_C_DIRECT(&placeOnBoatMessage"),

    ("ioman/main.c",                "socket("),
    (P,                             "bind("),
    (P,                             "listen("),
    (P,                             "accept("),
    ("ioman/ioman_client.c",        "connect("),
]

outputs = []

last_fname = None
for fname, line in snippets:
    if fname is P:
        fname = last_fname
    else:
        last_fname = fname
    with open(fname, 'r') as e:
        lines = e.readlines()
        for i, x in enumerate(lines):
            if line in x:
                outputs.append([fname, f"https://github.com/pktlkrak/projekt-so-11/blob/master/{fname}#L{i+1}"])
                break
        else:
            print(f"Error - line {line} not found in {fname}")
            sys.exit(-1)

with open("docs/codelinks.json", 'w') as e: json.dump(outputs, e)