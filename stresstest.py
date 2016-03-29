import subprocess
import random
import threading

def worker():
     subprocess.call(["Build_Client/Client", "-p", "2000", "-a", "127.0.0.1", "-n", "test" + `random.randint(0, 1000000)`])   

random.seed()
threads = [threading.Thread(target = worker) for _i in range(100)]
for thread in threads:
    thread.start()

