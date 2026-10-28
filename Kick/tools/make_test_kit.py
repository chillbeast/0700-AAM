"""Generate deterministic synthetic WAV fixtures for the engine tests."""
# SPDX-License-Identifier: AGPL-3.0-or-later
import math, random, struct, sys, wave, os
out = sys.argv[1]; os.makedirs(out, exist_ok=True); SR = 48000
def write(name, samples):
    with wave.open(os.path.join(out, name), 'wb') as w:
        w.setnchannels(2); w.setsampwidth(2); w.setframerate(SR)
        w.writeframes(b''.join(struct.pack('<hh', int(max(-1,min(1,s))*30000), int(max(-1,min(1,s))*30000)) for s in samples))
random.seed(1)
def kick(n=int(0.5*SR)):
    ph=0; r=[]
    for i in range(n):
        t=i/SR; f=50+120*math.exp(-t*30); ph+=2*math.pi*f/SR; r.append(math.sin(ph)*math.exp(-t*6))
    return r
def snare(n=int(0.3*SR)):
    return [ (0.6*random.uniform(-1,1)+0.4*math.sin(2*math.pi*190*i/SR))*math.exp(-i/SR*14) for i in range(n)]
def hat(n=int(0.12*SR)):
    p=0; r=[]
    for i in range(n):
        x=random.uniform(-1,1); r.append((x-p)*0.7*math.exp(-i/SR*40)); p=x
    return r
def clap(n=int(0.35*SR)):
    r=[]
    for i in range(n):
        t=i/SR; env=sum(math.exp(-(t-d)*60) for d in (0,0.01,0.02) if t>=d)/2 + 0.4*math.exp(-t*12)
        r.append(random.uniform(-1,1)*min(1,env)*0.8)
    return r
def tone(n=int(1.5*SR)):
    return [0.5*math.sin(2*math.pi*440*i/SR)*min(1,i/200)*math.exp(-i/SR*1.5) for i in range(n)]
for name,fn in [('Test Kick.wav',kick),('Test Snare.wav',snare),('Test Hat.wav',hat),('Test Clap.wav',clap),('Test Tone A440.wav',tone)]:
    write(name, fn())
print(sorted(os.listdir(out)))
