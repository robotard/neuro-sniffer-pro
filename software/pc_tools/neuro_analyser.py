#!/usr/bin/env python3
"""Neuro-Sniffer Pro PC analyser. Usage: python3 neuro_analyser.py --port /dev/ttyACM0"""
import argparse,serial,threading,queue

class Neuro:
    def __init__(self,port,baud=115200):
        self.s=serial.Serial(port,baud,timeout=1);self.q=queue.Queue()
        threading.Thread(target=self._rx,daemon=True).start()
        print(f"Connected: {port}")
    def _rx(self):
        while True:
            try:
                l=self.s.readline().decode('utf-8',errors='replace').strip()
                if l:self.q.put(l)
            except:pass
    def send(self,cmd):self.s.write((cmd+'\n').encode())
    def recv(self,t=3):
        try:return self.q.get(timeout=t)
        except:return None
    def cli(self):
        print("Neuro CLI. 'quit' to exit.")
        while True:
            try:c=input('neuro> ').strip()
            except(EOFError,KeyboardInterrupt):break
            if c in('quit','exit'):break
            self.send(c);r=self.recv(3)
            if r:print(f"  << {r}")

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--port',required=True);a=p.parse_args()
    Neuro(a.port).cli()
