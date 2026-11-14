#!/usr/bin/env python3
"""Passive channel charting triangulation — Neuro-Sniffer Pro v0.3.3.0
Usage: python3 channel_chart.py --csi-log u1.csi --csi-log u2.csi
CSI binary: 128B/frame (snap_t from ghost_tracker.c)
Requires: numpy umap-learn matplotlib scikit-learn"""
import argparse,struct,sys,numpy as np
SNAP_FMT='<q6BbB52b52bff'; SNAP_SIZE=struct.calcsize(SNAP_FMT)

def load(p):
    s=[]
    with open(p,'rb') as f:
        while True:
            r=f.read(SNAP_SIZE)
            if len(r)<SNAP_SIZE:break
            d=struct.unpack(SNAP_FMT,r)
            s.append({'mac':':'.join(f'{b:02X}'for b in d[1:7]),'amp':np.array(d[9:61],dtype=np.float32)})
    print(f"  {p}: {len(s)} frames");return s

def main():
    p=argparse.ArgumentParser();p.add_argument('--csi-log',action='append',required=True)
    p.add_argument('--output',default='channel_chart.png');a=p.parse_args()
    snaps=[];[snaps.extend(load(f)) for f in a.csi_log]
    if len(snaps)<10:print("Need >=10 frames");sys.exit(1)
    devs={};[devs.setdefault(s['mac'],[]).append(s['amp']) for s in snaps]
    macs=list(devs.keys())
    M=np.zeros((len(macs),52),dtype=np.float32)
    for i,mac in enumerate(macs):M[i]=np.stack(devs[mac]).mean(0)
    M=(M-M.mean(0))/(M.std(0)+1e-8)
    print(f"Devices: {len(macs)} | Frames: {len(snaps)}")
    try:
        import umap;emb=umap.UMAP(n_components=2,n_neighbors=min(15,len(macs)-1)).fit_transform(M);method='UMAP'
    except ImportError:
        from sklearn.decomposition import PCA;emb=PCA(n_components=2).fit_transform(M);method='PCA'
    try:
        import matplotlib.pyplot as plt
        fig,ax=plt.subplots(figsize=(10,8))
        ax.scatter(emb[:,0],emb[:,1],c=range(len(macs)),cmap='tab20',s=80,alpha=0.8)
        for i,mac in enumerate(macs):ax.annotate(mac[-8:],(emb[i,0],emb[i,1]),fontsize=7)
        ax.set_title(f'Neuro-Sniffer Pro Channel Chart ({method}) | {len(macs)} devices');ax.grid(True,alpha=0.3)
        plt.tight_layout();plt.savefig(a.output,dpi=150);print(f"Saved: {a.output}");plt.show()
    except ImportError:
        [print(f"  {mac} x={emb[i,0]:.3f} y={emb[i,1]:.3f}")for i,mac in enumerate(macs)]

if __name__=='__main__':main()
